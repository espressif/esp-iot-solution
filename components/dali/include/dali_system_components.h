/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_system_components.h
 * @brief DALI Part 101 — Physical Layer & RMT Driver Core.
 *
 * Provides the low-level RMT-backed driver: hardware initialisation,
 * forward-frame transmission, backward-frame reception, and the raw
 * three-byte Part-103 transaction helper used by upper layers.
 *
 * All higher-level modules (dali_control_gear, dali_control_device, …) depend on this
 * header and must never access the dali_master_t internals directly.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_attr.h"
#include "soc/gpio_num.h"
#include "dali_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Miscellaneous constants
 * ========================================================================= */

/** Default TX transmission timeout in milliseconds. */
#define DALI_TX_TIMEOUT_MS      110

/**
 * @brief Sentinel value returned in *result when no backward frame was received.
 */
#define DALI_RESULT_NO_REPLY    (-1)

/**
 * @brief Test whether a query result contains a valid backward-frame byte.
 */
#define DALI_RESULT_IS_VALID(r) ((r) >= 0)

/* =========================================================================
 * Types
 * ========================================================================= */

/**
 * @brief DALI address types.
 */
typedef enum {
    DALI_ADDR_SHORT     = 0, /*!< Short address (0–63)    */
    DALI_ADDR_GROUP     = 1, /*!< Group address (0–15)    */
    DALI_ADDR_BROADCAST = 2, /*!< Broadcast               */
    DALI_ADDR_SPECIAL   = 3, /*!< Special command byte    */
} dali_addr_type_t;

/**
 * @brief Commissioning mode selector (used by both Part 102 and Part 103).
 */
typedef enum {
    DALI_COMMISSION_ALL         = 0, /*!< All gear/devices (Part 102: init byte = 0x00; Part 103: init byte = 0xFF) */
    DALI_COMMISSION_UNADDRESSED = 1, /*!< Unaddressed gear/devices only (Part 102: init byte = 0xFF; Part 103: init byte = 0x00) */
} dali_commission_mode_t;

/**
 * @brief Opaque handle for a DALI driver instance.
 */
typedef struct dali_master_t *dali_master_handle_t;

/**
 * @brief Bus-level configuration for a DALI master instance.
 */
typedef struct {
    gpio_num_t rx_gpio; /*!< GPIO for DALI bus RX */
    gpio_num_t tx_gpio; /*!< GPIO for DALI bus TX */
    bool invert_tx;     /*!< Invert TX signal polarity */
    bool invert_rx;     /*!< Invert RX signal polarity */
} dali_master_config_t;

/**
 * @brief RMT-backend specific configuration.
 */
typedef struct {
    uint32_t mem_block_symbols; /*!< 0 = auto-detect from SOC */
} dali_master_rmt_config_t;

/**
 * @brief Transaction configuration for dali_master_do_transaction().
 */
typedef struct {
    dali_addr_type_t addr_type;
    uint8_t          addr;
    bool             is_cmd;
    uint8_t          command;
    bool             send_twice;
    int              tx_timeout_ms;
} dali_master_transaction_config_t;

/* =========================================================================
 * API — driver lifecycle
 * ========================================================================= */

/**
 * @brief Create and initialise a DALI master backed by RMT.
 */
esp_err_t dali_new_master_rmt(const dali_master_config_t *config, const dali_master_rmt_config_t *rmt_config,
                              dali_master_handle_t *handle);

/**
 * @brief De-initialise and free a DALI master instance.
 */
esp_err_t dali_del_master(dali_master_handle_t handle);

/* =========================================================================
 * API — transaction
 * ========================================================================= */

/**
 * @brief Execute a 2-byte DALI forward frame and optionally receive a
 *        backward frame.
 */
esp_err_t dali_master_do_transaction(dali_master_handle_t handle, const dali_master_transaction_config_t *config,
                                     int *result);

/**
 * @brief Send a raw N-byte DALI frame (low-level physical layer API).
 *
 * This is the underlying frame transmission primitive used by both:
 * - dali_master_do_transaction() for standard 2-byte Part 102 frames
 * - Part 103 input device functions for 3-byte extended frames
 *
 * Most applications should use the higher-level APIs (dali_master_do_transaction
 * or dali_103_do_device_command) instead of calling this directly.
 *
 * @param[in]  handle        Handle from dali_new_master_rmt().
 * @param[in]  tx_buf        Raw bytes to send (Manchester-encoded by RMT).
 * @param[in]  tx_len        Number of bytes in tx_buf (2 for Part 102, 3 for Part 103).
 * @param[in]  send_twice    If true, sends the frame twice within 100 ms.
 * @param[in]  tx_timeout_ms TX timeout per frame (ms).
 * @param[out] result        Received backward frame byte, or DALI_RESULT_NO_REPLY.
 */
esp_err_t dali_master_do_raw_transaction(dali_master_handle_t handle, const uint8_t *tx_buf, size_t tx_len,
                                         bool send_twice, int tx_timeout_ms, int *result);

#ifdef __cplusplus
}
#endif
