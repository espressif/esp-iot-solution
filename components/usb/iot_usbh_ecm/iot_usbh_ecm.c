/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "iot_usbh_cdc.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "iot_usbh_ecm.h"
#include "iot_eth_types.h"
#include "iot_eth_interface.h"
#include "iot_usbh_ecm_descriptor.h"

static const char *TAG = "iot_usbh_ecm";

#define ECM_DEV_CONNECTED (1 << 0)
#define ECM_STOP (1 << 4)
#define ECM_STOP_DONE (1 << 5)

#define ECM_EVENT_ALL (ECM_DEV_CONNECTED | ECM_STOP | ECM_STOP_DONE)

// Define the CDC ECM interface request codes, see CDC ECM Subclass 1.2
#define ECM_SET_ETHERNET_PACKET_FILTER 0x43
#define PACKET_TYPE_MULTICAST     BIT(4)
#define PACKET_TYPE_BROADCAST     BIT(3)
#define PACKET_TYPE_DIRECTED      BIT(2)
#define PACKET_TYPE_ALL_MULTICAST BIT(1)
#define PACKET_TYPE_PROMISCUOUS   BIT(0)

#define LANG_ID_ENGLISH 0x409

typedef struct {
    iot_eth_driver_t base;
    iot_eth_mediator_t *mediator;
    iot_usbh_ecm_config_t config;
    EventGroupHandle_t event_group;
    uint8_t cdc_dev_addr;
    usbh_cdc_port_handle_t cdc_port;
    // const usb_device_desc_t *device_desc;
    bool dev_connect_status;
    uint8_t eth_mac_str_index;
    uint8_t eth_mac_addr[6];
    uint16_t itf_num;
    void *user_data;

    int last_connected;
    uint32_t last_up_speed;
    uint32_t last_down_speed;
} usbh_ecm_t;

static bool _parse_mac_descriptor(const uint8_t *raw, uint8_t mac[6])
{
    if (raw[0] < 0x1A || raw[1] != 0x03) {
        return false;
    }

    char ascii[13];
    for (int i = 0; i < 12; ++i) {
        ascii[i] = raw[2 + i * 2];
    }
    ascii[12] = '\0';

    for (int i = 0; i < 6; ++i) {
        char hex[3] = { ascii[i * 2], ascii[i * 2 + 1], '\0' };
        mac[i] = (uint8_t) strtol(hex, NULL, 16);
    }

    return true;
}

static esp_err_t _ecm_config_intf(usbh_ecm_t *ecm)
{
    esp_err_t ret = ESP_OK;

    /* Config the ECM */
    uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_DEVICE;
    uint8_t bRequest = USB_B_REQUEST_GET_DESCRIPTOR;
    uint16_t wValue = (USB_W_VALUE_DT_STRING << 8) | (ecm->eth_mac_str_index & 0xFF);
    uint16_t wIndex = LANG_ID_ENGLISH;

    uint8_t str[64] = {0};
    uint16_t wLength = 64;
    ret = usbh_cdc_send_custom_request(ecm->cdc_port, bmRequestType, bRequest, wValue, wIndex, wLength, str);
    if (ret == ESP_OK) {
        bool result = _parse_mac_descriptor(str, ecm->eth_mac_addr);
        if (!result) {
            ESP_LOGE(TAG, "Failed to parse MAC address from %s", str);
            return ret;
        }
        ESP_LOGI(TAG, "Parsed MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
                 ecm->eth_mac_addr[0], ecm->eth_mac_addr[1], ecm->eth_mac_addr[2],
                 ecm->eth_mac_addr[3], ecm->eth_mac_addr[4], ecm->eth_mac_addr[5]);
    } else {
        ESP_LOGE(TAG, "Failed to send custom request: %s", esp_err_to_name(ret));
        return ret;
    }

    bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    bRequest = ECM_SET_ETHERNET_PACKET_FILTER;
    wValue =  PACKET_TYPE_BROADCAST | PACKET_TYPE_DIRECTED | PACKET_TYPE_ALL_MULTICAST; // 0x0E
    wIndex = 0; // Interface number
    wLength = 0; // No data

    // Setting ETHERNET packet filter to enable network interface
    ESP_LOGI(TAG, "Setting ETHERNET packet filter");
    ret = usbh_cdc_send_custom_request(ecm->cdc_port, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ETHERNET packet filter: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set interface alternate setting to enable network connection
    ESP_LOGI(TAG, "Setting interface alternate setting to 1");
    bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    bRequest = USB_B_REQUEST_SET_INTERFACE;
    wValue = 1; // Alternate setting
    wIndex = ecm->itf_num + 1; // Interface number
    wLength = 0; // No data
    ret = usbh_cdc_send_custom_request(ecm->cdc_port, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interface alternate setting: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t usbh_ecm_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
{
    usbh_ecm_t *ecm = __containerof(h, usbh_ecm_t, base);
    esp_err_t ret = ESP_OK;
    ret = usbh_cdc_write_bytes(ecm->cdc_port, buffer, buflen, pdMS_TO_TICKS(1000));
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to transmit packet: %s", esp_err_to_name(ret));
    return ret;
}

static void _usbh_ecm_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    esp_err_t ret = ESP_OK;
    int rx_length = 0;
    usbh_cdc_get_rx_buffer_size(cdc_port_handle, (size_t *)&rx_length);
    if (rx_length > 0) {
        uint8_t *buf = malloc(rx_length);
        ESP_RETURN_ON_FALSE(buf != NULL,, TAG, "Failed to allocate memory for buffer");
        ret = usbh_cdc_read_bytes(cdc_port_handle, buf, (size_t *)&rx_length, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ecm data from CDC");
            free(buf);
            return;
        }
        ESP_LOGD(TAG, "USB ECM received data: %d", rx_length);
        ecm->mediator->stack_input(ecm->mediator, buf, rx_length);
    }
}

static void _usbh_ecm_notif_cb(usbh_cdc_port_handle_t cdc_port_handle, iot_cdc_notification_t *notif, void *arg)
{
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    switch (notif->bNotificationCode) {
    case USB_CDC_NOTIFY_NETWORK_CONNECTION:
        int connected = notif->wValue;
        if (ecm->last_connected != connected) { // to avoid print the message too frequently
            ecm->last_connected = connected;
            ESP_LOGI(TAG, "Notify - network connection changed: %s", connected ? "Connected" : "Disconnected");
            iot_eth_link_t link = connected ? IOT_ETH_LINK_UP : IOT_ETH_LINK_DOWN;
            ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LINK, &link);
        }
        break;
    case USB_CDC_NOTIFY_SPEED_CHANGE: {
        uint32_t *speeds = (uint32_t *)notif->Data;
        if (ecm->last_up_speed != speeds[0] || ecm->last_down_speed != speeds[1]) {
            ecm->last_up_speed = speeds[0];
            ecm->last_down_speed = speeds[1];
            ESP_LOGI(TAG, "Notify - link speeds: %lu kbps ↑, %lu kbps ↓", (speeds[0]) / 1000, (speeds[1]) / 1000);
        }
        break;
    }
    default:
        ESP_LOGW(TAG, "unexpected notification 0x%02x!", notif->bNotificationCode);
        return;
    }
}

static void _usbh_ecm_dev_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED: {
        usbh_ecm_t *ecm = (usbh_ecm_t *)user_ctx;
        if (ecm->cdc_port != NULL) {
            ESP_LOGD(TAG, "ECM port already opened, ignore this event");
            return;
        }
        const usb_config_desc_t *config_desc = event_data->new_dev.active_config_desc;
        const usb_device_desc_t *device_desc = event_data->new_dev.device_desc;

        int itf_num = -1;
        esp_err_t ret = usbh_ecm_interface_check(device_desc, config_desc, &itf_num);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "No ECM interface found for device VID: %04X, PID: %04X, ignore it", device_desc->idVendor, device_desc->idProduct);
            return;
        }
        ESP_LOGI(TAG, "ECM interface found: VID: %04X, PID: %04X, IFNUM: %d", device_desc->idVendor, device_desc->idProduct, itf_num);
        const usb_ecm_function_desc_t *ecm_func_desc = NULL;
        ESP_RETURN_VOID_ON_ERROR(usbh_get_ecm_function_desc(device_desc, config_desc, &ecm_func_desc), TAG, "Failed to check ECM function descriptor: %s", esp_err_to_name(err_rc_));
        ecm->eth_mac_str_index = ecm_func_desc->iMACAddress;

        ecm->itf_num = itf_num;
        usbh_cdc_port_config_t cdc_port_config = {
            .dev_addr = event_data->new_dev.dev_addr,
            .itf_num = ecm->itf_num,
            .in_transfer_buffer_size = usb_round_up_to_mps(ecm_func_desc->wMaxSegmentSize, 512),
            .out_transfer_buffer_size = usb_round_up_to_mps(ecm_func_desc->wMaxSegmentSize, 512),
            .cbs = {
                .notif_cb = _usbh_ecm_notif_cb,
                .recv_data = _usbh_ecm_recv_data_cb,
                .user_data = ecm,
            },
#ifdef CONFIG_ECM_DISABLE_CDC_NOTIFICATION
            .flags = USBH_CDC_FLAGS_DISABLE_NOTIFICATION,
#endif
        };
        ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&cdc_port_config, &ecm->cdc_port), TAG, "Failed to open CDC port");

        ecm->cdc_dev_addr = event_data->new_dev.dev_addr;
        ecm->dev_connect_status = 1;
        ecm->last_connected = -1;
        ecm->last_up_speed = -1;
        ecm->last_down_speed = -1;

        xEventGroupSetBits(ecm->event_group, ECM_DEV_CONNECTED);
    } break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED: {
        usbh_ecm_t *ecm = (usbh_ecm_t *)user_ctx;
        if (ecm->cdc_dev_addr == event_data->dev_gone.dev_addr) {
            ecm->cdc_dev_addr = 0;
            ecm->dev_connect_status = 0;
            ecm->cdc_port = NULL;
            memset(ecm->eth_mac_addr, 0, 6);
            iot_eth_link_t link = IOT_ETH_LINK_DOWN;
            ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LINK, &link);
        }
    } break;

    default:
        break;
    }
}

static void _usbh_ecm_task(void *arg)
{
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    uint32_t connected_time_ms = 0;
    while (1) {
        uint32_t events = xEventGroupWaitBits(ecm->event_group, ECM_EVENT_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        if (events & ECM_DEV_CONNECTED) {
            _ecm_config_intf(ecm);
            connected_time_ms = pdTICKS_TO_MS(xTaskGetTickCount());
        }
        if (events & ECM_STOP) {
            ESP_LOGI(TAG, "USB ECM stop");
            break;
        }
#ifdef CONFIG_ECM_AUTO_LINK_UP_AFTER_NO_NOTIFICATION
        if (ecm->last_connected == -1 && ecm->dev_connect_status &&
                ((connected_time_ms + CONFIG_ECM_AUTO_LINK_UP_WAIT_TIME_MS) < pdTICKS_TO_MS(xTaskGetTickCount()))) {
            ESP_LOGW(TAG, "ECM network connection not notified after %dms of connection, assume link is up", CONFIG_ECM_AUTO_LINK_UP_WAIT_TIME_MS);
            ecm->last_connected = 1;
            iot_eth_link_t link = IOT_ETH_LINK_UP;
            ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LINK, &link);
        }
#else
        (void)connected_time_ms; // suppress unused variable warning
#endif
    }
    ESP_LOGI(TAG, "USB ECM task exit");
    xEventGroupSetBits(ecm->event_group, ECM_STOP_DONE);
    vTaskDelete(NULL);
}

static esp_err_t usbh_ecm_set_mediator(iot_eth_driver_t *driver, iot_eth_mediator_t *mediator)
{
    usbh_ecm_t *ecm = __containerof(driver, usbh_ecm_t, base);
    ecm->mediator = mediator;
    return ESP_OK;
}

static esp_err_t usbh_ecm_init(iot_eth_driver_t *handle)
{
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);

    ecm->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(ecm->event_group != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    ret = usbh_cdc_register_dev_event_cb(ecm->config.match_id_list, _usbh_ecm_dev_event_cb, ecm);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Failed to register CDC device event callback");

    BaseType_t task_created = xTaskCreate(_usbh_ecm_task, "_usbh_ecm_task", 4096, ecm, 5, NULL);
    ESP_GOTO_ON_FALSE(task_created == pdPASS, ESP_FAIL, err_task, TAG, "Failed to create USB ECM task");

    ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LL_INIT, NULL);
    ESP_LOGI(TAG, "USB ECM network interface init success");
    return ESP_OK;

err_task:
    usbh_cdc_unregister_dev_event_cb(_usbh_ecm_dev_event_cb);
err:
    if (ecm->event_group != NULL) {
        vEventGroupDelete(ecm->event_group);
    }
    return ret;
}

esp_err_t usbh_ecm_deinit(iot_eth_driver_t *handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);

    xEventGroupSetBits(ecm->event_group, ECM_STOP);
    xEventGroupWaitBits(ecm->event_group, ECM_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

    ret = usbh_cdc_unregister_dev_event_cb(_usbh_ecm_dev_event_cb);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to unregister CDC device event callback");

    if (ecm->event_group != NULL) {
        vEventGroupDelete(ecm->event_group);
    }
    if (ecm) {
        free(ecm);
    }

    return ESP_OK;
}

esp_err_t usbh_ecm_get_addr(iot_eth_driver_t *handle, uint8_t *mac_addr)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(mac_addr != NULL, ESP_ERR_INVALID_ARG, TAG, "mac_addr is NULL");
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);
    ESP_RETURN_ON_FALSE(ecm->dev_connect_status, ESP_ERR_INVALID_STATE, TAG, "ECM device not connected");
    ESP_RETURN_ON_FALSE((ecm->eth_mac_addr[0] + ecm->eth_mac_addr[1] + ecm->eth_mac_addr[2]) != 0, ESP_ERR_INVALID_STATE, TAG, "ECM MAC address not available");
    memcpy(mac_addr, ecm->eth_mac_addr, 6);
    return ESP_OK;
}

esp_err_t iot_eth_new_usb_ecm(const iot_usbh_ecm_config_t *config, iot_eth_driver_t **ret_handle)
{
    ESP_LOGI(TAG, "IOT USBH ECM Version: %d.%d.%d", IOT_USBH_ECM_VER_MAJOR, IOT_USBH_ECM_VER_MINOR, IOT_USBH_ECM_VER_PATCH);
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(ret_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "ret_handle is NULL");

    usbh_ecm_t *ecm = calloc(1, sizeof(usbh_ecm_t));
    ESP_RETURN_ON_FALSE(ecm != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for usbh_ecm_t");

    memcpy(&ecm->config, config, sizeof(iot_usbh_ecm_config_t));
    ecm->base.name = "usb_ecm";
    ecm->base.init = usbh_ecm_init;
    ecm->base.set_mediator = usbh_ecm_set_mediator;
    ecm->base.deinit = usbh_ecm_deinit;
    ecm->base.start = NULL;
    ecm->base.stop = NULL;
    ecm->base.transmit = usbh_ecm_transmit;
    ecm->base.get_addr = usbh_ecm_get_addr;

    *ret_handle = (iot_eth_driver_t *)&ecm->base;
    return ESP_OK;
}

usbh_cdc_port_handle_t usb_ecm_get_cdc_port_handle(const iot_eth_driver_t *ecm_drv)
{
    ESP_RETURN_ON_FALSE(ecm_drv != NULL, NULL, TAG, "ecm is NULL");
    usbh_ecm_t *ecm = __containerof(ecm_drv, usbh_ecm_t, base);
    return ecm->cdc_port;
}
