/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE JSONL protocol resources.
 *
 * Creates the RX worker task and permission-state synchronization primitives.
 * Call this before opening the BLE UART service.
 */
void ble_protocol_init(void);

/**
 * @brief Queue raw BLE UART RX bytes for JSONL parsing.
 *
 * This function is designed to be used as @c ble_uart_config_t::ble_uart_on_rx.
 *
 * @param data Received byte buffer.
 * @param len Number of bytes in @p data.
 */
void ble_protocol_rx_feed(const uint8_t *data, size_t len);

/**
 * @brief Parse one complete JSONL record.
 *
 * This function is exposed for direct tests and console tools. Normal BLE RX
 * should enter through @ref ble_protocol_rx_feed.
 *
 * @param line JSON object bytes without the trailing newline.
 * @param len Number of bytes in @p line.
 */
void ble_protocol_handle_line(const uint8_t *line, size_t len);

/**
 * @brief Check whether a permission request is waiting for user input.
 *
 * @return true if a permission prompt is pending, false otherwise.
 */
bool ble_protocol_permission_is_pending(void);

/**
 * @brief Submit a user decision for the active permission request.
 *
 * @param behavior Permission decision. Supported values are `"once"` and
 * `"reject"`.
 *
 * @note If no permission request is pending, the decision is ignored. If no
 * decision is submitted within 30 seconds, the protocol replies with `"reject"`.
 */
void ble_protocol_submit_permission(const char *behavior);

/**
 * @brief Reset protocol state after the BLE link drops.
 *
 * Bumps the connection generation, cancels any pending permission prompt, and
 * discards a partially received JSONL line.
 */
void ble_protocol_on_ble_disconnected(void);

#ifdef __cplusplus
}
#endif
