/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
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

typedef struct {
    uint32_t report_id;    // Report identifier
    union {
        struct {
            uint8_t modifier;     // Modifier keys
            uint8_t reserved;     // Reserved byte
            uint8_t keycode[15];  // Keycode
        } keyboard_full_key_report;  // Keyboard full key report
        struct {
            uint8_t modifier;   // Modifier keys
            uint8_t reserved;   // Reserved byte
            uint8_t keycode[6]; // Keycodes
        } keyboard_report;  // Keyboard report
        struct {
            uint16_t keycode;    // Keycode
        } consumer_report;
    };
} hid_report_t;

/**
 * @brief Button progress function.
 *
 * @param kbd_report Keyboard button report.
 */
void btn_progress(keyboard_btn_report_t kbd_report);

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
