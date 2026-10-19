/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_xmodem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                   uart_num;       /*!< The number of uart to send data(default is UART_NUM_0) */
    int                   baud_rate;      /*!< The uart baud rate (default is 115200) */
    int                   rx_buffer_size; /*!< The uart rx buffer size(default is 2048) */
    int                   tx_buffer_size; /*!< The uart tx buffer size(default is 2048) */
    int                   queue_size;     /*!< The uart queue size(default is 10) */
    int                   recv_timeout;   /*!< The timeout(ms) for queue receive(default is 100) */
    bool                  swap_pin;       /*!< Enable swap the pin(default is false) */
#ifndef CONFIG_IDF_TARGET_ESP8266         /*!< For ESP8266, tx/rx pin is unconfigurable, tx pin is default IO15, rx pin is default IO13) */
    int                   tx_pin;         /*!< The swap tx pin */
    int                   rx_pin;         /*!< The swap rx pin */
    int                   cts_pin;        /*!< The swap cts pin */
    int                   rts_pin;        /*!< The swap rts pin */
#endif
} esp_xmodem_transport_config_t;

typedef void* esp_xmodem_transport_handle_t;

/**
 * @brief      XModem transport init.
 *
 * @param[in]  config   The configurations, see `esp_xmodem_transport_config_t`
 *
 * @return
 *     - `esp_xmodem_transport_handle_t`
 *     - NULL if any errors
 */
esp_xmodem_transport_handle_t esp_xmodem_transport_init(esp_xmodem_transport_config_t *config);

/**
 * @brief      XModem transport close.
 *
 * @param[in]  handle   The esp_xmodem_handle_t handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_xmodem_transport_close(esp_xmodem_handle_t handle);

#ifdef __cplusplus
}
#endif
