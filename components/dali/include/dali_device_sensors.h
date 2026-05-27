/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_device_sensors.h
 * @brief DALI Part 303 (Occupancy Sensor) & Part 304 (Light Sensor) helpers.
 *
 * Both parts are extensions of Part 103 input devices.  This module provides
 * convenience wrappers that encode the correct instance command and parse the
 * backward frame.
 *
 * Depends on dali_control_device.h for the instance-command primitive.
 */

#pragma once

#include <math.h>  /* powf — used by DALI_304_RAW_TO_LUX_FP */
#include "dali_control_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Part 303 — Occupancy Sensor (PIR)
 * ========================================================================= */

/**
 * @brief Query the occupancy state of a Part 303 input device.
 *
 * @param[in]  handle        Handle from dali_new_master_rmt().
 * @param[in]  addr_type     Address type.
 * @param[in]  addr          Device short address.
 * @param[in]  instance      Occupancy instance number (typically 0).
 * @param[out] occupied      true if occupied, false if unoccupied/no reply.
 * @param[in]  tx_timeout_ms TX timeout (ms).
 */
esp_err_t dali_303_query_occupancy(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                   uint8_t instance, bool *occupied, int tx_timeout_ms);

/**
 * @brief Query the hold timer on a Part 303 occupancy sensor instance.
 */
esp_err_t dali_303_query_hold_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                    uint8_t instance, uint8_t *hold_timer, int tx_timeout_ms);

/**
 * @brief Query the deadtime timer on a Part 303 occupancy sensor instance.
 */
esp_err_t dali_303_query_deadtime_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                        uint8_t instance, uint8_t *deadtime_timer, int tx_timeout_ms);

/**
 * @brief Set the hold timer on a Part 303 occupancy sensor instance.
 *
 * Loads @p hold_time_s into Part 103 DTR0 via a special command, then
 * issues DALI_303_SET_HOLD_TIMER (send-twice) to the addressed instance.
 *
 * @param[in] hold_time_s  Raw DTR0 value (see IEC 62386-303 for encoding).
 */
esp_err_t dali_303_set_hold_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                  uint8_t instance, uint8_t hold_time_s, int tx_timeout_ms);

/* =========================================================================
 * Part 304 — Light Sensor (Illuminance)
 * ========================================================================= */

/**
 * @brief Query the hysteresis setting of a Part 304 light sensor instance.
 */
esp_err_t dali_304_query_hysteresis(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                    uint8_t instance, uint8_t *hysteresis, int tx_timeout_ms);

/**
 * @brief Query the report timer of a Part 304 light sensor instance.
 */
esp_err_t dali_304_query_report_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                      uint8_t instance, uint8_t *report_timer, int tx_timeout_ms);

/**
 * @brief Query the deadtime timer of a Part 304 light sensor instance.
 */
esp_err_t dali_304_query_deadtime_timer(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                        uint8_t instance, uint8_t *deadtime_timer, int tx_timeout_ms);

/**
 * @brief Convert a raw Part 304 sensor value to approximate Lux (float).
 *
 * IEC 62386-304 logarithmic encoding: lux = 10^((raw - 1) / 40).
 * Returns 0.0f for raw == 0 ("no value available").
 */
#define DALI_304_RAW_TO_LUX_FP(raw) \
    ((raw) ? powf(10.0f, ((float)(raw) - 1.0f) / 40.0f) : 0.0f)

#ifdef __cplusplus
}
#endif
