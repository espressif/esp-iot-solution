/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

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
                                               *    - could be NULL, if user app is responsible for freeing the buffer
                                               *    - must be used in asynchronous send mode
                                               *    - is only called if the used tinyusb_net_send...() function returns ESP_OK
                                               *        - in sync mode means that the packet was accepted by TinyUSB
                                               *        - in async mode means that the packet was queued to be processed in TinyUSB task
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
 * @brief TinyUSB NET driver send data synchronously
 *
 * @param[in] buffer            USB send data
 * @param[in] len               Send data len
 * @param[in] buff_free_arg     Pointer to be passed to the free_tx_buffer() callback
 * @return  ESP_OK on success == packet has been consumed by tusb and would be eventually freed
 *                              by free_tx_buffer() callback (if non null)
 *          ESP_ERR_INVALID_STATE if tusb not initialized, ESP_ERR_NO_MEM on alloc failure
 */
esp_err_t tinyusb_net_send(void *buffer, uint16_t len, void *buff_free_arg);

#endif // (CONFIG_TINYUSB_NET_MODE_NONE != 1)

#ifdef __cplusplus
}
#endif
