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
#include "esp_event.h"
#include "iot_usbh_ecm.h"
#include "iot_eth_types.h"
#include "iot_eth_interface.h"
#include "iot_usbh_cdc_type.h"
#include "iot_usbh_ecm_descriptor.h"

static const char *TAG = "iot_usbh_ecm";

#define ECM_AUTO_DETECT (1 << 0)
#define ECM_CONNECTED (1 << 1)
#define ECM_DISCONNECTED (1 << 2)
#define ECM_LINE_CHANGE (1 << 3)
#define ECM_STOP (1 << 4)
#define ECM_STOP_DONE (1 << 5)

#define ECM_EVENT_ALL (ECM_AUTO_DETECT | ECM_CONNECTED | ECM_DISCONNECTED | ECM_LINE_CHANGE | ECM_STOP | ECM_STOP_DONE)

#define ECM_NOTIF_NETWORK_CONNECTION  (0x00)

#define ECM_NET_MTU                   (1514)

typedef struct {
    iot_eth_driver_t base;
    iot_eth_mediator_t *mediator;
    iot_usbh_ecm_config_t config;
    EventGroupHandle_t event_group;
    usbh_cdc_handle_t cdc_dev;
    bool connect_status;
    uint8_t mac_addr[6];
    uint8_t eth_mac_str_index;
    uint8_t eth_mac_addr[6];
    void *user_data;
} usbh_ecm_t;

static esp_err_t usbh_ecm_connect(usbh_ecm_t *ecm)
{
#define ECM_SET_ETHERNET_PACKET_FILTER 0x43
#define PACKET_TYPE_MULTICAST     BIT(4)
#define PACKET_TYPE_BROADCAST     BIT(3)
#define PACKET_TYPE_DIRECTED      BIT(2)
#define PACKET_TYPE_ALL_MULTICAST BIT(1)
#define PACKET_TYPE_PROMISCUOUS   BIT(0)

    esp_err_t ret = ESP_OK;
    uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    uint8_t bRequest = ECM_SET_ETHERNET_PACKET_FILTER;
    uint16_t wValue =  PACKET_TYPE_BROADCAST | PACKET_TYPE_DIRECTED | PACKET_TYPE_ALL_MULTICAST; // 0x0E
    uint16_t wIndex = 0; // Interface number
    uint16_t wLength = 0; // No data

    // Setting ETHERNET packet filter to enable network interface
    ESP_LOGI(TAG, "Setting ETHERNET packet filter");
    ret = usbh_cdc_send_custom_request(ecm->cdc_dev, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ETHERNET packet filter: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set interface alternate setting to enable network connection
    ESP_LOGI(TAG, "Setting interface alternate setting to 1");
    bmRequestType = USB_BM_REQUEST_TYPE_DIR_OUT | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_INTERFACE;
    bRequest = USB_B_REQUEST_SET_INTERFACE;
    wValue = 1; // Alternate setting
    wIndex = ecm->config.itf_num + 1; // Interface number
    wLength = 0; // No data
    ret = usbh_cdc_send_custom_request(ecm->cdc_dev, bmRequestType, bRequest, wValue, wIndex, wLength, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interface alternate setting: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static void _usbh_ecm_cdc_new_dev_cb(usb_device_handle_t usb_dev, void *user_data)
{
    ESP_LOGI(TAG, "USB ECM new device detected");
    if (usb_dev == NULL) {
        ESP_LOGE(TAG, "usb_dev is NULL");
        return;
    }
    const usb_config_desc_t *config_desc = NULL;
    const usb_device_desc_t *device_desc = NULL;
    esp_err_t ret = usb_host_get_active_config_descriptor(usb_dev, &config_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get active config descriptor: %s", esp_err_to_name(ret));
        return;
    }
    ret = usb_host_get_device_descriptor(usb_dev, &device_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device descriptor: %s", esp_err_to_name(ret));
        return;
    }

#if CONFIG_ECM_PRINTF_USB_DESCRIPTION
    usb_print_device_descriptor(device_desc);
    usb_print_config_descriptor(config_desc, NULL);
#endif

    usbh_ecm_t *ecm = (usbh_ecm_t *)user_data;
    int itf_num = 0;
    ret = usbh_ecm_interface_check(device_desc, config_desc, &itf_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to check ECM interface: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "ECM interface found: VID: %04X, PID: %04X, IFNUM: %d", ecm->config.vid, ecm->config.pid, itf_num);
    ecm->config.vid = device_desc->idVendor;
    ecm->config.pid = device_desc->idProduct;
    ecm->config.itf_num = itf_num;

    ret = usbh_ecm_mac_str_index_check(device_desc, config_desc, &ecm->eth_mac_str_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to check ECM MAC string index: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "ECM MAC string index: %d", ecm->eth_mac_str_index);

    xEventGroupSetBits(ecm->event_group, ECM_AUTO_DETECT);
}

static bool parse_mac_descriptor(const uint8_t *raw, uint8_t mac[6])
{
    if (raw[0] < 0x1A) {
        return false;
    }
    if (raw[1] != 0x03) {
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

static void _usbh_ecm_conn_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
#define LANG_ID_ENGLISH 0x409

    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;

    uint8_t bmRequestType = USB_BM_REQUEST_TYPE_DIR_IN | USB_BM_REQUEST_TYPE_TYPE_STANDARD | USB_BM_REQUEST_TYPE_RECIP_DEVICE;
    uint8_t bRequest = USB_B_REQUEST_GET_DESCRIPTOR;
    uint16_t wValue = (USB_W_VALUE_DT_STRING << 8) | (ecm->eth_mac_str_index & 0xFF);
    uint16_t wIndex = LANG_ID_ENGLISH;

    uint8_t str[64] = {0};
    uint16_t wLength = 64;
    ret = usbh_cdc_send_custom_request(cdc_handle, bmRequestType, bRequest, wValue, wIndex, wLength, str);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send custom request: %s", esp_err_to_name(ret));
        return;
    }

    bool result = parse_mac_descriptor(str, ecm->eth_mac_addr);
    if (!result) {
        ESP_LOGE(TAG, "Failed to parse MAC address");
        return;
    }
    ESP_LOGI(TAG, "ECM MAC address: %02X:%02X:%02X:%02X:%02X:%02X", ecm->eth_mac_addr[0], ecm->eth_mac_addr[1], ecm->eth_mac_addr[2], ecm->eth_mac_addr[3], ecm->eth_mac_addr[4], ecm->eth_mac_addr[5]);

    ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_GET_MAC, NULL);

    xEventGroupSetBits(ecm->event_group, ECM_CONNECTED);
}

static void _usbh_ecm_disconn_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    xEventGroupSetBits(ecm->event_group, ECM_DISCONNECTED);
}

static void usbh_ecm_handle_recv_data(usbh_ecm_t *ecm)
{
    esp_err_t ret = ESP_OK;
    int rx_length = 0;
    usbh_cdc_get_rx_buffer_size(ecm->cdc_dev, (size_t *)&rx_length);
    if (rx_length > 0) {
        uint8_t *buf = malloc(rx_length);
        ESP_RETURN_ON_FALSE(buf != NULL,, TAG, "Failed to allocate memory for buffer");
        ret = usbh_cdc_read_bytes(ecm->cdc_dev, buf, (size_t *)&rx_length, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read ecm data from CDC");
            free(buf);
            return;
        }
        ESP_LOGD(TAG, "USB ECM received data: %d", rx_length);
        ecm->mediator->stack_input(ecm->mediator, buf, rx_length);
    }
}

static void _usbh_ecm_recv_data_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    usbh_ecm_handle_recv_data(ecm);
}

static void _usbh_ecm_notif_cb(usbh_cdc_handle_t cdc_handle, iot_cdc_notification_t *notif, void *arg)
{
    if (notif->bNotificationCode == ECM_NOTIF_NETWORK_CONNECTION) {
        usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
        bool connected = notif->wValue;
        if (connected != ecm->connect_status) {
            ecm->connect_status = connected;
            xEventGroupSetBits(ecm->event_group, ECM_LINE_CHANGE);
        }
    }
}

static void _usbh_ecm_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = (usbh_ecm_t *)arg;
    while (1) {
        uint32_t events = xEventGroupWaitBits(ecm->event_group, ECM_EVENT_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        if (events & ECM_CONNECTED) {
            ret = usbh_ecm_connect(ecm);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "USB ECM connected failed, please reconnect");
            }
        }
        if (events & ECM_DISCONNECTED) {
            ESP_LOGD(TAG, "USB ECM disconnected");
            iot_eth_link_t link = IOT_ETH_LINK_DOWN;
            ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LINK, &link);
        }
        if (events & ECM_LINE_CHANGE) {
            ESP_LOGD(TAG, "USB ECM line change: %d", ecm->connect_status);
            iot_eth_link_t link = ecm->connect_status ? IOT_ETH_LINK_UP : IOT_ETH_LINK_DOWN;
            ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LINK, &link);
        }
        if (events & ECM_STOP) {
            ESP_LOGI(TAG, "USB ECM stop");
            break;
        }
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

static esp_err_t usbh_ecm_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
{
    usbh_ecm_t *ecm = __containerof(h, usbh_ecm_t, base);
    esp_err_t ret = ESP_OK;
    ret = usbh_cdc_write_bytes(ecm->cdc_dev, buffer, buflen, pdMS_TO_TICKS(200));
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to transmit packet: %s", esp_err_to_name(ret));
    return ret;
}

static esp_err_t usbh_ecm_start(iot_eth_driver_t *driver)
{
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = __containerof(driver, usbh_ecm_t, base);

    if (ecm->config.auto_detect) {
        uint32_t events = xEventGroupWaitBits(ecm->event_group, ECM_AUTO_DETECT, pdFALSE, pdTRUE, ecm->config.auto_detect_timeout);
        if (events & ECM_AUTO_DETECT) {
            ESP_LOGI(TAG, "USB ECM auto detect success");
        } else {
            ESP_LOGE(TAG, "USB ECM auto detect failed");
            return ESP_ERR_TIMEOUT;
        }
    }

    usbh_cdc_device_config_t dev_config = {
        .vid = ecm->config.vid,
        .pid = ecm->config.pid,
        .itf_num = ecm->config.itf_num,
        .rx_buffer_size = 0,
        .tx_buffer_size = 0,
        .cbs = {
            .connect = _usbh_ecm_conn_cb,
            .disconnect = _usbh_ecm_disconn_cb,
            .recv_data = _usbh_ecm_recv_data_cb,
            .notif_cb = _usbh_ecm_notif_cb,
            .user_data = ecm,
        },
    };

    ret = usbh_cdc_create(&dev_config, &ecm->cdc_dev);
    if (!ecm->config.auto_detect && ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "The device not connect yet");
        ret = ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create CDC handle");
    xTaskCreate(_usbh_ecm_task, "_usbh_ecm_task", 4096, ecm, 5, NULL);
    return ret;
}

esp_err_t usbh_ecm_stop(iot_eth_driver_t *handle)
{
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);
    xEventGroupSetBits(ecm->event_group, ECM_STOP);
    uint32_t events = xEventGroupWaitBits(ecm->event_group, ECM_STOP_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    if (events == 0) {
        ESP_LOGE(TAG, "USB ECM stop failed");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = usbh_cdc_delete(ecm->cdc_dev);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to delete CDC handle");
    return ESP_OK;
}

static esp_err_t usbh_ecm_init(iot_eth_driver_t *handle)
{
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);

    ecm->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(ecm->event_group != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    esp_read_mac(ecm->mac_addr, ESP_MAC_ETH);
    ecm->mac_addr[5] ^= 0x01;

    if (ecm->config.auto_detect) {
        ret = usbh_cdc_register_new_dev_cb(_usbh_ecm_cdc_new_dev_cb, ecm);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to register new device callback");
    }

    ecm->mediator->on_stage_changed(ecm->mediator, IOT_ETH_STAGE_LL_INIT, NULL);
    ESP_LOGI(TAG, "USB ECM network interface init success");
    return ESP_OK;
err:
    if (ecm->event_group != NULL) {
        vEventGroupDelete(ecm->event_group);
    }
    return ret;
}

esp_err_t usbh_ecm_deinit(iot_eth_driver_t *handle)
{
    esp_err_t ret = ESP_OK;
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);
    if (ecm->config.auto_detect) {
        ret = usbh_cdc_unregister_new_dev_cb(_usbh_ecm_cdc_new_dev_cb);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to unregister new device callback");
    }
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
    usbh_ecm_t *ecm = __containerof(handle, usbh_ecm_t, base);
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
    ecm->base.start = usbh_ecm_start;
    ecm->base.stop = usbh_ecm_stop;
    ecm->base.transmit = usbh_ecm_transmit;
    ecm->base.get_addr = usbh_ecm_get_addr;

    *ret_handle = (iot_eth_driver_t *)&ecm->base;
    return ESP_OK;
}
