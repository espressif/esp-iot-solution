/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_control_gear.h
 * @brief DALI Part 102 — Control Gear commissioning and addressing helpers.
 *
 * Depends on dali_system_components.h for the handle type and transaction primitives.
 */

#pragma once

#include "dali_system_components.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Part 102 Commissioning (Address Assignment)
 * ========================================================================= */

/**
 * @brief Assign short addresses to Part 102 control gear (IEC 62386-102).
 *
 * Executes TERMINATE → INITIALISE → RANDOMISE → binary-search loop
 * (COMPARE / PROGRAM_SHORT_ADDR / WITHDRAW) → TERMINATE.
 *
 * Before starting the Part 102 sequence the function broadcasts Part 103
 * START_QUIESCENT_MODE so that input devices do not emit frames during the
 * COMPARE windows.  On completion the bus is left in quiescent mode; call
 * STOP_QUIESCENT_MODE explicitly when you are ready for sensors to push
 * event frames.
 *
 * @param[in]  handle        Handle from dali_new_master_rmt().
 * @param[in]  mode          DALI_COMMISSION_ALL or DALI_COMMISSION_UNADDRESSED.
 * @param[in]  start_addr    First short address to assign (0–63).
 * @param[in]  max_devices   Maximum addresses to assign (1–64).
 * @param[out] count         Devices actually found and addressed (may be NULL).
 * @param[in]  tx_timeout_ms Per-frame TX timeout (ms).
 *
 * @return ESP_OK on success, or an ESP_ERR code.
 */
esp_err_t dali_commission(dali_master_handle_t handle, dali_commission_mode_t mode, uint8_t start_addr,
                          uint8_t max_devices, uint8_t *count, int tx_timeout_ms);

#ifdef __cplusplus
}
#endif
