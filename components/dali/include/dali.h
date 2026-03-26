/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali.h
 * @brief DALI (IEC 62386) bus controller driver for ESP-IDF.
 *
 * This driver implements a DALI master controller using the ESP32 RMT
 * (Remote Control Transceiver) peripheral. It supports sending forward
 * frames (FF) and receiving backward frames (BF) according to the
 * Manchester-encoded DALI physical layer specification.
 *
 * ### Signal characteristics
 * - Half-period (Te): 416.67 µs ± 10%
 * - Logic 1 (start of bit): 1 Te low, then 1 Te high
 * - Logic 0 (start of bit): 1 Te high, then 1 Te low
 * - Start bit: logic 1
 * - Stop bits: 2 Te high
 *
 * ### Frame timing rules (IEC 62386 Part 101)
 * - Forward frame (FF):  2 × (1 start + 16 data + 2 stop) half-periods = 38 Te
 * - Backward frame (BF): 2 × (1 start + 8 data  + 2 stop) half-periods = 22 Te
 * - No reply expected:   next FF must be sent > 22 Te after current FF ends
 * - Reply expected:      BF arrives within 7 Te–22 Te after FF ends;
 *                        next FF must wait > 22 Te after BF ends
 * - Send-twice commands: second FF must follow within 100 ms of first FF
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "soc/gpio_num.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/** RMT clock resolution used by this driver (1 MHz → 1 µs per tick). */
#define DALI_RMT_RESOLUTION_HZ      1000000U

/** Convert microseconds to RMT ticks. */
#define DALI_US_TO_RMT_TICKS(us)    ((us) * (DALI_RMT_RESOLUTION_HZ / 1000000U))

/** Convert microseconds to nanoseconds (used for RMT filter/idle config). */
#define DALI_US_TO_NS(us)           ((us) * 1000U)

/** DALI half-period Te in microseconds (nominal 416.67 µs). */
#define DALI_TE_US                  416U

/**
 * @defgroup dali_timing DALI bit-period detection thresholds
 *
 * The decoder accepts a ±20 % tolerance around the nominal Te value,
 * which comfortably covers the ±10 % allowed by the standard.
 * @{
 */
#define DALI_1TE_MIN_US             334U  /**< Lower bound for 1 Te pulse  */
#define DALI_1TE_MAX_US             500U  /**< Upper bound for 1 Te pulse  */
#define DALI_2TE_MIN_US             (2U * DALI_1TE_MIN_US)  /**< Lower bound for 2 Te pulse */
#define DALI_2TE_MAX_US             (2U * DALI_1TE_MAX_US)  /**< Upper bound for 2 Te pulse */
/** @} */

/**
 * @defgroup dali_misc Miscellaneous constants
 * @{
 */
/** Default TX transmission timeout in milliseconds. */
#define DALI_TX_TIMEOUT_MS          110

/**
 * @brief Sentinel value returned in *result when no backward frame was received.
 *
 * A genuine DALI backward frame carries an 8-bit value (0x00–0xFF).
 * This negative sentinel is outside that range and is safe to use as
 * a "no reply" indicator.
 */
#define DALI_RESULT_NO_REPLY        (-1)

/**
 * @brief Test whether a query result contains a valid backward-frame byte.
 *
 * Usage:
 * @code
 *   int reply;
 *   dali_transaction(..., &reply);
 *   if (DALI_RESULT_IS_VALID(reply)) { ... }
 * @endcode
 */
#define DALI_RESULT_IS_VALID(r)     ((r) >= 0)
/** @} */

/**
 * @brief DALI address types.
 *
 * Selects the addressing mode used when constructing the first byte of a
 * forward frame.
 */
typedef enum {
    DALI_ADDR_SHORT     = 0, /**< Short address (0–63, format: 0AAAAAAS)    */
    DALI_ADDR_GROUP     = 1, /**< Group address (0–15, format: 100AAAAS)    */
    DALI_ADDR_BROADCAST = 2, /**< Broadcast     (format: 1111111S)          */
    DALI_ADDR_SPECIAL   = 3, /**< Special command byte passed through as-is */
} dali_addr_type_t;

/**
 * @brief Initialize the DALI driver.
 *
 * Configures the RMT TX channel on @p tx_gpio and the RMT RX channel on
 * @p rx_gpio. Must be called once before any call to @ref dali_transaction.
 *
 * @param[in] rx_gpio  GPIO number for the DALI bus receive line.
 * @param[in] tx_gpio  GPIO number for the DALI bus transmit line.
 *
 * @return
 *   - ESP_OK               on success
 *   - ESP_ERR_INVALID_ARG  if either GPIO number is invalid
 *   - Other ESP_ERR codes  propagated from RMT driver
 */
esp_err_t dali_init(gpio_num_t rx_gpio, gpio_num_t tx_gpio);

/**
 * @brief De-initialise the DALI driver and release all RMT resources.
 *
 * Must be called before calling dali_init() a second time (e.g. in unit tests).
 * Safe to call even if dali_init() has never been called.
 *
 * @return ESP_OK always.
 */
esp_err_t dali_deinit(void);

/**
 * @brief Execute a DALI transaction (forward frame, optional backward frame).
 *
 * Transmits a 16-bit DALI forward frame composed of an address byte and a
 * command/data byte, then optionally listens for an 8-bit backward frame.
 *
 * **Address byte construction:**
 * | @p addr_type          | First byte format            |
 * |-----------------------|------------------------------|
 * | DALI_ADDR_SHORT       | `0AAAAAAS` (A = 0–63)        |
 * | DALI_ADDR_GROUP       | `100AAAAS` (A = 0–15)        |
 * | DALI_ADDR_BROADCAST   | `1111111S`                   |
 * | DALI_ADDR_SPECIAL     | @p addr passed through as-is |
 *
 * The S (selector) bit is set to 1 when @p is_cmd is `true` (indirect
 * command), or 0 when `false` (direct arc-power control, DAPC).
 *
 * **Send-twice commands:** Certain configuration commands (e.g., RESET,
 * STORE_ACTUAL_LEVEL) only take effect when sent twice within 100 ms.
 * Set @p send_twice to `true` and the driver will automatically re-transmit
 * the frame after a 40 ms delay.
 *
 * **Result semantics:**
 * - Pass `NULL` for @p result if no backward frame is expected. The driver
 *   will wait one backward-frame window before returning.
 * - Pass a pointer for @p result to enable RX. On return:
 *   - `*result >= 0`: valid 8-bit backward-frame value (0x00–0xFF)
 *   - `*result == DALI_RESULT_NO_REPLY`: timeout — no reply received
 *
 * @param[in]  addr_type   Address type (@ref dali_addr_type_t).
 * @param[in]  addr        Device address (0–63 for short, 0–15 for group,
 *                         ignored for broadcast, raw byte for special).
 * @param[in]  is_cmd      `true`  → indirect command (S-bit = 1).
 *                         `false` → direct DAPC value  (S-bit = 0).
 * @param[in]  command     Command code or arc-power value (second byte).
 * @param[in]  send_twice  `true` to repeat the frame within 100 ms.
 * @param[in]  tx_timeout_ms  Timeout in ms for the RMT TX completion wait.
 * @param[out] result      Pointer to store the backward-frame result, or
 *                         `NULL` if no reply is expected.
 *
 * @return
 *   - ESP_OK               on success (including DALI_RESULT_NO_REPLY case)
 *   - ESP_ERR_INVALID_ARG  if address is out of range for the given type
 *   - Other ESP_ERR codes  propagated from RMT driver
 */
esp_err_t IRAM_ATTR dali_transaction(dali_addr_type_t addr_type,
                                     uint8_t          addr,
                                     bool             is_cmd,
                                     uint8_t          command,
                                     bool             send_twice,
                                     int              tx_timeout_ms,
                                     int             *result);


static inline void dali_wait_between_frames(void)
{
    vTaskDelay(pdMS_TO_TICKS(20));
}

#ifdef __cplusplus
}
#endif
