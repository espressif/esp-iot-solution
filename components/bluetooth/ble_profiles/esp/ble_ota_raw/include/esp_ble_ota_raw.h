/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 *  @brief BLE OTA profile API based on esp_ble_conn_mgr.
 */

/**
 * @brief Firmware data callback from BLE OTA profile.
 *
 * @param[in] buf     Firmware data buffer (owned by profile, valid only during callback).
 * @param[in] length  Data length in bytes.
 */
typedef void (*esp_ble_ota_raw_recv_fw_cb_t)(uint8_t *buf, uint32_t length);

/**
 * @brief Optional hook to open the flash OTA session (e.g. @c esp_ota_begin) on START.
 *
 * Invoked with the firmware length from START after RAM is allocated, before @c start_ota
 * is set and before START CMD ACK is sent. If this returns non-ESP_OK, START is rejected.
 *
 * @param[in] image_size_bytes  Total firmware size from START payload.
 *
 * @return ESP_OK on success, or an error from the application OTA begin path.
 */
typedef esp_err_t (*esp_ble_ota_raw_ota_begin_cb_t)(uint32_t image_size_bytes);

/**
 * @brief Register OTA flash begin hook (e.g. @c esp_ota_begin).
 *
 * Call from the application before the host may send START. May be NULL to skip.
 */
void esp_ble_ota_raw_set_ota_begin_cb(esp_ble_ota_raw_ota_begin_cb_t cb);

/**
 * @brief Derive sector send window from firmware ring buffer capacity.
 *
 * Window is @p ringbuf_capacity_bytes / 4096 (one sector), clamped to 1..64, and sent in START
 * ACK byte 6 (byte 7 reserved). Call with the same capacity passed to @c xRingbufferCreate (e.g.
 * example default @c BLE_OTA_RAW_RINGBUF_DEFAULT_SIZE gives window 3).
 *
 * @param ringbuf_capacity_bytes  Ring buffer size in bytes.
 */
void esp_ble_ota_raw_set_sector_send_window_for_ringbuf(uint32_t ringbuf_capacity_bytes);

/**
 * @brief Initialize BLE OTA profile over ble_conn_mgr.
 *
 * Registers OTA service (0x8018) and DIS service.
 * BLE connection manager init/start is done by the application.
 *
 * @return
 *  - ESP_OK: success
 *  - Others: error from dependent service initialization
 */
esp_err_t esp_ble_ota_raw_init(void);

/**
 * @brief Deinitialize BLE OTA profile over ble_conn_mgr.
 *
 * @return
 *  - ESP_OK: success
 *  - Others: error from OTA service deinitialization
 */
esp_err_t esp_ble_ota_raw_deinit(void);

/**
 * @brief Register OTA firmware receive callback for conn_mgr backend.
 *
 * Callback is invoked when one full sector passes CRC check.
 * Register this before processing START command from the host.
 *
 * @param[in] callback  Firmware receive callback.
 *
 * @return
 *  - ESP_OK: success
 */
esp_err_t esp_ble_ota_raw_recv_fw_data_callback(esp_ble_ota_raw_recv_fw_cb_t callback);

/**
 * @brief Get current OTA firmware total length.
 *
 * Returns @c UINT32_MAX before START command is received.
 *
 * @return Total firmware size in bytes, or @c UINT32_MAX when OTA is not started.
 */
uint32_t esp_ble_ota_raw_get_fw_length(void);

#ifdef __cplusplus
}
#endif
