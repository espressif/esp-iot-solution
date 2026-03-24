/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** @file
 *  @brief Example helper API for BLE OTA raw profile demo.
 */

/** Default ring buffer size when @ref ble_ota_raw_ringbuf_init is called with 0. */
#define BLE_OTA_RAW_RINGBUF_DEFAULT_SIZE   8192

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create firmware ring buffer for OTA data buffering.
 *
 * Calling @ref ble_ota_raw_ringbuf_init multiple times will delete any existing
 * ring buffer and create a new one. Reinitialization discards any buffered OTA
 * data that has not been consumed yet.
 *
 * @param[in] ringbuf_size  Buffer size in bytes. Set 0 to use
 *                          @ref BLE_OTA_RAW_RINGBUF_DEFAULT_SIZE.
 *
 * @return true if the ring buffer is successfully created (including after
 *         reinitialization), false if allocation/creation fails.
 */
bool ble_ota_raw_ringbuf_init(uint32_t ringbuf_size);

/**
 * @brief Create OTA worker task.
 *
 * @ref ble_ota_raw_ringbuf_init must be called successfully before
 * @ref ble_ota_raw_task_init.
 *
 * Calling @ref ble_ota_raw_task_init repeatedly is safe: if the OTA worker task
 * already exists, this function returns true and does not create a duplicate task.
 *
 * @return true if preconditions are met and the OTA worker task exists (created
 *         in this call or already running), false if ring buffer is not
 *         initialized or task creation fails.
 */
bool ble_ota_raw_task_init(void);

/**
 * @brief Firmware receive callback for ble_ota_raw profile.
 *
 * @param[in] buf     Firmware chunk buffer. The data is copied into the
 *                    internal ring buffer before this function returns, so the
 *                    caller retains ownership and only needs to keep @p buf
 *                    valid for the duration of this call.
 * @param[in] length  Firmware chunk length in bytes.
 *
 * @return true when data is accepted and copied into the internal ring buffer;
 *         false when input is invalid (@p buf is NULL or @p length is 0), the
 *         ring buffer is not initialized, or ring buffer write fails.
 */
bool ble_ota_raw_recv_fw_cb(const uint8_t *buf, uint32_t length);

#ifdef __cplusplus
}
#endif
