/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dali_control_device.h
 * @brief DALI Part 103 — Input Device commissioning and general helpers.
 *
 * Input devices use a separate 24-bit frame format and an independent
 * special-command set from Part 102 control gear.  This module provides:
 *   - Binary-search commissioning (dali_103_commission)
 *   - Device-level query helpers (status, reset, instance count/type)
 *
 * The dali_device_sensors module (Part 303/304) depends on the internal
 * helpers declared here.
 *
 * Depends on dali_system_components.h (provides dali_commission_mode_t and transaction primitives).
 */

#pragma once

#include "dali_system_components.h"   /* dali_commission_mode_t, dali_master_handle_t, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Internal helpers — also used by dali_device_sensors.c
 *
 * These functions are NOT part of the public API but must be visible across
 * translation units within this component.
 * ========================================================================= */

/**
 * @brief Send a Part 103 special command (3-byte frame: 0xC1, cmd, data).
 */
esp_err_t dali_103_send_special(dali_master_handle_t handle, uint8_t special_cmd, uint8_t data, bool send_twice,
                                int tx_timeout_ms, int *result);

/**
 * @brief Send a Part 103 device-level command (3-byte frame).
 *
 * @param addr_type  DALI_ADDR_SHORT or DALI_ADDR_BROADCAST.
 * @param addr       Short address (0–63) for DALI_ADDR_SHORT.
 */
esp_err_t dali_103_do_device_command(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                     uint8_t command, bool send_twice, int tx_timeout_ms, int *result);

/**
 * @brief Send a Part 103 instance-level command (3-byte frame).
 *
 * @param instance  Instance number (0–31); encoded directly in byte 2.
 */
esp_err_t dali_103_do_instance_command(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t instance, uint8_t command, bool send_twice, int tx_timeout_ms,
                                       int *result);

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * @brief Assign short addresses to Part 103 input devices (IEC 62386-103).
 *
 * Mirrors dali_commission() but uses the Part 103 special-command set.
 *
 * @param[in]  handle        Handle from dali_new_master_rmt().
 * @param[in]  mode          DALI_COMMISSION_ALL or DALI_COMMISSION_UNADDRESSED.
 * @param[in]  start_addr    First short address to assign (0–63).
 * @param[in]  max_devices   Maximum addresses to assign (1–64).
 * @param[out] count         Devices found and addressed (may be NULL).
 * @param[in]  tx_timeout_ms Per-frame TX timeout (ms).
 */
esp_err_t dali_103_commission(dali_master_handle_t handle, dali_commission_mode_t mode, uint8_t start_addr,
                              uint8_t max_devices, uint8_t *count, int tx_timeout_ms);

/**
 * @brief Query Part 103 input device status byte.
 */
esp_err_t dali_103_query_device_status(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t *status, int tx_timeout_ms);

/**
 * @brief Send a Part 103 RESET command (send-twice).
 */
esp_err_t dali_103_reset_device(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr, int tx_timeout_ms);

/**
 * @brief Query the type of a Part 103 instance.
 */
esp_err_t dali_103_query_instance_type(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                       uint8_t instance, uint8_t *type, int tx_timeout_ms);

/**
 * @brief Query the number of instances on a Part 103 input device.
 */
esp_err_t dali_103_query_number_of_instances(dali_master_handle_t handle, dali_addr_type_t addr_type, uint8_t addr,
                                             uint8_t *num_instances, int tx_timeout_ms);

#ifdef __cplusplus
}
#endif
