/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "hid_report_types.h"
#include "keyboard_button.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TINYUSB_HID_REPORT = 0,
    BLE_HID_REPORT,
    BTN_REPORT_TYPE_MAX,
} btn_report_type_t;

typedef enum {
    RGB_MATRIX = 0,
    LAMP_ARRAY_MATRIX,
} light_type_t;

/**
 * @brief Button progress function.
 *
 * @param kbd_report Keyboard button report.
 */
void btn_progress(keyboard_btn_report_t kbd_report);

/**
 * @brief Switch HID report transport at runtime.
 *
 * @param new_type New report transport type.
 */
esp_err_t keyboard_switch_report_type(btn_report_type_t new_type);

/**
 * @brief Set the report type for button progress.
 *
 * @param type Button report type.
 */
void btn_progress_set_report_type(btn_report_type_t type);

/**
 * @brief Set the light type for button progress.
 *
 * @param type Light type.
 */
void btn_progress_set_light_type(light_type_t type);

/**
 * @brief Function to handle light progress.
 */
void light_progress(void);

#ifdef __cplusplus
}
#endif
