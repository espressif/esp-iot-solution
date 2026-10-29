/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "protocomm_ext_transports.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Configuration for protocomm console client transport
 *
 * Talks to a device-side protocomm_console server over UART using the
 * text protocol: `<ep_name> <session_id> <hex_payload>\r`, response is a
 * hex line.
 *
 * If the UART driver is already installed for @p uart_num, init reuses it
 * and does not reconfigure pins/baud or delete the driver on deinit.
 * Otherwise init installs the driver (param/pin) and deinit deletes it.
 */
typedef struct {
    /**
     * UART port number, for example @c UART_NUM_0.
     */
    int uart_num;
    /**
     * TX GPIO number used when the driver is installed by this transport.
     */
    int tx_io_num;
    /**
     * RX GPIO number used when the driver is installed by this transport.
     */
    int rx_io_num;
    /**
     * UART baud rate. Uses 115200 when set to 0.
     */
    int baud_rate;
    /**
     * Response timeout in milliseconds. Uses 5000 when set to 0.
     */
    uint32_t timeout_ms;
} protocomm_ext_console_config_t;

/**
 * @brief   Protocomm Ext console (UART) client transport
 */
extern const protocomm_ext_transport_t protocomm_ext_transport_console;

#ifdef __cplusplus
}
#endif
