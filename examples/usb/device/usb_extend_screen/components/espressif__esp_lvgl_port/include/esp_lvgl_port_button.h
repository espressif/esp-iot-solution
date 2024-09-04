/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LVGL port button
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#if __has_include ("iot_button.h")
#include "iot_button.h"
#define ESP_LVGL_PORT_BUTTON_COMPONENT 1
#endif

#if LVGL_VERSION_MAJOR == 8
#include "esp_lvgl_port_compatibility.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ESP_LVGL_PORT_BUTTON_COMPONENT
/**
 * @brief Configuration of the navigation buttons structure
 */
typedef struct {
    lv_display_t *disp;                /*!< LVGL display handle (returned from lvgl_port_add_disp) */
    const button_config_t *button_prev;   /*!< Navigation button for previous */
    const button_config_t *button_next;   /*!< Navigation button for next */
    const button_config_t *button_enter;  /*!< Navigation button for enter */
} lvgl_port_nav_btns_cfg_t;

/**
 * @brief Add buttons as an input device
 *
 * @note Allocated memory in this function is not free in deinit. You must call lvgl_port_remove_navigation_buttons for free all memory!
 *
 * @param buttons_cfg Buttons configuration structure
 * @return Pointer to LVGL buttons input device or NULL when error occurred
 */
lv_indev_t *lvgl_port_add_navigation_buttons(const lvgl_port_nav_btns_cfg_t *buttons_cfg);

/**
 * @brief Remove selected buttons from input devices
 *
 * @note Free all memory used for this input device.
 *
 * @return
 *      - ESP_OK                    on success
 */
esp_err_t lvgl_port_remove_navigation_buttons(lv_indev_t *buttons);
#endif

#ifdef __cplusplus
}
#endif
