/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "tinyusb_net.h"
#include "descriptors_control.h"
#include "usb_descriptors.h"
#include "device/usbd_pvt.h"
#include "esp_check.h"

#define MAC_ADDR_LEN 6

typedef struct packet {
    void *buffer;
    void *buff_free_arg;
    uint16_t len;
    esp_err_t result;
} packet_t;

struct tinyusb_net_handle {
    bool initialized;
    SemaphoreHandle_t buffer_sema;
    EventGroupHandle_t  tx_flags;
    tusb_net_rx_cb_t    rx_cb;
    tusb_net_free_tx_cb_t tx_buff_free_cb;
    tusb_net_init_cb_t init_cb;
    char mac_str[2 * MAC_ADDR_LEN + 1];
    void *ctx;
    packet_t *packet_to_send;
};

const static int TX_FINISHED_BIT = BIT0;
static struct tinyusb_net_handle s_net_obj = { };
static const char *TAG = "tusb_net";

static void do_send_sync(void *ctx)
{
    (void) ctx;
    if (xSemaphoreTake(s_net_obj.buffer_sema, 0) != pdTRUE || s_net_obj.packet_to_send == NULL) {
        return;
    }

    packet_t *packet = s_net_obj.packet_to_send;
    if (tud_network_can_xmit(packet->len)) {
        tud_network_xmit(packet, packet->len);
        packet->result = ESP_OK;
    } else {
        packet->result = ESP_FAIL;
    }
    xSemaphoreGive(s_net_obj.buffer_sema);
    xEventGroupSetBits(s_net_obj.tx_flags, TX_FINISHED_BIT);
}

static void do_send_async(void *ctx)
{
    packet_t *packet = ctx;
    if (tud_network_can_xmit(packet->len)) {
        tud_network_xmit(packet, packet->len);
    } else if (s_net_obj.tx_buff_free_cb) {
        ESP_LOGW(TAG, "Packet cannot be accepted on USB interface, dropping");
        s_net_obj.tx_buff_free_cb(packet->buff_free_arg, s_net_obj.ctx);
    }
    free(packet);
}

esp_err_t tinyusb_net_send_async(void *buffer, uint16_t len, void *buff_free_arg)
{
    if (!tud_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    packet_t *packet = calloc(1, sizeof(packet_t));
    packet->len = len;
    packet->buffer = buffer;
    packet->buff_free_arg = buff_free_arg;
    ESP_RETURN_ON_FALSE(packet, ESP_ERR_NO_MEM, TAG, "Failed to allocate packet to send");
    usbd_defer_func(do_send_async, packet, false);
    return ESP_OK;
}

esp_err_t tinyusb_net_send_sync(void *buffer, uint16_t len, void *buff_free_arg, TickType_t  timeout)
{
    if (!tud_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    // Lazy init the flags and semaphores, as they might not be needed (if async approach is used)
    if (!s_net_obj.tx_flags) {
        s_net_obj.tx_flags = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_net_obj.tx_flags, ESP_ERR_NO_MEM, TAG, "Failed to allocate event flags");
    }
    if (!s_net_obj.buffer_sema) {
        s_net_obj.buffer_sema = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_net_obj.buffer_sema, ESP_ERR_NO_MEM, TAG, "Failed to allocate buffer semaphore");
    }

    packet_t packet = {
        .buffer = buffer,
        .len = len,
        .buff_free_arg = buff_free_arg
    };
    s_net_obj.packet_to_send = &packet;
    xSemaphoreGive(s_net_obj.buffer_sema);  // now the packet is ready, let's mark it available to tusb send

    // to execute the send function in tinyUSB task context
    usbd_defer_func(do_send_sync, NULL, false);  // arg=NULL -> sync send, we keep the packet inside the object

    // wait wor completion with defined timeout
    EventBits_t bits = xEventGroupWaitBits(s_net_obj.tx_flags, TX_FINISHED_BIT, pdTRUE, pdTRUE, timeout);
    xSemaphoreTake(s_net_obj.buffer_sema, portMAX_DELAY);   // if tusb sending already started, we have wait before ditching the packet
    s_net_obj.packet_to_send = NULL;        // invalidate the argument
    if (bits & TX_FINISHED_BIT) {   // If transaction finished, return error code
        return packet.result;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev, const tinyusb_net_config_t *cfg)
{
    (void) usb_dev;

    ESP_RETURN_ON_FALSE(s_net_obj.initialized == false, ESP_ERR_INVALID_STATE, TAG, "TinyUSB Net class is already initialized");

    // the semaphore and event flags are initialized only if needed
    s_net_obj.rx_cb = cfg->on_recv_callback;
    s_net_obj.init_cb = cfg->on_init_callback;
    s_net_obj.tx_buff_free_cb = cfg->free_tx_buffer;
    s_net_obj.ctx = cfg->user_context;

    const uint8_t *mac = &cfg->mac_addr[0];
    snprintf(s_net_obj.mac_str, sizeof(s_net_obj.mac_str), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    uint8_t mac_id = tusb_get_mac_string_id();
    // Pass it to Descriptor control module
    tinyusb_set_str_descriptor(s_net_obj.mac_str, mac_id);

    s_net_obj.initialized = true;

    return ESP_OK;
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (s_net_obj.rx_cb) {
        s_net_obj.rx_cb((void *)src, size, s_net_obj.ctx);
    }
    tud_network_recv_renew();
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    packet_t *packet = ref;
    uint16_t len = arg;

    memcpy(dst, packet->buffer, packet->len);
    if (s_net_obj.tx_buff_free_cb) {
        s_net_obj.tx_buff_free_cb(packet->buff_free_arg, s_net_obj.ctx);
    }
    return len;
}

void tud_network_init_cb(void)
{
    if (s_net_obj.init_cb) {
        s_net_obj.init_cb(s_net_obj.ctx);
    }
}
