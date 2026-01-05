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
    uint8_t cdc_dev_addr;
    usbh_cdc_port_handle_t cdc_port;
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

static esp_err_t usbh_rndis_init_msg_request(usbh_rndis_t *rndis)
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
    usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0, sizeof(rndis_initialize_msg_t), (uint8_t *)cmd);

    resp = (rndis_initialize_cmplt_t *)data;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read response %s", esp_err_to_name(ret));
        return ret;
    }

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
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_initialize_msg_t) + query_len, data);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send query message");

    resp = (rndis_query_cmplt_t *)data;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read response");
    memcpy(info, ((uint8_t *)resp + sizeof(rndis_query_cmplt_t)), resp->InformationBufferLength);
    *info_len = resp->InformationBufferLength;
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
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_set_msg_t) + info_len, data);

    resp = (rndis_set_cmplt_t *)data;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
    ESP_LOGD(TAG, "resp->Status: %"PRIu32"", resp->Status);
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
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                       sizeof(rndis_keepalive_msg_t), data);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send keepalive message");

    resp = (rndis_keepalive_cmplt_t *)data;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = usbh_cdc_send_custom_request(rndis->cdc_port, req_type, CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, 256, (uint8_t *)resp);
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
            bool connect_status = (NDIS_MEDIA_STATE_CONNECTED == data[0]);
            if (connect_status != rndis->connect_status) {
                rndis->connect_status = connect_status;
                xEventGroupSetBits(rndis->event_group, RNDIS_LINE_CHANGE);
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
            ESP_LOGD(TAG, "Ignore rndis query iod:%08x", oid);
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

    return ret;
query_errorout:
    ESP_LOGE(TAG, "rndis query iod:%08x error", oid);
    return ret;
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
    esp_err_t ret = ESP_OK;

    hdr = (rndis_data_packet_t *)malloc(sizeof(rndis_data_packet_t) + buflen);
    ESP_RETURN_ON_FALSE(hdr != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for RNDIS data packet");

    hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = sizeof(rndis_data_packet_t) + buflen;
    hdr->DataOffset = sizeof(rndis_data_packet_t) - sizeof(rndis_generic_msg_t);
    hdr->DataLength = buflen;

    len = hdr->MessageLength;
    ESP_LOGD(TAG, "rndis tx length %"PRIu32"", len);
    memcpy((uint8_t *)hdr + sizeof(rndis_data_packet_t), buffer, buflen);

    ESP_GOTO_ON_ERROR(usbh_cdc_write_bytes(rndis->cdc_port, (uint8_t *)hdr, len, pdMS_TO_TICKS(500)),
                      err, TAG, "Failed to write data to RNDIS");
err:
    free(hdr);
    return ret;
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

static void _usbh_rndis_recv_data_cb(usbh_cdc_port_handle_t cdc_port_handle, void *arg)
{
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    esp_err_t ret = ESP_OK;
    rndis_data_packet_t pmsg;
    size_t rx_length = 0;
    const size_t head_len = sizeof(rndis_data_packet_t);
    static size_t last_data_len = 0;
    while (1) {
        if (last_data_len == 0) {
            // read the header first
            usbh_cdc_get_rx_buffer_size(cdc_port_handle, &rx_length);
            if (rx_length <= head_len) {
                return; // wait for the full packet to be received
            }
            rx_length = head_len;
            ret = usbh_cdc_read_bytes(cdc_port_handle, (uint8_t *)&pmsg, &rx_length, pdMS_TO_TICKS(10));
            ESP_RETURN_ON_FALSE(ret == ESP_OK,, TAG, "Failed to read data from RNDIS (%s)", esp_err_to_name(ret));
            if (pmsg.MessageType != REMOTE_NDIS_PACKET_MSG) {
                ESP_LOGE(TAG, "Unknown rndis packet message type: 0x%lx, msglength: %lu, data offset: %lu, data length: %lu, rx_length: %d",
                         pmsg.MessageType, pmsg.MessageLength, pmsg.DataOffset, pmsg.DataLength, rx_length);
                usbh_cdc_flush_rx_buffer(cdc_port_handle);
                return; // Not a RNDIS data packet, ignore
            }
        } else {
            pmsg.DataLength = last_data_len;
            last_data_len = 0;
        }
        // check if the full packet is received
        usbh_cdc_get_rx_buffer_size(cdc_port_handle, &rx_length);
        if (rx_length >= pmsg.DataLength) { // if the ring buffer has enough data for the full packet
            uint8_t *data = malloc(pmsg.DataLength);
            ESP_RETURN_VOID_ON_FALSE(data != NULL, TAG, "Failed to allocate memory for data buffer");
            size_t data_len = pmsg.DataLength;
            ESP_GOTO_ON_ERROR(usbh_cdc_read_bytes(cdc_port_handle, data, &data_len, pdMS_TO_TICKS(10)), err, TAG, "Failed to read data from RNDIS (%s)", esp_err_to_name(ret));
            assert(data_len == pmsg.DataLength);
            rndis->mediator->stack_input(rndis->mediator, data, data_len);
            continue;
err:
            free(data);
        } else {
            last_data_len = pmsg.DataLength; // indicate that the full packet is not received yet. next loop will read the rest of the data
            ESP_LOGD(TAG, "Not enough data rx_length: %d, pmsg.DataLength: %lu\n", rx_length, pmsg.DataLength);
            break; // Not enough data in the ring buffer, wait for more data
        }
    }
}

static void _usbh_rndis_notif_cb(usbh_cdc_port_handle_t cdc_port_handle, iot_cdc_notification_t *notif, void *arg)
{
    switch (notif->bNotificationCode) {
    case USB_CDC_NOTIFY_NETWORK_CONNECTION:
        bool connected = notif->wValue;
        ESP_LOGD(TAG, "Notify - network connection changed: %s", connected ? "Connected" : "Disconnected");
        break;
    case USB_CDC_NOTIFY_SPEED_CHANGE: {
        uint32_t *speeds = (uint32_t *)notif->Data;
        ESP_LOGI(TAG, "Notify - link speeds: %lu kbps ↑, %lu kbps ↓", (speeds[0]) / 1000, (speeds[1]) / 1000);
        break;
    }
    default:
        ESP_LOGW(TAG, "unexpected notification %02x!", notif->bNotificationCode);
        return;
    }
}

static void _usbh_rndis_dev_event_cb(usbh_cdc_device_event_t event, usbh_cdc_device_event_data_t *event_data, void *user_ctx)
{
    switch (event) {
    case CDC_HOST_DEVICE_EVENT_CONNECTED: {
        usbh_rndis_t *rndis = (usbh_rndis_t *)user_ctx;
        if (rndis->cdc_port != NULL) {
            ESP_LOGD(TAG, "RNDIS port already opened, ignore this event");
            return;
        }
        const usb_config_desc_t *config_desc = event_data->new_dev.active_config_desc;
        const usb_device_desc_t *device_desc = event_data->new_dev.device_desc;

        int itf_num = -1;
        esp_err_t ret = usbh_rndis_interface_check(device_desc, config_desc, &itf_num);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "No RNDIS interface found for device VID: %04X, PID: %04X, ignore it", device_desc->idVendor, device_desc->idProduct);
            return;
        }
        ESP_LOGI(TAG, "RNDIS device found: VID: %04X, PID: %04X, IFNUM: %d", device_desc->idVendor, device_desc->idProduct, itf_num);

        usbh_cdc_port_config_t cdc_port_config = {
            .dev_addr = event_data->new_dev.dev_addr,
            .itf_num = itf_num,
            .in_transfer_buffer_size = 512 * 4,
            .out_transfer_buffer_size = 512 * 3,
            .in_ringbuf_size = 4096, // enable ring buffer to handle packet sticking
            .cbs = {
                .notif_cb = _usbh_rndis_notif_cb,
                .recv_data = _usbh_rndis_recv_data_cb,
                .user_data = rndis,
            },
#ifdef CONFIG_RNDIS_DISABLE_CDC_NOTIFICATION
            .flags = USBH_CDC_FLAGS_DISABLE_NOTIFICATION,
#endif
        };
        ESP_RETURN_VOID_ON_ERROR(usbh_cdc_port_open(&cdc_port_config, &rndis->cdc_port), TAG, "Failed to open CDC port");

        rndis->cdc_dev_addr = event_data->new_dev.dev_addr;
        xEventGroupSetBits(rndis->event_group, RNDIS_CONNECTED);
    } break;

    case CDC_HOST_DEVICE_EVENT_DISCONNECTED: {
        usbh_rndis_t *rndis = (usbh_rndis_t *)user_ctx;
        if (rndis->cdc_dev_addr == event_data->dev_gone.dev_addr) {
            rndis->cdc_dev_addr = 0;
            rndis->connect_status = false;
            rndis->cdc_port = NULL;
            xEventGroupSetBits(rndis->event_group, RNDIS_DISCONNECTED);
            ESP_LOGI(TAG, "RNDIS disconnected");
        }
    } break;
    default:
        break;
    }
}

static void _usbh_rndis_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    uint32_t delay_time_ms = 500;
    while (1) {
        uint32_t events = xEventGroupWaitBits(rndis->event_group, RNDIS_EVENT_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(delay_time_ms));
        if (events & RNDIS_CONNECTED) {
            ret = usbh_rndis_connect(rndis);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "RNDIS connected success");
            } else {
                ESP_LOGE(TAG, "RNDIS connected failed, please reconnect");
            }
        }
        if (events & RNDIS_DISCONNECTED) {
            rndis->request_id = 0;
            iot_eth_link_t link = IOT_ETH_LINK_DOWN;
            rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LINK, &link);
            delay_time_ms = 500;
        }
        if (events & RNDIS_LINE_CHANGE) {
            ESP_LOGI(TAG, "RNDIS line change: %d", rndis->connect_status);
            iot_eth_link_t link = rndis->connect_status ? IOT_ETH_LINK_UP : IOT_ETH_LINK_DOWN;
            rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LINK, &link);
            delay_time_ms = rndis->connect_status ? 5000 : 500;
        }
        if (events & RNDIS_STOP) {
            ESP_LOGI(TAG, "RNDIS stop");
            break;
        }
        if (events == 0 && rndis->cdc_port != NULL) {
            ret = usbh_rndis_get_connect_status(rndis);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to get connect status");
                continue;
            }
            // Timeout occurred, send keepalive
            ESP_LOGD(TAG, "No events for 5s, sending keepalive");
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
static esp_err_t usbh_rndis_init(iot_eth_driver_t *driver)
{
    esp_err_t ret = ESP_OK;
    usbh_rndis_t *rndis = __containerof(driver, usbh_rndis_t, base);

    rndis->event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(rndis->event_group != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create event group");

    // Generate a MAC address for the interface
    esp_read_mac(rndis->mac_addr, ESP_MAC_ETH);
    rndis->mac_addr[5] ^= 0x01; // Make it unique from the default Ethernet MAC

    ret = usbh_cdc_register_dev_event_cb(rndis->config.match_id_list, _usbh_rndis_dev_event_cb, rndis);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to register CDC device event callback");
    BaseType_t task_created = xTaskCreate(_usbh_rndis_task, "_usbh_rndis_task", 4096, rndis, 5, NULL);
    ESP_GOTO_ON_FALSE(task_created == pdPASS, ESP_FAIL, err_task, TAG, "Failed to create USB RNDIS task");

    rndis->mediator->on_stage_changed(rndis->mediator, IOT_ETH_STAGE_LL_INIT, NULL);
    ESP_LOGI(TAG, "USB RNDIS network interface init success");
    return ESP_OK;

err_task:
    usbh_cdc_unregister_dev_event_cb(_usbh_rndis_dev_event_cb);
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

    xEventGroupSetBits(rndis->event_group, RNDIS_STOP);
    xEventGroupWaitBits(rndis->event_group, RNDIS_STOP_DONE, pdTRUE, pdFALSE, portMAX_DELAY);

    ret = usbh_cdc_unregister_dev_event_cb(_usbh_rndis_dev_event_cb);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to unregister CDC device event callback");

    vEventGroupDelete(rndis->event_group);
    free(rndis);
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
    rndis->base.start = NULL;
    rndis->base.stop = NULL;
    rndis->base.transmit = usbh_rndis_transmit;
    rndis->base.get_addr = usbh_rndis_get_addr;

    *ret_handle = (iot_eth_driver_t *)&rndis->base;
    return ESP_OK;
}

usbh_cdc_port_handle_t usb_rndis_get_cdc_port_handle(const iot_eth_driver_t *rndis_drv)
{
    ESP_RETURN_ON_FALSE(rndis_drv != NULL, NULL, TAG, "rndis_drv is NULL");
    usbh_rndis_t *rndis = __containerof(rndis_drv, usbh_rndis_t, base);
    return rndis->cdc_port;
}
