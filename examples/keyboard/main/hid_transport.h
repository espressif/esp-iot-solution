/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "btn_progress.h"
#include "hid_report_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize active transport from persisted report mode.
 */
esp_err_t hid_transport_init(void);

/**
 * @brief Apply the power policy required by the selected transport.
 */
esp_err_t hid_transport_configure_power(btn_report_type_t type);

/**
 * @brief Tear down active transport (BLE stack, etc.).
 */
void hid_transport_deinit(void);

void hid_transport_keyboard_report(hid_report_t report);

void hid_transport_battery_report(int battery_level);

/** BLE-only: activity timer + light sleep policy in main. */
bool hid_transport_needs_ble_style_pm(void);

#ifdef __cplusplus
}
#endif
