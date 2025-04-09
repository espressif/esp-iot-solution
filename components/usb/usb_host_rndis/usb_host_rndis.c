/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "usbh_rndis_protocol.h"
#include "usb/cdc_acm_host.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "usb_host_rndis.h"

static const char *TAG = "usbh_rndis";

#define USBH_RNDIS_MSG_BUF_SIZE 1024

#define CONFIG_USBHOST_RNDIS_ETH_MAX_FRAME_SIZE 1514
#define CONFIG_USBHOST_RNDIS_ETH_MSG_SIZE       (CONFIG_USBHOST_RNDIS_ETH_MAX_FRAME_SIZE + 44)

#define CONFIG_USBHOST_RNDIS_ETH_MAX_RX_SIZE (2048)
#define CONFIG_USBHOST_RNDIS_ETH_MAX_TX_SIZE (2048)

static uint8_t g_rndis_rx_buffer[CONFIG_USBHOST_RNDIS_ETH_MAX_RX_SIZE];
static uint8_t g_rndis_tx_buffer[CONFIG_USBHOST_RNDIS_ETH_MAX_TX_SIZE];

typedef struct {
    esp_netif_t *netif;
    cdc_acm_dev_hdl_t cdc_dev;
    uint8_t minor;
    uint32_t request_id;
    uint32_t tx_offset;
    uint32_t max_transfer_pkts; /* max packets in one transfer */
    uint32_t max_transfer_size; /* max size in one transfer */
    uint32_t link_speed;
    bool connect_status;
    uint8_t mac[6];
    uint8_t *rndis_msg_buf;
    size_t rndis_msg_buf_len;
    RingbufHandle_t in_ringbuf_handle;   /*!< in ringbuffer handle of corresponding interface */
    QueueHandle_t in_queue_handle;   /*!< in queue handle of corresponding interface */
    QueueHandle_t tx_queue_handle;   /*!< in queue handle of corresponding interface */
    size_t in_ringbuf_size;

    void *user_data;
} usbh_rndis_t;

// Structure for TX packets
typedef struct {
    void *buffer;
    uint32_t length;
} usb_ecm_tx_packet_t;

static usbh_rndis_t *rndis = NULL;

/*--------------------------------- CDC Buffer Handle Code --------------------------------------*/
static size_t _get_ringbuf_len(RingbufHandle_t ringbuf_hdl)
{
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    return uxItemsWaiting;
}

static esp_err_t _ringbuf_pop(RingbufHandle_t ringbuf_hdl, uint8_t *buf, size_t req_bytes, size_t *read_bytes, TickType_t ticks_to_wait)
{
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, read_bytes, ticks_to_wait, req_bytes);
    if (!buf_rcv) {
        return ESP_FAIL;
    }

    memcpy(buf, buf_rcv, *read_bytes);
    vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));

    size_t read_bytes2 = 0;
    if (*read_bytes < req_bytes) {
        buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, &read_bytes2, 0, req_bytes - *read_bytes);
        if (buf_rcv) {
            memcpy(buf + *read_bytes, buf_rcv, read_bytes2);
            *read_bytes += read_bytes2;
            vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));
        }
    }

    return ESP_OK;
}

static esp_err_t _ringbuf_push(RingbufHandle_t ringbuf_hdl, const uint8_t *buf, size_t write_bytes, TickType_t ticks_to_wait)
{
    int res = xRingbufferSend(ringbuf_hdl, buf, write_bytes, ticks_to_wait);

    if (res != pdTRUE) {
        ESP_LOGW(TAG, "The in buffer is too small, the data has been lost");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void _ring_buffer_flush(RingbufHandle_t ringbuf_hdl)
{
    assert(ringbuf_hdl);
    size_t read_bytes = 0;
    size_t uxItemsWaiting = 0;
    vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    uint8_t *buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, &read_bytes, 0, uxItemsWaiting);

    if (buf_rcv) {
        vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));
    }

    if (uxItemsWaiting > read_bytes) {
        // read the second time to flush all data
        vRingbufferGetInfo(ringbuf_hdl, NULL, NULL, NULL, NULL, &uxItemsWaiting);
        buf_rcv = xRingbufferReceiveUpTo(ringbuf_hdl, &read_bytes, 0, uxItemsWaiting);
        if (buf_rcv) {
            vRingbufferReturnItem(ringbuf_hdl, (void *)(buf_rcv));
        }
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    // s_got_ip = true;
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

static void usb_rndis_free_rx_buffer(void *h, void *buffer)
{
    if (buffer) {
        free(buffer - 44);
    }
}

#define USB_RNDIS_MSG_BUF_SIZE 512
#define USB_RNDIS_IN_RINGBUF_SIZE 2048*2

esp_err_t usbh_rndis_init(const usb_host_rndis_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    // Init TCP/IP network interface (should be called only once in application)
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to initialize TCP/IP stack");
        return ret;
    }

    // Create default event loop that runs in background if not already created
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { // Already initialized is OK
        ESP_LOGE(TAG, "Failed to create event loop");
        return ret;
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    rndis = calloc(1, sizeof(usbh_rndis_t));
    ESP_RETURN_ON_FALSE(rndis != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for rndis");

    rndis->rndis_msg_buf = calloc(1, USB_RNDIS_MSG_BUF_SIZE);
    rndis->rndis_msg_buf_len = USB_RNDIS_MSG_BUF_SIZE;
    ESP_RETURN_ON_FALSE(rndis->rndis_msg_buf != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for rndis_msg_buf");

    rndis->in_ringbuf_size = USB_RNDIS_IN_RINGBUF_SIZE;
    rndis->in_ringbuf_handle = xRingbufferCreate(rndis->in_ringbuf_size, RINGBUF_TYPE_BYTEBUF);
    ESP_RETURN_ON_FALSE(rndis->in_ringbuf_handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create ring buffer");
    rndis->in_queue_handle = xQueueCreate(10, sizeof(size_t));
    ESP_RETURN_ON_FALSE(rndis->in_queue_handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create queue");

    rndis->tx_queue_handle = xQueueCreate(10, sizeof(usb_ecm_tx_packet_t));
    ESP_RETURN_ON_FALSE(rndis->tx_queue_handle != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create queue");

    // Create network interface for USB ECM with default config
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    rndis->netif = esp_netif_new(&netif_cfg);
    if (!rndis->netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return ESP_ERR_NO_MEM;
    }

    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = rndis->netif,                // not using an instance, USB-NCM is a static singleton (must be != NULL)
        .transmit = usbh_rndis_eth_output,  // point to static Tx function
        .driver_free_rx_buffer = usb_rndis_free_rx_buffer,    // point to Free Rx buffer function
    };

    // Set the driver configuration for the netif
    ret = esp_netif_set_driver_config(rndis->netif, &driver_cfg);
    if (ret != ESP_OK) {
        esp_netif_destroy(rndis->netif);
        rndis->netif = NULL;
        return ret;
    }

    // Generate a MAC address for the interface
    uint8_t mac_addr[6];
    // uint8_t mac_addr[6] = {  0x01, 0x01, 0x5E, 0x01, 0x01, 0x01 };

    esp_read_mac(mac_addr, ESP_MAC_ETH);
    mac_addr[5] ^= 0x01; // Make it unique from the default Ethernet MAC
    ESP_ERROR_CHECK(esp_netif_set_mac(rndis->netif, mac_addr));

    ESP_LOGI(TAG, "USB RNDIS MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    ESP_LOGI(TAG, "USB RNDIS network interface initialized");
    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
    return ESP_OK;
}

/**
 * @brief Data received callback
 *
 * @param[in] data     Pointer to received data
 * @param[in] data_len Length of received data in bytes
 * @param[in] arg      Argument we passed to the device open function
 * @return
 *   true:  We have processed the received data
 *   false: We expect more data
 */
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    usbh_rndis_t *rndis = (usbh_rndis_t *)arg;
    if (_ringbuf_push(rndis->in_ringbuf_handle, data, data_len, pdMS_TO_TICKS(1000)) == ESP_OK) {
        xQueueSend(rndis->in_queue_handle, &data_len, pdMS_TO_TICKS(1000));
    }

    return true;
}

/**
 * @brief Device event callback
 *
 * Apart from handling device disconnection it doesn't do anything useful
 *
 * @param[in] event    Device event type and data
 * @param[in] user_ctx Argument we passed to the device open function
 */
static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        // xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
        break;
    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

// USB ECM transmit task
static void usb_rndis_tx_task(void *arg)
{
    usb_ecm_tx_packet_t tx_packet;
    printf("usb_rndis_tx_task started\n");
    while (1) {
        if (xQueueReceive(rndis->tx_queue_handle, &tx_packet, portMAX_DELAY) == pdTRUE) {
            // if (!s_cdc_dev) {
            //     // Device disconnected, free buffer and continue
            //     free(tx_packet.buffer);
            //     continue;
            // }

            // Send packet through USB ECM interface
            ESP_LOGD(TAG, "Transmitting packet, len=%lu", tx_packet.length);
            esp_err_t ret = cdc_acm_host_data_tx_blocking(rndis->cdc_dev, tx_packet.buffer,
                                                          tx_packet.length, 1000);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to transmit packet: %s", esp_err_to_name(ret));
            }

            // Free the buffer after transmitting
            free(tx_packet.buffer);
        }
    }
}

esp_err_t usbh_rndis_create(void)
{
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 2048,
        // TODO: make this configurable
        .in_buffer_size = 2048,
        .event_cb = handle_event,
        .data_cb = handle_rx,
        .user_arg = rndis,
    };

    while (1) {
        // Open USB device from tusb_serial_device example example. Either single or dual port configuration.
        ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", 0, 0);
        esp_err_t err = cdc_acm_host_open(0, 0, 0, &dev_config, &rndis->cdc_dev);
        if (ESP_OK != err) {
            ESP_LOGI(TAG, "Fail to open devcive");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        break;
    }
    cdc_acm_host_desc_print(rndis->cdc_dev);
    xTaskCreate(usb_rndis_tx_task, "usb_rndis_tx_task", 4096, rndis, 10, NULL);
    return ESP_OK;
}

esp_err_t usbh_rndis_init_msg_transfer(usbh_rndis_t * rndis_class)
{
    esp_err_t ret = ESP_OK;
    rndis_initialize_msg_t *cmd;
    rndis_initialize_cmplt_t *resp;

    cmd = (rndis_initialize_msg_t *)rndis_class->rndis_msg_buf;
    cmd->MessageType = REMOTE_NDIS_INITIALIZE_MSG;
    cmd->MessageLength = sizeof(rndis_initialize_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->MajorVersion = 0x1;
    cmd->MinorVersion = 0x0;
    cmd->MaxTransferSize = 0x4000;

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    cdc_acm_host_send_custom_request(rndis_class->cdc_dev, req_type, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0, sizeof(rndis_initialize_msg_t), (uint8_t *)cmd);

    resp = (rndis_initialize_cmplt_t *)rndis_class->rndis_msg_buf;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, USB_RNDIS_MSG_BUF_SIZE - 9, (uint8_t *)resp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read response %s", esp_err_to_name(ret));
        return ret;
    }

    rndis_class->max_transfer_pkts = resp->MaxPacketsPerTransfer;
    rndis_class->max_transfer_size = resp->MaxTransferSize;
    ESP_LOGI(TAG, "Max transfer packets: %"PRIu32"", rndis_class->max_transfer_pkts);
    ESP_LOGI(TAG, "Max transfer size: %"PRIu32"", rndis_class->max_transfer_size);

    return ESP_OK;
}

esp_err_t usbh_rndis_query_msg_transfer(usbh_rndis_t *rndis_class, uint32_t oid, uint32_t query_len, uint8_t *info, uint32_t *info_len)
{
    esp_err_t ret = ESP_OK;
    rndis_query_msg_t *cmd;
    rndis_query_cmplt_t *resp;

    // TODO: maybe need clear ringbuffer here?

    cmd = (rndis_query_msg_t *)rndis_class->rndis_msg_buf;

    cmd->MessageType = REMOTE_NDIS_QUERY_MSG;
    cmd->MessageLength = query_len + sizeof(rndis_query_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = query_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;
    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                           sizeof(rndis_initialize_msg_t) + query_len, rndis->rndis_msg_buf);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send query message");

    resp = (rndis_query_cmplt_t *)rndis_class->rndis_msg_buf;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, USB_RNDIS_MSG_BUF_SIZE - 9, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to read response");
    memcpy(info, ((uint8_t *)resp + sizeof(rndis_query_cmplt_t)), resp->InformationBufferLength);
    *info_len = resp->InformationBufferLength;
    return ret;
}

static esp_err_t usbh_rndis_set_msg_transfer(usbh_rndis_t *rndis_class, uint32_t oid, uint8_t *info, uint32_t info_len)
{
    esp_err_t ret = 0;
    rndis_set_msg_t *cmd;
    rndis_set_cmplt_t *resp;

    // TODO: maybe need clear ringbuffer here?

    cmd = (rndis_set_msg_t *)rndis_class->rndis_msg_buf;

    cmd->MessageType = REMOTE_NDIS_SET_MSG;
    cmd->MessageLength = info_len + sizeof(rndis_set_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = info_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;

    memcpy(((uint8_t *)cmd + sizeof(rndis_set_msg_t)), info, info_len);

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                           sizeof(rndis_set_msg_t) + info_len, rndis->rndis_msg_buf);

    resp = (rndis_set_cmplt_t *)rndis_class->rndis_msg_buf;
    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, USB_RNDIS_MSG_BUF_SIZE - 9, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "oid:%08x recv error, ret: %s\r\n", (unsigned int)oid, esp_err_to_name(ret));
    return ret;
}

esp_err_t usbh_rndis_get_connect_status(usbh_rndis_t *rndis_class)
{
    esp_err_t ret = ESP_OK;
    uint8_t data[32];
    uint32_t data_len = 0;

    ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to query OID_GEN_MEDIA_CONNECT_STATUS");
    if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
        rndis_class->connect_status = true;
    } else {
        rndis_class->connect_status = false;
    }

    return ret;
}

esp_err_t usbh_rndis_keepalive(void)
{
    esp_err_t ret = ESP_OK;
    rndis_keepalive_msg_t *cmd;
    rndis_keepalive_cmplt_t *resp;

    usbh_rndis_t *rndis_class = rndis;

    cmd = (rndis_keepalive_msg_t *)rndis_class->rndis_msg_buf;

    cmd->MessageType = REMOTE_NDIS_KEEPALIVE_MSG;
    cmd->MessageLength = sizeof(rndis_keepalive_msg_t);
    cmd->RequestId = rndis_class->request_id++;

    uint8_t req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_OUT;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND, 0, 0,
                                           sizeof(rndis_keepalive_msg_t), rndis->rndis_msg_buf);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to send keepalive message");

    resp = (rndis_keepalive_cmplt_t *)rndis_class->rndis_msg_buf;

    req_type = USB_BM_REQUEST_TYPE_TYPE_CLASS | USB_BM_REQUEST_TYPE_RECIP_INTERFACE | USB_BM_REQUEST_TYPE_DIR_IN;
    ret = cdc_acm_host_send_custom_request(rndis->cdc_dev, req_type, USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE, 0, 0, USB_RNDIS_MSG_BUF_SIZE - 9, (uint8_t *)resp);
    ESP_RETURN_ON_ERROR(ret, TAG, "rndis_keepalive error, ret: %s\r\n", esp_err_to_name(ret));
    return ret;
}

esp_err_t usbh_rndis_open(void)
{
    esp_err_t ret = ESP_OK;
    uint32_t *oid_support_list;
    unsigned int oid = 0;
    unsigned int oid_num = 0;
    uint8_t tmp_buffer[512];
    uint8_t data[32];
    uint32_t data_len;
    ESP_RETURN_ON_FALSE(rndis, ESP_ERR_INVALID_STATE, TAG, "rndis not init\r\n");
    ret = usbh_rndis_init_msg_transfer(rndis);
    if (ret != ESP_OK) {
        return ret;
    }
    ESP_LOGI(TAG, "rndis init success\r\n");

    ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_SUPPORTED_LIST, 0, tmp_buffer, &data_len);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to query OID_GEN_SUPPORTED_LIST");

    oid_num = (data_len / 4);
    ESP_LOGI(TAG, "rndis query OID_GEN_SUPPORTED_LIST success,oid num :%d\n", oid_num);

    oid_support_list = (uint32_t *)tmp_buffer;
    for (int i = 0; i < oid_num; i++) {
        oid = oid_support_list[i];
        switch (oid) {
        case OID_GEN_PHYSICAL_MEDIUM:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_PHYSICAL_MEDIUM, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
            break;
        case OID_GEN_MAXIMUM_FRAME_SIZE:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_MAXIMUM_FRAME_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_MAXIMUM_FRAME_SIZE query error, ret: %s\r\n", esp_err_to_name(ret));
            break;
        case OID_GEN_LINK_SPEED:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_LINK_SPEED, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_LINK_SPEED query error, ret: %s\r\n", esp_err_to_name(ret));
            memcpy(&rndis->link_speed, data, 4);
            break;
        case OID_GEN_MEDIA_CONNECT_STATUS:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_GEN_MEDIA_CONNECT_STATUS query error, ret: %s\r\n", esp_err_to_name(ret));
            if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
                rndis->connect_status = true;
            } else {
                rndis->connect_status = false;
            }
            break;
        case OID_802_3_MAXIMUM_LIST_SIZE:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_802_3_MAXIMUM_LIST_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_802_3_MAXIMUM_LIST_SIZE query error, ret: %s\r\n", esp_err_to_name(ret));
            break;
        case OID_802_3_CURRENT_ADDRESS:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_802_3_CURRENT_ADDRESS, 6, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG, "OID_802_3_CURRENT_ADDRESS query error, ret: %s\r\n", esp_err_to_name(ret));

            for (uint8_t j = 0; j < 6; j++) {
                rndis->mac[j] = data[j];
            }
            break;
        case OID_802_3_PERMANENT_ADDRESS:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_802_3_PERMANENT_ADDRESS, 6, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
            break;
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
            ret = usbh_rndis_query_msg_transfer(rndis, OID_GEN_MAXIMUM_TOTAL_SIZE, 4, data, &data_len);
            ESP_GOTO_ON_ERROR(ret, query_errorout, TAG,);
        default:
            ESP_LOGW(TAG, "Ignore rndis query iod:%08x\n", oid);
            continue;
        }
        ESP_LOGI(TAG, "rndis query iod:%08x success\n", oid);
    }

    uint32_t packet_filter = 0x0f;
    ret = usbh_rndis_set_msg_transfer(rndis, OID_GEN_CURRENT_PACKET_FILTER, (uint8_t *)&packet_filter, 4);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OID_GEN_CURRENT_PACKET_FILTER");
    ESP_LOGI(TAG, "rndis set OID_GEN_CURRENT_PACKET_FILTER success\n");

    uint8_t multicast_list[6] = { 0x01, 0x00, 0x5E, 0x00, 0x00, 0x01 };
    ret = usbh_rndis_set_msg_transfer(rndis, OID_802_3_MULTICAST_LIST, multicast_list, 6);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to set OID_802_3_MULTICAST_LIST");
    ESP_LOGI(TAG, "rndis set OID_802_3_MULTICAST_LIST success\r\n");

    ESP_LOGI(TAG, "rndis MAC address %02x:%02x:%02x:%02x:%02x:%02x\r\n",
             rndis->mac[0],
             rndis->mac[1],
             rndis->mac[2],
             rndis->mac[3],
             rndis->mac[4],
             rndis->mac[5]);

    // TODO: set interface name DEV_FORMAT "/dev/rndis"
    // strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    ESP_LOGI(TAG, "Register RNDIS success");
    // usbh_rndis_run(rndis);

    xTaskCreate(usbh_rndis_rx_thread, "usbh_rndis_rx_thread", 4096, rndis->netif, 5, NULL);

    esp_netif_action_start(rndis->netif, NULL, 0, NULL);
    esp_netif_action_connected(rndis->netif, NULL, 0, NULL);
    return ret;
query_errorout:
    ESP_LOGE(TAG, "rndis query iod:%08x error", oid);
    return ret;
}

esp_err_t usbh_rndis_close(void)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(rndis != NULL, ESP_ERR_INVALID_ARG, TAG, "rndis handle is NULL");
    memset(rndis, 0, sizeof(usbh_rndis_t));
    rndis = NULL;

    // TODO: Clear ringbuffer here
    return ret;
}

extern esp_netif_t *ap_netif;

void usbh_rndis_rx_thread(void *arg)
{
    esp_err_t ret;
    uint32_t pmg_offset;
    rndis_data_packet_t *pmsg;
    rndis_data_packet_t temp;
    esp_netif_t *netif = (esp_netif_t *)arg;

    ESP_GOTO_ON_FALSE(rndis != NULL, ESP_ERR_INVALID_ARG, delete, TAG, "rndis handle is NULL");
    ESP_LOGI(TAG, "rndis rx thread start\r\n");

    rndis->connect_status = false;
    while (rndis->connect_status == false) {
        ret = usbh_rndis_get_connect_status(rndis);
        if (ret != ESP_OK) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "rndis rx thread wait connect status\r\n");
    }

    while (1) {
        size_t rx_length = 0;
        xQueueReceive(rndis->in_queue_handle, &rx_length, portMAX_DELAY);
        uint8_t *data_buffer = calloc(1, rx_length);
        assert(data_buffer != NULL);

        if (rx_length <= CONFIG_USBHOST_RNDIS_ETH_MAX_RX_SIZE) {
            pmg_offset = 0;
            uint32_t total_len = rx_length;
            while (rx_length > 0) {
                ESP_LOGD(TAG, "rndis rx thread rx_length %d\r\n", rx_length);
                size_t read_len = 0;
                // TODO: if can get read_len < sizeof(rndis_query_cmplt_t)
                ret = _ringbuf_pop(rndis->in_ringbuf_handle, data_buffer, rx_length, &read_len, pdMS_TO_TICKS(1000));
                if (ret != ESP_OK) {
                    // TODO:
                }

                pmsg = (rndis_data_packet_t *)data_buffer + pmg_offset;

                /* Not word-aligned case */
                if (pmg_offset & 0x3) {
                    memcpy(&temp, pmsg, sizeof(rndis_data_packet_t));
                    pmsg = &temp;
                }

                if (pmsg->MessageType == REMOTE_NDIS_PACKET_MSG) {
                    uint8_t *buf = (uint8_t *)(data_buffer + pmg_offset + sizeof(rndis_generic_msg_t) + pmsg->DataOffset);
                    // uint8_t *buf = (uint8_t *)g_rndis_rx_buffer + pmg_offset + sizeof(rndis_generic_msg_t);

                    pmg_offset += pmsg->MessageLength;
                    rx_length -= pmsg->MessageLength;
                    // ESP_LOG_BUFFER_HEX("rndis_rx", buf, pmsg->DataLength);
                    esp_netif_receive(netif, (void *)(buf), pmsg->DataLength, NULL);
                    // uint8_t *buf = (uint8_t *)g_rndis_rx_buffer + pmg_offset + sizeof(rndis_data_packet_t) + pmsg->DataOffset;
                    // TODO:
                    // usbh_rndis_eth_input(buf, pmsg->DataLength);

                    /* drop the last dummy byte, it is a short packet to tell us we have received a multiple of wMaxPacketSize */
                    if (rx_length < 4) {
                        rx_length = 0;
                    }
                } else {
                    ESP_LOGE(TAG, "offset:%"PRIu32",remain:%d,total:%"PRIu32"\n", pmg_offset, rx_length, total_len);
                    rx_length = 0;
                    ESP_LOGE(TAG, "Error rndis packet message\n");
                }
            }
        } else {
            ESP_LOGE(TAG, "rndis rx length %d is too large\r\n", rx_length);
            continue;
        }
    }

delete:
    vTaskDelete(NULL);
    return;
}

// TODO: check return type
esp_err_t usbh_rndis_eth_output(void *h, void *buffer, size_t buflen)
{
    // ESP_LOG_BUFFER_HEX("rndis tx data", buffer, buflen);
    rndis_data_packet_t *hdr;
    uint32_t len;

    if (rndis->connect_status == false) {
        return ESP_ERR_INVALID_STATE;
    }

    hdr = (rndis_data_packet_t *)malloc(sizeof(rndis_data_packet_t) + buflen);
    // (rndis_data_packet_t *)g_rndis_tx_buffer;
    // memset(hdr, 0, sizeof(rndis_data_packet_t));

    hdr->MessageType = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = sizeof(rndis_data_packet_t) + buflen;
    hdr->DataOffset = sizeof(rndis_data_packet_t) - sizeof(rndis_generic_msg_t);
    hdr->DataLength = buflen;

    len = hdr->MessageLength;
    ESP_LOGD(TAG, "rndis tx length %"PRIu32"", len);
    memcpy((uint8_t *)hdr + sizeof(rndis_data_packet_t), buffer, buflen);
    // ESP_LOG_BUFFER_HEX("rndis tx data", g_rndis_tx_buffer, len);
    // if (len > CONFIG_USBHOST_RNDIS_ETH_MAX_TX_SIZE) {
    //     ESP_LOGE(TAG, "rndis tx length %"PRIu32" is too large", len);
    //     return ESP_ERR_INVALID_SIZE;
    // }

    // Create packet structure
    usb_ecm_tx_packet_t tx_packet = {
        .buffer = hdr,
        .length = len
    };

    // Send packet to transmit queue
    if (xQueueSend(rndis->tx_queue_handle, &tx_packet, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "TX queue full, dropping packet");
        free(hdr);
        return ESP_ERR_TIMEOUT;
    }
    // cdc_acm_host_data_tx_blocking(rndis->cdc_dev, g_rndis_tx_buffer, len, pdMS_TO_TICKS(1000));
    return ESP_OK;
}

uint8_t *usbh_rndis_get_mac(void)
{
    return rndis->mac;
}
