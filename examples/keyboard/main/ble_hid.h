/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btn_progress.h"

/**
 * @brief Initialize BLE HID device.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: No memory
 */
esp_err_t ble_hid_init(void);

/**
 * @brief Deinitialize BLE HID device.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: No memory
 */
esp_err_t ble_hid_deinit(void);
/**
 * @brief Send ble hid keyboard report
 *
 * @param report hid report
 */
void ble_hid_keyboard_report(hid_report_t report);

/**
 * @brief Send ble hid battery level
 *
 * @param battery_level
 */
void ble_hid_battery_report(int battery_level);

#ifdef __cplusplus
}
#endif
