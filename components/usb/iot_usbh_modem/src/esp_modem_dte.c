/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_modem_dte.h"
#include "dte_helper.h"
#include "at_3gpp_ts_27_007.h"

static const char *TAG = "esp-modem-dte";

#define ESP_MODEM_START_BIT     BIT0
#define ESP_MODEM_COMMAND_BIT   BIT1
#define ESP_MODEM_STOP_PPP_BIT  BIT2
#define ESP_MODEM_STOP_BIT      BIT3

typedef enum {
    USB_MODEM_CDC_INTF_MODEM,
    USB_MODEM_CDC_INTF_AT,
    USB_MODEM_CDC_INTF_MAX_NUM,
} modem_cdc_intf_num_t;

#define PPP_BUFFER_SIZE 1540

typedef struct esp_modem_dte_s {
    iot_eth_driver_t eth_driver;
    iot_eth_mediator_t *mediator;
    esp_modem_dte_config_t config;
    modem_port_mode_t port_mode;          /*!< Modem mode */
    bool ppp_connected;                   /*!< PPP connection status */
    at_handle_t at_handle;                /*!< AT command parser handle */
    bool temporary_command_mode;          /*!< Temporary command mode flag */
    uint8_t ppp_buffer[PPP_BUFFER_SIZE];  /*!< PPP frame buffer */
    bool is_exiting_ppp;                  /*!< Flag to indicate if exiting PPP mode */

    int connect_status;                         /*!< DTE connection state, 0 if disconnect, 1 if connect */
    usbh_cdc_port_handle_t cdc_port[USB_MODEM_CDC_INTF_MAX_NUM];   /*!< USB CDC handle */
} esp_modem_dte_t;

static const usb_modem_id_t *modem_device_match(const usb_modem_id_t *list, const usb_device_desc_t *device_desc, const usb_config_desc_t *config_desc)
{
    const usb_modem_id_t *cur = list;
    while (cur->match_id.match_flags != 0) {
        if (0 == usbh_match_device(device_desc, &cur->match_id)) {
            goto _skip; // Device does not match, skip to next ID
        }
        if (0 == usbh_match_interface_in_configuration(config_desc, &cur->match_id, NULL)) {
            goto _skip; // Interface does not match, skip to next ID
        }
        return cur; // Match found
_skip:
        cur++;
    }
    return NULL;
}

static void _usbh_dte_notif_cb(usbh_cdc_port_handle_t cdc_port_handle, iot_cdc_notification_t *notif, void *arg)
{
    switch (notif->bNotificationCode) {
    case USB_CDC_NOTIFY_NETWORK_CONNECTION: {
        bool connected = notif->wValue;
        ESP_LOGI(TAG, "Notify - network connection changed: %s", connected ? "Connected" : "Disconnected");
    } break;
    case USB_CDC_NOTIFY_SPEED_CHANGE: {
        uint32_t *speeds = (uint32_t *)notif->Data;
        ESP_LOGI(TAG, "Notify - link speeds: %"PRIu32" kbps ↑, %"PRIu32" kbps ↓", (speeds[0]) / 1000, (speeds[1]) / 1000);
    } break;
    case USB_CDC_NOTIFY_SERIAL_STATE: {
        usb_cdc_serial_state_t *uart_state = (usb_cdc_serial_state_t *)notif->Data;
        ESP_LOGI(TAG, "Notify - serial state: intf(%d), dcd: %d, dsr: %d, break: %d, ring: %d, frame_err: %d, parity_err: %d, overrun: %d", notif->wIndex,
                 uart_state->dcd, uart_state->dsr, uart_state->break_det, uart_state->ring,
                 uart_state->framing_err, uart_state->parity_err, uart_state->overrun_err);
    } break;
    default:
        ESP_LOGW(TAG, "unexpected notification 0x%02x!", notif->bNotificationCode);
        return;
    }
}

static void _handle_ppp_data(usbh_cdc_port_handle_t cdc_port_handle, esp_modem_dte_t *dte)
{
    esp_err_t ret = ESP_OK;
    int rx_length = 0;
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, (size_t *)&rx_length);
    if (rx_length > 0) {
        if (rx_length > PPP_BUFFER_SIZE) {
            ESP_LOGE(TAG, "Received data size is too big, truncated to %d", PPP_BUFFER_SIZE);
            rx_length = PPP_BUFFER_SIZE;
        }
        uint8_t *buf = dte->ppp_buffer;
        ret = usbh_cdc_read_bytes(cdc_port_handle, buf, (size_t *)&rx_length, 0);
        ESP_RETURN_VOID_ON_ERROR(ret, TAG, "Failed to read data from USB CDC port");
        dte->mediator->stack_input(dte->mediator, buf, rx_length);
    }
}

static void _handle_at_data(usbh_cdc_port_handle_t cdc_port_handle, esp_modem_dte_t *dte)
{
    size_t length = 0;
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, &length);

    char *buffer;
    size_t buffer_remain;
    modem_at_get_response_buffer(dte->at_handle, &buffer, &buffer_remain);
    if (buffer_remain < length) {
        length = buffer_remain;
        ESP_LOGE(TAG, "data size is too big, truncated to %d", length);
    }
    usbh_cdc_read_bytes(cdc_port_handle, (uint8_t *)buffer, &length, 0);
    // Parse the AT command response
    modem_at_write_response_done(dte->at_handle, length);
}

static void _usbh_dte_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    esp_modem_dte_t *dte = (esp_modem_dte_t *)arg;

    if (dte->cdc_port[USB_MODEM_CDC_INTF_AT] == cdc_port_handle) {
        _handle_at_data(cdc_port_handle, dte);
    } else if (dte->cdc_port[USB_MODEM_CDC_INTF_MODEM] == cdc_port_handle) {
        if (dte->port_mode == ESP_MODEM_COMMAND_MODE || dte->port_mode == ESP_MODEM_UNKOWN_MODE ||
                dte->is_exiting_ppp) {
            _handle_at_data(cdc_port_handle, dte);
        } else if (dte->port_mode == ESP_MODEM_DATA_MODE) {
            _handle_ppp_data(cdc_port_handle, dte);
        }
    }
}

static void _usbh_dte_dev_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED: {
        esp_modem_dte_t *dte = (esp_modem_dte_t *)user_ctx;

        const usb_config_desc_t *config_desc = event_data->new_dev.active_config_desc;
        const usb_device_desc_t *device_desc = event_data->new_dev.device_desc;

        ESP_LOGI(TAG, "USB CDC device connected, VID: 0x%04x, PID: 0x%04x", device_desc->idVendor, device_desc->idProduct);
        const usb_modem_id_t *matched = modem_device_match(dte->config.modem_id_list, device_desc, config_desc);
        if (matched) {
            ESP_LOGI(TAG, "Matched USB Modem: \"%s\", Modem Interface: %d, AT Interface: %d",
                     matched->name ? matched->name : "Unknown", matched->modem_itf_num, matched->at_itf_num);
        } else {
            ESP_LOGI(TAG, "No matched USB Modem found, ignore this connection");
            return;
        }
        usbh_cdc_port_config_t config = {
            .dev_addr = event_data->new_dev.dev_addr,
            .itf_num = matched->modem_itf_num,
            .in_transfer_buffer_size = 512 * 3,
            .out_transfer_buffer_size = 512 * 3,
            .cbs = {
                .recv_data = _usbh_dte_recv_data_cb,
                .notif_cb = _usbh_dte_notif_cb,
                .user_data = dte,
            }
        };
        ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&config, &(dte->cdc_port[USB_MODEM_CDC_INTF_MODEM])), TAG, "Failed to open MODEM port");
        if (matched->at_itf_num >= 0) {
            usbh_cdc_port_config_t data_config = {
                .dev_addr = event_data->new_dev.dev_addr,
                .itf_num = matched->at_itf_num,
                .in_transfer_buffer_size = 512 * 3,
                .out_transfer_buffer_size = 512 * 3,
                .cbs = {
                    .recv_data = _usbh_dte_recv_data_cb,
                    .notif_cb = _usbh_dte_notif_cb,
                    .user_data = dte,
                }
            };
            ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&data_config, &(dte->cdc_port[USB_MODEM_CDC_INTF_AT])), TAG, "Failed to open AT port");
        }

        dte->port_mode = ESP_MODEM_UNKOWN_MODE; // Set to unknown mode first
        dte->connect_status = 1; // Set to connected
        if (dte->config.cbs.connect) {
            dte->config.cbs.connect(&(dte->eth_driver), dte->config.cbs.user_data);
        }
    } break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED: {
        esp_modem_dte_t *dte = (esp_modem_dte_t *)user_ctx;
        dte->connect_status = 0;
        dte->ppp_connected = false;
        dte->port_mode = ESP_MODEM_UNKOWN_MODE;
        if (dte->config.cbs.disconnect) {
            dte->config.cbs.disconnect(&(dte->eth_driver), dte->config.cbs.user_data);
        }
    } break;
    default:
        break;
    }
}

static esp_err_t _at_send_cmd(const char *command, size_t length, void *usr_data)
{
    esp_err_t ret = ESP_FAIL;
    esp_modem_dte_t *esp_dte = (esp_modem_dte_t *)usr_data;

    // Check if the modem is in command mode or in data mode with "+++" command
    if (esp_dte->cdc_port[USB_MODEM_CDC_INTF_AT] && esp_dte->port_mode == ESP_MODEM_DATA_MODE) {
        ret = usbh_cdc_write_bytes(esp_dte->cdc_port[USB_MODEM_CDC_INTF_AT], (const uint8_t *)command, length, pdMS_TO_TICKS(500));
    } else if (esp_dte->port_mode == ESP_MODEM_COMMAND_MODE || esp_dte->port_mode == ESP_MODEM_UNKOWN_MODE ||
               (esp_dte->port_mode == ESP_MODEM_DATA_MODE && strcmp(command, "+++") == 0)) {
        ret = usbh_cdc_write_bytes(esp_dte->cdc_port[USB_MODEM_CDC_INTF_MODEM], (const uint8_t *)command, length, pdMS_TO_TICKS(500));
    } else {
        ESP_LOGW(TAG, "Not support sending command without AT port command: \"%s\"", command);
        ret = ESP_ERR_NOT_SUPPORTED;
    }
    return ret;
}

static esp_err_t usbh_ppp_set_mediator(iot_eth_driver_t *driver, iot_eth_mediator_t *mediator)
{
    esp_modem_dte_t *dte = __containerof(driver, esp_modem_dte_t, eth_driver);
    dte->mediator = mediator;
    return ESP_OK;
}

static esp_err_t esp_modem_dte_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
{
    esp_modem_dte_t *dte = __containerof(h, esp_modem_dte_t, eth_driver);
    esp_err_t ret = ESP_OK;
    ESP_LOG_BUFFER_HEXDUMP("esp-modem-dte: ppp_output", buffer, buflen, ESP_LOG_VERBOSE);

    usbh_cdc_port_handle_t modem_port = dte->cdc_port[USB_MODEM_CDC_INTF_MODEM];
    if (modem_port != NULL) {
        ret = usbh_cdc_write_bytes(modem_port, buffer, buflen, pdMS_TO_TICKS(1000));
    } else {
        ESP_LOGE(TAG, "Failed to transmit packet: modem port is NULL");
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to transmit packet: %s", esp_err_to_name(ret));
    return ret;
}

static esp_err_t usbh_ppp_init(iot_eth_driver_t *handle)
{
    esp_err_t ret = ESP_OK;
    esp_modem_dte_t *dte = __containerof(handle, esp_modem_dte_t, eth_driver);

    ret = usbh_cdc_register_dev_event_cb(ESP_USB_DEVICE_MATCH_ID_ANY, _usbh_dte_dev_event_cb, dte);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to register CDC device event callback");

    // init the AT command parser
    modem_at_config_t at_config = {
        .send_buffer_length = dte->config.at_tx_buffer_size,
        .recv_buffer_length = dte->config.at_rx_buffer_size,
        .io = {
            .send_cmd = _at_send_cmd,
            .usr_data = dte,
        },
        .skip_first_line = true,
    };
    dte->at_handle = modem_at_parser_create(&at_config);
    ESP_GOTO_ON_FALSE(dte->at_handle != NULL, ESP_FAIL, err, TAG, "esp_at_command_create failed");

    dte->mediator->on_stage_changed(dte->mediator, IOT_ETH_STAGE_LL_INIT, NULL);
    ESP_LOGI(TAG, "USB PPP network interface init success");
    return ESP_OK;
err:
    usbh_cdc_unregister_dev_event_cb(_usbh_dte_dev_event_cb);
    if (dte->at_handle) {
        modem_at_parser_destroy(dte->at_handle);
    }
    free(dte);
    return ret;
}

static esp_err_t usbh_ppp_deinit(iot_eth_driver_t *handle)
{
    esp_modem_dte_t *dte = __containerof(handle, esp_modem_dte_t, eth_driver);
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(usbh_cdc_unregister_dev_event_cb(_usbh_dte_dev_event_cb), err, TAG, "Failed to unregister CDC device event callback");
    if (dte->cdc_port[USB_MODEM_CDC_INTF_MODEM] != NULL) {
        usbh_cdc_port_close(dte->cdc_port[USB_MODEM_CDC_INTF_MODEM]);
    }
    if (dte->cdc_port[USB_MODEM_CDC_INTF_AT] != NULL) {
        usbh_cdc_port_close(dte->cdc_port[USB_MODEM_CDC_INTF_AT]);
    }

    ESP_GOTO_ON_ERROR(modem_at_stop(dte->at_handle), err, TAG, "Failed to stop AT parser");
    ESP_GOTO_ON_ERROR(modem_at_parser_destroy(dte->at_handle), err, TAG, "Failed to destroy AT parser");
    free(dte);
err:
    return ret;
}

esp_err_t esp_modem_dte_sync(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    ESP_RETURN_ON_FALSE(dte->port_mode == ESP_MODEM_UNKOWN_MODE, ESP_ERR_INVALID_STATE, TAG, "Already in command mode");

    // Try to switch to command mode when port mode is unknown
    bool ret = DTE_RETRY_OPERATION(at_cmd_at(dte->at_handle) == ESP_OK, (!dte->connect_status));
    if (ret) {
        return ESP_OK; // already in command mode, no need to do anything
    }
    ESP_LOGW(TAG, "Failed to sync with modem DCE, trying to exit data mode");
    ret = DTE_RETRY_OPERATION(at_cmd_exit_data_mode(dte->at_handle) == ESP_OK, (!dte->connect_status));
    ret = DTE_RETRY_OPERATION(at_cmd_at(dte->at_handle) == ESP_OK, (!dte->connect_status));
    ret = DTE_RETRY_OPERATION(at_cmd_hang_up_call(dte->at_handle) == ESP_OK, (!dte->connect_status));
    ESP_RETURN_ON_FALSE(ret == true, ESP_FAIL, TAG, "Failed to sync with modem DCE");
    dte->port_mode = ESP_MODEM_COMMAND_MODE;
    return ESP_OK;
}

esp_err_t esp_modem_dte_dial_up(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    ESP_RETURN_ON_FALSE(dte->ppp_connected == false, ESP_ERR_INVALID_STATE, TAG, "Already in PPP mode, can't dial up");
    vTaskDelay(pdMS_TO_TICKS(500)); // wait for modem to be ready
    ESP_RETURN_ON_ERROR(at_cmd_make_call(dte->at_handle), TAG, "dial up failed");
    dte->ppp_connected = true;
    dte->port_mode = ESP_MODEM_DATA_MODE; // mark as data mode
    vTaskDelay(pdMS_TO_TICKS(100)); // wait for modem to be ready
    return ESP_OK;
}

esp_err_t esp_modem_dte_hang_up(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    ESP_RETURN_ON_FALSE(dte->ppp_connected == true, ESP_ERR_INVALID_STATE, TAG, "Not in PPP mode, can't hang up");
    vTaskDelay(pdMS_TO_TICKS(500)); // wait for modem to be ready
    ESP_RETURN_ON_ERROR(at_cmd_hang_up_call(dte->at_handle), TAG, "hang up failed");
    dte->ppp_connected = false;
    vTaskDelay(pdMS_TO_TICKS(100)); // wait for modem to be ready
    return ESP_OK;
}

esp_err_t esp_modem_dte_change_port_mode(iot_eth_driver_t *dte_drv, modem_port_mode_t new_mode, bool send_at_cmd)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(dte->connect_status, ESP_FAIL, TAG, "Modem not connected");
    ESP_RETURN_ON_FALSE(dte->port_mode != new_mode, ESP_FAIL, TAG, "already in mode: %d", new_mode);
    ESP_RETURN_ON_FALSE(dte->ppp_connected == true, ESP_FAIL, TAG, "PPP not connected, can't change port mode");
    switch (new_mode) {
    case ESP_MODEM_DATA_MODE:
        ESP_RETURN_ON_ERROR(at_cmd_resume_data_mode(dte->at_handle), TAG, "Failed to enter data mode");
        esp_modem_dte_on_stage_changed(dte_drv, IOT_ETH_LINK_UP); // notify mediator that link is up
        break;
    case ESP_MODEM_COMMAND_MODE:
        esp_modem_dte_on_stage_changed(dte_drv, IOT_ETH_LINK_DOWN); // notify mediator that link is down
        if (send_at_cmd) { // some modem need to send AT command to exit data mode
            vTaskDelay(pdMS_TO_TICKS(CONFIG_MODEM_EXIT_PPP_GAP_TIME_MS)); // wait for modem to be ready
            dte->is_exiting_ppp = true;
            ESP_GOTO_ON_ERROR(at_cmd_exit_data_mode(dte->at_handle), exit_data_err, TAG, "Failed to exit data mode");
            dte->is_exiting_ppp = false;
        }
        break;
exit_data_err:
        esp_modem_dte_on_stage_changed(dte_drv, IOT_ETH_LINK_UP); // restore link state
        dte->is_exiting_ppp = false;
        return ESP_FAIL;
    default:
        ESP_LOGE(TAG, "Unsupported mode: %d", new_mode);
        break;
    }
    dte->port_mode = new_mode;
    return ret;
}

modem_port_mode_t esp_modem_dte_get_port_mode(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    return dte->port_mode;
}

esp_err_t esp_modem_dte_on_stage_changed(iot_eth_driver_t *dte_drv, iot_eth_link_t link)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    dte->mediator->on_stage_changed(dte->mediator, IOT_ETH_STAGE_LINK, &link);
    return ESP_OK;
}

at_handle_t esp_modem_dte_get_atparser(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, NULL, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    return dte->at_handle;
}

bool esp_modem_dte_is_connected(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, false, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);
    return dte->connect_status;
}

bool esp_modem_dte_ppp_is_connect(iot_eth_driver_t *dte_drv)
{
    ESP_RETURN_ON_FALSE(dte_drv != NULL, false, TAG, "Invalid dte handle");
    esp_modem_dte_t *dte = __containerof(dte_drv, esp_modem_dte_t, eth_driver);

    return dte->ppp_connected;
}

esp_err_t esp_modem_dte_new(const esp_modem_dte_config_t *config, iot_eth_driver_t **ret_handle)
{
    /* malloc memory for dte object */
    esp_err_t ret = ESP_OK;
    esp_modem_dte_t *dte = calloc(1, sizeof(esp_modem_dte_t));
    ESP_RETURN_ON_FALSE(dte != NULL, ESP_ERR_NO_MEM, TAG, "calloc dte failed");
    dte->config = *config;

    dte->eth_driver.name = "usb_ppp";
    dte->eth_driver.init = usbh_ppp_init;
    dte->eth_driver.set_mediator = usbh_ppp_set_mediator;
    dte->eth_driver.deinit = usbh_ppp_deinit;
    dte->eth_driver.start = NULL;
    dte->eth_driver.stop = NULL;
    dte->eth_driver.transmit = esp_modem_dte_transmit;
    dte->eth_driver.get_addr = NULL;
    *ret_handle = &(dte->eth_driver);

    return ret;
}
