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
