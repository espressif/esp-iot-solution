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
#include "esp_attr.h"
#include "soc/gpio_num.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 *   dali_master_do_transaction(handle, &config, &reply);
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
    DALI_ADDR_SHORT     = 0, /*!< Short address (0–63, format: 0AAAAAAS)    */
    DALI_ADDR_GROUP     = 1, /*!< Group address (0–15, format: 100AAAAS)    */
    DALI_ADDR_BROADCAST = 2, /*!< Broadcast     (format: 1111111S)          */
    DALI_ADDR_SPECIAL   = 3, /*!< Special command byte passed through as-is */
} dali_addr_type_t;

/**
 * @brief Opaque handle for a DALI driver instance.
 *
 * Obtained from @ref dali_new_master_rmt and passed to all subsequent API calls.
 * Multiple independent instances can coexist on different GPIO pairs.
 */
typedef struct dali_master_t *dali_master_handle_t;

/**
 * @brief Configuration structure for creating a DALI driver instance.
 *
 * Contains bus-agnostic parameters shared across all backend implementations
 * (RMT, UART, etc.).  Backend-specific parameters are passed separately via
 * the corresponding config struct (e.g. @ref dali_master_rmt_config_t).
 */
typedef struct {
    gpio_num_t rx_gpio; /*!< GPIO number for the DALI bus receive line */
    gpio_num_t tx_gpio; /*!< GPIO number for the DALI bus transmit line */
    bool invert_tx;     /*!< Invert TX signal polarity.
                         *   Enable when the hardware path between the
                         *   MCU TX GPIO and the DALI bus performs an
                         *   odd number of signal inversions.         */
    bool invert_rx;     /*!< Invert RX input signal polarity.
                         *   Enable when the hardware path between the
                         *   DALI bus and the MCU RX GPIO performs an
                         *   odd number of signal inversions.         */
} dali_master_config_t;

/**
 * @brief RMT-backend specific configuration for a DALI master driver instance.
 *
 * Pass a pointer to this struct as the @p rmt_config argument of
 * @ref dali_new_master_rmt.  Initialize with designated initializers
 * to override the built-in defaults:
 * @code
 *   dali_master_rmt_config_t rmt_cfg = {
 *       .mem_block_symbols = 48,
 *   };
 *   dali_new_master_rmt(&cfg, &rmt_cfg, &handle);
 * @endcode
 */
typedef struct {
    uint32_t mem_block_symbols; /*!< RMT memory block size in symbols for both
                                 *   TX and RX channels.  Set to 0 to let the
                                 *   driver auto-detect the optimal value based
                                 *   on SOC_RMT_MEM_WORDS_PER_CHANNEL (48 on
                                 *   ESP32-C3/S3, 64 on ESP32/S2).           */
} dali_master_rmt_config_t;

/**
 * @brief Create and initialize a new DALI driver instance backed by RMT.
 *
 * Allocates a driver context, configures the RMT TX channel on
 * @p config->tx_gpio and the RMT RX channel on @p config->rx_gpio, and
 * returns an opaque handle.  Multiple independent instances can be created
 * on different GPIO pairs.
 *
 * The naming convention follows the pattern @c dali_new_bus_<backend> so
 * that future backends (e.g. @c dali_new_bus_uart) can coexist without
 * naming conflicts.
 *
 * @param[in]  config      Pointer to bus configuration (GPIO numbers).
 * @param[in]  rmt_config  Pointer to RMT-specific configuration.  Pass NULL
 *                            to use built-in defaults (mem_block_symbols=64).
 * @param[out] handle      Pointer to store the created handle on success.
 *
 * @return
 *   - ESP_OK               on success; @p *handle is valid
 *   - ESP_ERR_INVALID_ARG  if @p config or @p handle is NULL, or GPIO numbers
 *                          are invalid
 *   - ESP_ERR_NO_MEM       if heap allocation fails
 *   - Other ESP_ERR codes  propagated from RMT driver
 */
esp_err_t dali_new_master_rmt(const dali_master_config_t *config,
                              const dali_master_rmt_config_t *rmt_config,
                              dali_master_handle_t *handle);

/**
 * @brief De-initialize a DALI driver instance and release all resources.
 *
 * Disables and deletes the RMT channels, encoder, and RX queue associated
 * with @p handle, then frees the context memory. The handle is invalid after
 * this call.
 *
 * Safe to call even if @ref dali_new_master_rmt only partially succeeded (e.g.
 * during error recovery).
 *
 * @param[in] handle  Handle returned by @ref dali_new_master_rmt.
 *
 * @return ESP_OK always.
 */
esp_err_t dali_del_master(dali_master_handle_t handle);

/**
 * @brief Transaction configuration for dali_master_do_transaction().
 *
 * Bundles all parameters that describe a single DALI transaction (forward
 * frame addressing, command/data byte, and transmission options).
 */
typedef struct {
    dali_addr_type_t addr_type;  /*!< Address type (@ref dali_addr_type_t) */
    uint8_t          addr;       /*!< Device address (0–63 for short, 0–15 for
                                  *   group, ignored for broadcast, raw byte
                                  *   for special) */
    bool             is_cmd;     /*!< true  → indirect command (S-bit = 1)
                                  *   false → direct DAPC value  (S-bit = 0) */
    uint8_t          command;    /*!< Command code or arc-power value (second byte) */
    bool             send_twice; /*!< true to repeat the frame within 100 ms */
    int              tx_timeout_ms; /*!< Timeout in ms for the RMT TX completion wait */
} dali_master_transaction_config_t;

/**
 * @brief Execute a DALI transaction (forward frame, optional backward frame).
 *
 * Transmits a 16-bit DALI forward frame composed of an address byte and a
 * command/data byte, then optionally listens for an 8-bit backward frame.
 * After the transaction completes the driver automatically inserts the
 * minimum inter-frame gap required by IEC 62386 (> 22 Te ≈ 9.2 ms), so
 * callers do not need to add an explicit delay between consecutive calls.
 *
 * **Address byte construction:**
 * | config->addr_type      | First byte format            |
 * |------------------------|------------------------------|
 * | DALI_ADDR_SHORT        | `0AAAAAAS` (A = 0–63)        |
 * | DALI_ADDR_GROUP        | `100AAAAS` (A = 0–15)        |
 * | DALI_ADDR_BROADCAST    | `1111111S`                   |
 * | DALI_ADDR_SPECIAL      | config->addr passed through  |
 *
 * The S (selector) bit is set to 1 when config->is_cmd is `true` (indirect
 * command), or 0 when `false` (direct arc-power control, DAPC).
 *
 * **Send-twice commands:** Certain configuration commands (e.g., RESET,
 * STORE_ACTUAL_LEVEL) only take effect when sent twice within 100 ms.
 * Set config->send_twice to `true` and the driver will automatically
 * re-transmit the frame after a 40 ms delay.
 *
 * **Result semantics:**
 * - Pass `NULL` for @p result if no backward frame is expected. The driver
 *   will wait one backward-frame window before returning.
 * - Pass a pointer for @p result to enable RX. On return:
 *   - `*result >= 0`: valid 8-bit backward-frame value (0x00–0xFF)
 *   - `*result == DALI_RESULT_NO_REPLY`: timeout — no reply received
 *
 * @note This function is **blocking**. It occupies the calling task for the
 *       duration of the TX transmission plus the backward-frame reception
 *       window (up to config->tx_timeout_ms + 50 ms). Do **not** call this
 *       function from an ISR or from a high-priority real-time task where
 *       blocking is not acceptable.
 *
 * @param[in]  handle  Handle returned by @ref dali_new_master_rmt.
 * @param[in]  config  Pointer to transaction configuration.
 * @param[out] result  Pointer to store the backward-frame result, or
 *                     `NULL` if no reply is expected.
 *
 * @return
 *   - ESP_OK               on success (including DALI_RESULT_NO_REPLY case)
 *   - ESP_ERR_INVALID_ARG  if @p handle or @p config is NULL, or address
 *                          is out of range
 *   - Other ESP_ERR codes  propagated from RMT driver
 */
esp_err_t dali_master_do_transaction(dali_master_handle_t handle,
                                     const dali_master_transaction_config_t *config,
                                     int *result);

#ifdef __cplusplus
}
#endif
