/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "tinyusb_net.h"
#include "descriptors_control.h"
#include "usb_descriptors.h"
#include "device/usbd_pvt.h"
#include "esp_check.h"

#define MAC_ADDR_LEN 6
#define TINYUSB_NET_TX_QUEUE_LENGTH 16

typedef struct packet {
    void *buffer;
    void *buff_free_arg;
    uint16_t len;
    esp_err_t result;
} packet_t;

struct tinyusb_net_handle {
    bool initialized;
    tusb_net_rx_cb_t rx_cb;
    tusb_net_free_tx_cb_t tx_buff_free_cb;
    tusb_net_init_cb_t init_cb;
    char mac_str[2 * MAC_ADDR_LEN + 1];
    void *ctx;
    QueueHandle_t tx_packet_queue;
    QueueHandle_t link_state_queue;
    packet_t packet_to_send;
};

static struct tinyusb_net_handle s_net_obj = { };
static const char *TAG = "tusb_net";

esp_err_t tinyusb_net_send(void *buffer, uint16_t len, void *buff_free_arg)
{
    ESP_RETURN_ON_FALSE(s_net_obj.initialized && s_net_obj.tx_packet_queue,
                        ESP_ERR_INVALID_STATE, TAG, "TinyUSB Net class is not initialized");

    packet_t packet = {
        .buffer = buffer,
        .buff_free_arg = buff_free_arg,
        .len = len,
    };
    ESP_RETURN_ON_FALSE(xQueueSend(s_net_obj.tx_packet_queue, &packet, 0) == pdTRUE,
                        ESP_ERR_NO_MEM, TAG, "TX packet queue is full");
    return ESP_OK;
}

void tinyusb_net_set_link_state(bool connected)
{
    if (s_net_obj.link_state_queue) {
        xQueueOverwrite(s_net_obj.link_state_queue, &connected);
    }
}

void tinyusb_net_process_tx_queue(void)
{
    if (!s_net_obj.initialized || !s_net_obj.tx_packet_queue) {
        return;
    }

    packet_t packet;
    if (!tud_ready()) {
        // Match the synchronous API behavior: reject packets when USB is not ready.
        while (xQueueReceive(s_net_obj.tx_packet_queue, &packet, 0) == pdTRUE) {
            if (s_net_obj.tx_buff_free_cb) {
                s_net_obj.tx_buff_free_cb(packet.buff_free_arg, s_net_obj.ctx);
            }
        }
        return;
    }

    bool link_connected;
    if (xQueueReceive(s_net_obj.link_state_queue, &link_connected, 0) == pdTRUE) {
        tud_network_link_state(0, link_connected);
    }

    if (xQueuePeek(s_net_obj.tx_packet_queue, &packet, 0) != pdTRUE ||
            !tud_network_can_xmit(packet.len)) {
        return;
    }

    if (xQueueReceive(s_net_obj.tx_packet_queue, &packet, 0) == pdTRUE) {
        s_net_obj.packet_to_send = packet;
        tud_network_xmit(&s_net_obj.packet_to_send, s_net_obj.packet_to_send.len);
    }
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

    QueueHandle_t tx_packet_queue = xQueueCreate(TINYUSB_NET_TX_QUEUE_LENGTH, sizeof(packet_t));
    QueueHandle_t link_state_queue = xQueueCreate(1, sizeof(bool));
    if (!tx_packet_queue || !link_state_queue) {
        if (tx_packet_queue) {
            vQueueDelete(tx_packet_queue);
        }
        if (link_state_queue) {
            vQueueDelete(link_state_queue);
        }
        return ESP_ERR_NO_MEM;
    }
    s_net_obj.tx_packet_queue = tx_packet_queue;
    s_net_obj.link_state_queue = link_state_queue;

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
