/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
#include "usbh_rndis_protocol.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "iot_usbh_rndis.h"
#include "iot_usbh_rndis_descriptor.h"
#include "iot_eth_types.h"
#include "iot_usbh_cdc_type.h"

static const char *TAG = "usbh_rndis";

#define RNDIS_AUTO_DETECT (1 << 0)
#define RNDIS_CONNECTED (1 << 1)
#define RNDIS_DISCONNECTED (1 << 2)
#define RNDIS_LINE_CHANGE (1 << 3)
#define RNDIS_STOP (1 << 4)
#define RNDIS_STOP_DONE (1 << 5)

#define RNDIS_EVENT_ALL (RNDIS_CONNECTED | RNDIS_DISCONNECTED | RNDIS_LINE_CHANGE | RNDIS_STOP | RNDIS_STOP_DONE)

typedef struct {
    iot_eth_driver_t base;
    iot_eth_mediator_t *mediator;
    iot_usbh_rndis_config_t config;
    usbh_cdc_handle_t cdc_dev;
    EventGroupHandle_t event_group;
    uint32_t request_id;
    uint32_t max_transfer_pkts; /* max packets in one transfer */
    uint32_t max_transfer_size; /* max size in one transfer */
    uint32_t link_speed;
    bool connect_status;
    uint8_t mac_addr[6];
    uint8_t eth_mac_addr[6];
    void *user_data;
} usbh_rndis_t;

static esp_err_t usbh_rndis_init_msg_request(usbh_rndis_t * rndis)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[256];
    rndis_initialize_msg_t *cmd;
    rndis_initialize_cmplt_t *resp;

    cmd = (rndis_initialize_msg_t *)data;
    cmd->MessageType = REMOTE_NDIS_INITIALIZE_MSG;
    cmd->MessageLength = sizeof(rndis_initialize_msg_t);
    cmd->RequestId = rndis->request_id++;
    cmd->MajorVersion = 0x1;
    cmd->MinorVersion = 0x0;
    cmd->MaxTransferSize = 0x4000;

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0, sizeof(rndis_initialize_msg_t), (uint8_t *)cmd);

    resp = (rndis_initialize_cmplt_t *)data;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read response %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "MessageLength %lu", resp->MessageLength);

    rndis->max_transfer_pkts = resp->MaxPacketsPerTransfer;
    rndis->max_transfer_size = resp->MaxTransferSize;
    ESP_LOGI(TAG, "Max transfer packets: %"PRIu32"", rndis->max_transfer_pkts);
    ESP_LOGI(TAG, "Max transfer size: %"PRIu32"", rndis->max_transfer_size);

    return ESP_OK;
}

static esp_err_t usbh_rndis_query_msg_request(usbh_rndis_t *rndis, uint32_t oid, uint32_t query_len, uint8_t *info, uint32_t *info_len)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[256];
    rndis_query_msg_t *cmd;
    rndis_query_cmplt_t *resp;

    cmd = (rndis_query_msg_t *)data;

    cmd->MessageType = REMOTE_NDIS_QUERY_MSG;
    cmd->MessageLength = query_len + sizeof(rndis_query_msg_t);
    cmd->RequestId = rndis->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = query_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;
    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_initialize_msg_t) + query_len, data);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send query message");

    resp = (rndis_query_cmplt_t *)data;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read response");
    memcpy(info, ((uint8_t *)resp + sizeof(rndis_query_cmplt_t)), resp->InformationBufferLength);
    *info_len = resp->InformationBufferLength;
    ESP_LOGI(TAG, "InformationBufferLength %lu", *info_len);
    return ret;
}

static esp_err_t usbh_rndis_set_msg_request(usbh_rndis_t *rndis, uint32_t oid, uint8_t *info, uint32_t info_len)
{
    esp_err_t ret = 0;
    uint8_t data[256];
    rndis_set_msg_t *cmd;
    rndis_set_cmplt_t *resp;

    cmd = (rndis_set_msg_t *)data;

    cmd->MessageType = REMOTE_NDIS_SET_MSG;
    cmd->MessageLength = info_len + sizeof(rndis_set_msg_t);
    cmd->RequestId = rndis->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = info_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;

    memcpy(((uint8_t *)cmd + sizeof(rndis_set_msg_t)), info, info_len);

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_set_msg_t) + info_len, data);

    resp = (rndis_set_cmplt_t *)data;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    ESP_LOGI(TAG, "resp->Status: %"PRIu32"", resp->Status);
    ESP_RETURN_ON_ERROR(ret, TAG, "oid:%08x recv error, ret: %s", (unsigned int)oid, esp_err_to_name(ret));
    return ret;
}

static esp_err_t usbh_rndis_get_connect_status(usbh_rndis_t *rndis)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[32];
    uint32_t data_len = 0;
    bool connect_status = false;
    ret = usbh_rndis_query_msg_request(rndis, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to query OID_GEN_MEDIA_CONNECT_STATUS");
    if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
        connect_status = true;
    } else {
        connect_status = false;
    }

    if (connect_status != rndis->connect_status) {
        rndis->connect_status = connect_status;
        xEventGroupSetBits(rndis->event_group, RNDIS_LINE_CHANGE);
    }

    return ret;
}

static esp_err_t usbh_rndis_keepalive(usbh_rndis_t *handle)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[256];
    rndis_keepalive_msg_t *cmd;
    rndis_keepalive_cmplt_t *resp;

    usbh_rndis_t *rndis = handle;

    cmd = (rndis_keepalive_msg_t *)data;

    cmd->MessageType = REMOTE_NDIS_KEEPALIVE_MSG;
    cmd->MessageLength = sizeof(rndis_keepalive_msg_t);
    cmd->RequestId = rndis->request_id++;

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_keepalive_msg_t), data);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send keepalive message");

    resp = (rndis_keepalive_cmplt_t *)data;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_dev, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "rndis_keepalive error, ret: %s", esp_err_to_name(ret));
    return ret;
}

static esp_err_t usbh_rndis_connect(usbh_rndis_t *handle)
{
    usbh_rndis_t *rndis = handle;
    esp_err_t ret = ESP_OK;
    uint32_t *oid_support_list;
    unsigned int oid = 0;
    unsigned int oid_num = 0;
    uint8_t tmp_buffer[256];
    uint8_t data[32];
    uint32_t data_len;
    ESP_RETURN_ON_FALSE(rndis, ESP_ERR_INVALID_STATE, TAG, "rndis not init");
    ret = usbh_rndis_init_msg_request(rndis);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to init rndis");
    ret = usbh_rndis_query_msg_request(rndis, OID_GEN_SUPPORTED_LIST, 0, tmp_buffer, &data_len);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to query OID_GEN_SUPPORTED_LIST");

    oid_num = (data_len / 4);
    ESP_LOGI(TAG, "rndis query OID_GEN_SUPPORTED_LIST success,oid num :%d", oid_num);

    oid_support_list = (uint32_t *)tmp_buffer;
    for (int i = 0; i < oid_num; i++) {
        oid = oid_support_list[i];
        switch (oid) {
        case OID_GEN_PHYSICAL_MEDIUM:
            ret = usbh_rndis_query_msg_request(rndis, OID_GEN_PHYSICAL_MEDIUM, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
            break;
        case OID_GEN_MAXIMUM_FRAME_SIZE:
            ret = usbh_rndis_query_msg_request(rndis, OID_GEN_MAXIMUM_FRAME_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_MAXIMUM_FRAME_SIZE query error, ret: %s", esp_err_to_name(ret));
            break;
        case OID_GEN_LINK_SPEED:
            ret = usbh_rndis_query_msg_request(rndis, OID_GEN_LINK_SPEED, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_LINK_SPEED query error, ret: %s", esp_err_to_name(ret));
            memcpy(&rndis->link_speed, data, 4);
            break;
        case OID_GEN_MEDIA_CONNECT_STATUS:
            ret = usbh_rndis_query_msg_request(rndis, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_MEDIA_CONNECT_STATUS query error, ret: %s", esp_err_to_name(ret));
            if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
                rndis->connect_status = true;
            } else {
                rndis->connect_status = false;
            }
            break;
        case OID_802_3_MAXIMUM_LIST_SIZE:
            ret = usbh_rndis_query_msg_request(rndis, OID_802_3_MAXIMUM_LIST_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_802_3_MAXIMUM_LIST_SIZE query error, ret: %s", esp_err_to_name(ret));
            break;
        case OID_802_3_CURRENT_ADDRESS:
            ret = usbh_rndis_query_msg_request(rndis, OID_802_3_CURRENT_ADDRESS, 6, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_802_3_CURRENT_ADDRESS query error, ret: %s", esp_err_to_name(ret));
            for (uint8_t j = 0; j < 6; j++) {
                rndis->eth_mac_addr[j] = data[j];
            }
            break;
        case OID_802_3_PERMANENT_ADDRESS:
            ret = usbh_rndis_query_msg_request(rndis, OID_802_3_PERMANENT_ADDRESS, 6, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
            break;
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            ret = usbh_rndis_query_msg_request(rndis, OID_GEN_MAXIMUM_TOTAL_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
            break;
        default:
            ESP_LOGW(TAG, "Ignore rndis query iod:%08x", oid);
            continue;
        }
        ESP_LOGD(TAG, "rndis query iod:%08x success", oid);
    }

    uint32_t packet_filter = 0x0f;
    ret = usbh_rndis_set_msg_request(rndis, OID_GEN_CURRENT_PACKET_FILTER, (uint8_t *)&packet_filter, 4);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OID_GEN_CURRENT_PACKET_FILTER");
    ESP_LOGI(TAG, "rndis set OID_GEN_CURRENT_PACKET_FILTER success");

    ret = usbh_rndis_set_msg_request(rndis, OID_802_3_MULTICAST_LIST, rndis->mac_addr, 6);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OID_802_3_MULTICAST_LIST");
    ESP_LOGI(TAG, "rndis set OID_802_3_MULTICAST_LIST success");

    ESP_LOGD(TAG, "rndis MAC address %02x:%02x:%02x:%02x:%02x:%02x",
             rndis->eth_mac_addr[0],
             rndis->eth_mac_addr[1],
             rndis->eth_mac_addr[2],
             rndis->eth_mac_addr[3],
             rndis->eth_mac_addr[4],
             rndis->eth_mac_addr[5]);

    rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_GET_MAC, NULL);
    return ret;
query_errorout:
    ESP_LOGE(TAG, "rndis query iod:%08x error", oid);
    return ret;
}

static esp_err_t usbh_rndis_handle_recv_data(usbh_rndis_t *rndis)
{
    esp_err_t ret;
    rndis_data_packet_t pmsg = {0};
    int rx_length = 0;
    usbh_cdc_get_rx_buffer_size(rndis->cdc_dev, (size_t *)&rx_length);
    if (rx_length > 0) {
        int read_len = sizeof(rndis_data_packet_t);
        if (rx_length < read_len) {
            return ESP_ERR_INVALID_STATE;
        }
        uint8_t *buf = malloc(rx_length);
        ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for buffer");
        ret = usbh_cdc_read_bytes(rndis->cdc_dev, buf, (size_t *)&rx_length, 0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read rndis_data_packet_t from CDC");
            free(buf);
            return ret;
        }

        pmsg = *(rndis_data_packet_t *)buf;
        if (pmsg.MessageType == REMOTE_NDIS_PACKET_MSG) {
            uint8_t *data = malloc(pmsg.DataLength);
            if (data == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for data");
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            memcpy(data, (uint8_t *)buf + sizeof(rndis_data_packet_t), pmsg.DataLength);

            rndis->mediator->stack_input(rndis->mediator, data, pmsg.DataLength);
            free(buf);
        } else {
            ESP_LOGE(TAG, "Error rndis packet message");
            free(buf);
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ESP_OK;
}

/**
 * @brief Transmit data over the RNDIS interface.
 *
 * This function sends data through the RNDIS interface to the connected USB device.
 *
 * @param h Pointer to the Ethernet driver structure.
 * @param buffer Pointer to the data buffer to be transmitted.
 * @param buflen Length of the data buffer.
 *
 * @return
 *     - ESP_OK: Data transmitted successfully
 *     - ESP_ERR_INVALID_STATE: Device not connected
 *     - ESP_ERR_TIMEOUT: Write operation timed out
 */
static esp_err_t usbh_rndis_transmit(iot_eth_driver_t *h, uint8_t *buffer, size_t buflen)
{
    usbh_rndis_t *rndis = __containerof(h, usbh_rndis_t, base);
    rndis_data_packet_t *hdr;
    uint32_t len;

    if (rndis->connect_status == false) {
        return ESP_ERR_INVALID_STATE;
    }

    hdr = (rndis_data_packet_t *)malloc(sizeof(rndis_data_packet_t) + buflen);

    hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = sizeof(rndis_data_packet_t) + buflen;
    hdr->DataOffset = sizeof(rndis_data_packet_t) - sizeof(rndis_generic_msg_t);
    hdr->DataLength = buflen;

    len = hdr->MessageLength;
    ESP_LOGD(TAG, "rndis tx length %"PRIu32"", len);
    memcpy((uint8_t *)hdr + sizeof(rndis_data_packet_t), buffer, buflen);

    if (usbh_cdc_write_bytes(rndis->cdc_dev, (uint8_t *)hdr, len, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to CDC");
        free(hdr);
        return ESP_ERR_TIMEOUT;
    }
    free(hdr);
    return ESP_OK;
}

/**
 * @brief Get the MAC address of the RNDIS interface.
 *
 * This function retrieves the MAC address of the RNDIS interface.
 *
 * @param driver Pointer to the Ethernet driver structure.
 * @param mac_address Pointer to the buffer where the MAC address will be stored.
 *
 * @return
 *     - ESP_OK: MAC address retrieved successfully
 */
static esp_err_t usbh_rndis_get_addr(iot_eth_driver_t *driver, uint8_t *mac_address)
{
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);
    ESP_RETURN_ON_FALSE(rndis->eth_mac_addr != NULL, ESP_ERR_INVALID_STATE, TAG, "MAC address can not be NULL");
    memcpy(mac_address, rndis->eth_mac_addr, 6);
    return ESP_OK;
}

/**
 * @brief Set the mediator for the RNDIS driver.
 *
 * This function assigns a mediator to the RNDIS driver, which is used for communication between the driver and the application.
 *
 * @param driver Pointer to the Ethernet driver structure.
 * @param mediator Pointer to the mediator structure.
 *
 * @return
 *     - ESP_OK: Mediator set successfully
 */
static esp_err_t usbh_rndis_set_mediator(iot_eth_driver_t *driver, iot_eth_mediator_t *mediator)
{
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);
    rndis->mediator = mediator;
    return ESP_OK;
}

static void _usbh_rndis_cdc_new_dev_cb(usb_device_handle_t usb_dev, void *user_data)
{
    ESP_LOGI(TAG, "USB RNDIS CDC new device found");
    if (usb_dev == NULL) {
        ESP_LOGE(TAG, "usb_dev is NULL");
        return;
    }
    const usb_config_desc_t *config_desc = NULL;
    const usb_device_desc_t *device_desc = NULL;
    esp_err_t ret = usb_host_get_active_config_descriptor(usb_dev, &config_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get active config descriptor");
        return;
    }
    ret = usb_host_get_device_descriptor(usb_dev, &device_desc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get device descriptor");
        return;
    }

    usb_print_device_descriptor(device_desc);
    usb_print_config_descriptor(config_desc, NULL);

    // TODO: May need to filter out HUB devices here

    int itf_num = 0;
    ret = usbh_rndis_interface_check(device_desc, config_desc, &itf_num);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "This device is not a RNDIS device");
        return;
    }

    ESP_LOGI(TAG, "RNDIS device found, VID: %d, PID: %d, ITF: %d", device_desc->idVendor, device_desc->idProduct, itf_num);
    usbh_rndis_t *rndis = (usbh_rndis_t *)user_data;
    rndis->config.vid = device_desc->idVendor;
    rndis->config.pid = device_desc->idProduct;
    rndis->config.itf_num = itf_num;
    xEventGroupSetBits(rndis->event_group, RNDIS_AUTO_DETECT);
}

static void _usbh_rndis_conn_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    xEventGroupSetBits(rndis->event_group, RNDIS_CONNECTED);
}

static void _usbh_rndis_disconn_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    xEventGroupSetBits(rndis->event_group, RNDIS_DISCONNECTED);
}

static void _usbh_rndis_recv_data_cb(usbh_cdc_handle_t cdc_handle, void *arg)
{
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    usbh_rndis_handle_recv_data(rndis);
}

/**
 * @brief Used to receive control response length, no processing needed here
 */
static void _usbh_rndis_notif_cb(usbh_cdc_handle_t cdc_handle, iot_cdc_notification_t *notif, void *arg)
{
    ESP_LOGI(TAG, "USB RNDIS CDC notification");
}

static void _usbh_rndis_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    while (1) {
        uint32_t events = xEventGroupWaitBits(rndis->event_group, RNDIS_EVENT_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        if (events & RNDIS_CONNECTED) {
            ret = usbh_rndis_connect(rndis);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "RNDIS connected success");
                iot_eth_link_t link = IOT_ETH_LINK_UP;
                rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LINK, &link);
            } else {
                ESP_LOGE(TAG, "RNDIS connected failed, please reconnect");
            }
        }
        if (events & RNDIS_DISCONNECTED) {
            ESP_LOGI(TAG, "RNDIS disconnected");
            rndis->request_id = 0;
            iot_eth_link_t link = IOT_ETH_LINK_DOWN;
            rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LINK, &link);
        }
        if (events & RNDIS_LINE_CHANGE) {
            ESP_LOGI(TAG, "RNDIS line change: %d", rndis->connect_status);
            iot_eth_link_t link = rndis->connect_status ? IOT_ETH_LINK_UP : IOT_ETH_LINK_DOWN;
            rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LINK, &link);
        }
        if (events & RNDIS_STOP) {
            ESP_LOGI(TAG, "RNDIS stop");
            break;
        }
        if (events == 0) {
            // Timeout occurred, send keepalive
            ret = usbh_rndis_get_connect_status(rndis);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to get connect status");
                continue;
            }
            if (rndis->connect_status == false) {
                ESP_LOGD(TAG, "No events for 5s, sending keepalive");
                ret = usbh_rndis_keepalive(rndis);
            }

            ESP_LOGI(TAG, "No events for 5s, sending keepalive");
            ret = usbh_rndis_keepalive(rndis);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send keepalive");
            }
        }
    }
    ESP_LOGI(TAG, "RNDIS task exit");
    xEventGroupSetBits(rndis->event_group, RNDIS_STOP_DONE);
    vTaskDelete(NULL);
}

/**
 * @brief Start the RNDIS USB host driver.
 *
 * This function starts the RNDIS USB host driver, enabling it to handle USB events and data transfers.
 *
 * @param driver Pointer to the Ethernet driver structure.
 *
 * @return
 *     - ESP_OK: Driver started successfully
 *     - ESP_ERR_TIMEOUT: Auto detect failed
 */
static esp_err_t usbh_rndis_start(iot_eth_driver_t *driver)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);

    if (rndis->config.auto_detect) {
        uint32_t events = xEventGroupWaitBits(rndis->event_group, RNDIS_AUTO_DETECT, pdFALSE, pdTRUE, rndis->config.auto_detect_timeout);
        if (events & RNDIS_AUTO_DETECT) {
            ESP_LOGI(TAG, "RNDIS auto detect success");
        } else {
            ESP_LOGE(TAG, "RNDIS auto detect failed");
            return ESP_ERR_TIMEOUT;
        }
    }

    usbh_cdc_device_config_t dev_config = {
        .vid = rndis->config.vid,
        .pid = rndis->config.pid,
        .itf_num = rndis->config.itf_num,
        .rx_buffer_size = 0,                      // Not use internal ringbuf
        .tx_buffer_size = 0,                      // Not use internal ringbuf
        .cbs = {
            .connect = _usbh_rndis_conn_cb,
            .disconnect = _usbh_rndis_disconn_cb,
            .recv_data = _usbh_rndis_recv_data_cb,
            .notif_cb = _usbh_rndis_notif_cb,
            .user_data = rndis,
        },
    };

    ret = usbh_cdc_create(&dev_config, &rndis->cdc_dev);
    if (!rndis->config.auto_detect && ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "The device not connect yet");
        ret = ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create CDC handle");
    xTaskCreate(_usbh_rndis_task, "_usbh_rndis_task", 4096, rndis, 5, NULL);
    return ret;
}

/**
 * @brief Stop the RNDIS USB host driver.
 *
 * This function stops the RNDIS USB host driver, halting all operations and releasing resources.
 *
 * @param driver Pointer to the Ethernet driver structure.
 *
 * @return
 *     - ESP_OK: Driver stopped successfully
 *     - ESP_ERR_TIMEOUT: Stop operation timed out
 */
static esp_err_t usbh_rndis_stop(iot_eth_driver_t *driver)
{
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);
    xEventGroupSetBits(rndis->event_group, RNDIS_STOP);
    uint32_t events = xEventGroupWaitBits(rndis->event_group, RNDIS_STOP_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
    if (events == 0) {
        ESP_LOGE(TAG, "RNDIS stop failed");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = usbh_cdc_delete(rndis->cdc_dev);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to delete CDC handle");

    return ESP_OK;
}

/**
 * @brief Initialize the RNDIS USB host driver.
 *
 * This function sets up the necessary resources and configurations for the RNDIS USB host driver.
 *
 * @param driver Pointer to the Ethernet driver structure.
 *
 * @return
 *     - ESP_OK: Initialization successful
 *     - ESP_ERR_NO_MEM: Failed to create event group
 *     - ESP_FAIL: Failed to install CDC driver
 */
esp_err_t usbh_rndis_init(iot_eth_driver_t *driver)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);

    rndis->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(rndis->event_group != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    // Generate a MAC address for the interface
    esp_read_mac(rndis->mac_addr, ESP_MAC_ETH);
    rndis->mac_addr[5] ^= 0x01; // Make it unique from the default Ethernet MAC

    if (rndis->config.auto_detect) {
        ret = usbh_cdc_register_new_dev_cb(_usbh_rndis_cdc_new_dev_cb, rndis);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, err, TAG, "Failed to register new device callback");
    }

    rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LL_INIT, NULL);
    ESP_LOGI(TAG, "USB RNDIS network interface init success");
    return ESP_OK;
err:
    if (rndis->event_group != NULL) {
        vEventGroupDelete(rndis->event_group);
    }
    return ret;
}

/**
 * @brief Deinitialize the RNDIS USB host driver.
 *
 * This function releases the resources allocated by the RNDIS USB host driver.
 *
 * @param driver Pointer to the Ethernet driver structure.
 *
 * @return
 *     - ESP_OK: Deinitialization successful
 *     - ESP_ERR_INVALID_STATE: Failed to uninstall CDC driver
 */
static esp_err_t usbh_rndis_deinit(iot_eth_driver_t *driver)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);
    if (rndis->config.auto_detect) {
        ret = usbh_cdc_unregister_new_dev_cb(_usbh_rndis_cdc_new_dev_cb);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to unregister new device callback");
    }
    if (rndis->event_group != NULL) {
        vEventGroupDelete(rndis->event_group);
    }
    if (rndis) {
        free(rndis);
    }
    return ESP_OK;
}

esp_err_t iot_eth_new_usb_rndis(const iot_usbh_rndis_config_t *config, iot_eth_driver_t **ret_handle)
{
    ESP_LOGI(TAG, "IOT USBH RNDIS Version: %d.%d.%d", IOT_USBH_RNDIS_VER_MAJOR, IOT_USBH_RNDIS_VER_MINOR, IOT_USBH_RNDIS_VER_PATCH);
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(ret_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "ret_handle is NULL");

    usbh_rndis_t *rndis = calloc(1, sizeof(usbh_rndis_t));
    ESP_RETURN_ON_FALSE(rndis != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for usbh_rndis_t");

    memcpy(&rndis->config, config, sizeof(iot_usbh_rndis_config_t));
    rndis->base.name = "usb_rndis";
    rndis->base.init = usbh_rndis_init;
    rndis->base.set_mediator = usbh_rndis_set_mediator;
    rndis->base.deinit = usbh_rndis_deinit;
    rndis->base.start = usbh_rndis_start;
    rndis->base.stop = usbh_rndis_stop;
    rndis->base.transmit = usbh_rndis_transmit;
    rndis->base.get_addr = usbh_rndis_get_addr;

    *ret_handle = (iot_eth_driver_t *)&rndis->base;
    return ESP_OK;
}
