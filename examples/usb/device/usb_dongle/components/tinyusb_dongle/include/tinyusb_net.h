/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "tinyusb_types.h"
#include "esp_err.h"
#include "sdkconfig.h"

#if (CONFIG_TINYUSB_NET_MODE_NONE != 1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief On receive callback type
 */
typedef esp_err_t (*tusb_net_rx_cb_t)(void *buffer, uint16_t len, void *ctx);

/**
 * @brief Free Tx buffer callback type
 */
typedef void (*tusb_net_free_tx_cb_t)(void *buffer, void *ctx);

/**
 * @brief On init callback type
 */
typedef void (*tusb_net_init_cb_t)(void *ctx);

/**
 * @brief ESP TinyUSB NCM driver configuration structure
 */
typedef struct {
    uint8_t mac_addr[6];                      /*!< MAC address. Must be 6 bytes long. */
    tusb_net_rx_cb_t on_recv_callback;        /*!< TinyUSB receive data callbeck */
    tusb_net_free_tx_cb_t free_tx_buffer;     /*!< User function for freeing the Tx buffer.
                                               *    - could be NULL if the application is responsible for freeing the buffer
                                               *    - is called after tinyusb_net_send() returns ESP_OK and the packet is sent
                                               *      or discarded because USB is not ready; it runs in TinyUSB task context
                                               */
    tusb_net_init_cb_t on_init_callback;      /*!< TinyUSB init network callback */
    void *user_context;                       /*!< User context to be passed to any of the callback */
} tinyusb_net_config_t;

/**
 * @brief Initialize TinyUSB NET driver
 *
 * @param[in] usb_dev USB device to use
 * @param[in] cfg     Configuration of the driver
 * @return esp_err_t
 */
esp_err_t tinyusb_net_init(tinyusb_usbdev_t usb_dev, const tinyusb_net_config_t *cfg);

/**
 * @brief Queue a packet for asynchronous transmission over USB
 *
 * @param[in] buffer            USB send data
 * @param[in] len               USB send data length
 * @param[in] buff_free_arg     Pointer passed to the free_tx_buffer() callback
 * @return ESP_OK if the packet was queued; the send buffer must remain valid until the
 *         free_tx_buffer() callback runs, if configured
 *         ESP_ERR_INVALID_STATE if the TinyUSB NET driver is not initialized
 *         ESP_ERR_NO_MEM if the transmit queue is full
 */
esp_err_t tinyusb_net_send(void *buffer, uint16_t len, void *buff_free_arg);

/**
 * @brief Queue a network link-state update for the TinyUSB task
 *
 * @param[in] connected True when the network link is up
 */
void tinyusb_net_set_link_state(bool connected);

/**
 * @brief Process one pending transmit packet and any queued link-state update
 *
 * Call this from the same task that runs the TinyUSB device task. When using
 * CONFIG_TINYUSB_NO_DEFAULT_TASK, call tud_task_ext(1, false) and this function
 * in the application-owned TinyUSB task loop.
 */
void tinyusb_net_process_tx_queue(void);

#endif // (CONFIG_TINYUSB_NET_MODE_NONE != 1)

#ifdef __cplusplus
}
#endif
