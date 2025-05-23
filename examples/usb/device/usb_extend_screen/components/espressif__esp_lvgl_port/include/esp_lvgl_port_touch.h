/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LVGL port touch
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#if __has_include ("esp_lcd_touch.h")
#include "esp_lcd_touch.h"
#define ESP_LVGL_PORT_TOUCH_COMPONENT 1
#endif

#if LVGL_VERSION_MAJOR == 8
#include "esp_lvgl_port_compatibility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_LVGL_PORT_TOUCH_COMPONENT
/**
 * @brief Configuration touch structure
 */
typedef struct {
    lv_display_t *disp;    /*!< LVGL display handle (returned from lvgl_port_add_disp) */
    esp_lcd_touch_handle_t   handle;   /*!< LCD touch IO handle */
} lvgl_port_touch_cfg_t;

/**
 * @brief Add LCD touch as an input device
 *
 * @note Allocated memory in this function is not free in deinit. You must call lvgl_port_remove_touch for free all memory!
 *
 * @param touch_cfg Touch configuration structure
 * @return Pointer to LVGL touch input device or NULL when error occurred
 */
lv_indev_t *lvgl_port_add_touch(const lvgl_port_touch_cfg_t *touch_cfg);

/**
 * @brief Remove selected LCD touch from input devices
 *
 * @note Free all memory used for this display.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_remove_touch(lv_indev_t *touch);
#endif

#ifdef __cplusplus
}
#endif
