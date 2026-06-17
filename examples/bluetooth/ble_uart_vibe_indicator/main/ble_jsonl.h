/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Reset line accumulator and start the JSONL worker task. */
esp_err_t ble_jsonl_init(void);

/** Enqueue raw RX bytes from ble_uart_on_rx; returns quickly. */
void ble_jsonl_rx_feed(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
