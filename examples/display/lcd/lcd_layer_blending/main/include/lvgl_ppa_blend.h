/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "lcd_ppa_blend.h"
#include "lvgl_ppa_blend_private.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LVGL_PPA_BLEND_COLOR_MODE_ARGB8888 = 0,
    LVGL_PPA_BLEND_COLOR_MODE_RGB565,
    LVGL_PPA_BLEND_COLOR_MODE_RGB888,
    LVGL_PPA_BLEND_USER_DATA_MODE,
} lvgl_ppa_blend_color_mode_t;

#define LVGL_PPA_BLEND_FG_H_RES          (LCD_PPA_BLEND_OUT_H_RES)
#define LVGL_PPA_BLEND_FG_V_RES          (LCD_PPA_BLEND_OUT_V_RES)

/**
 * LVGL related parameters, can be adjusted by users
 *
 */
#define LVGL_PPA_BLEND_H_RES             (LCD_PPA_BLEND_OUT_H_RES)
#define LVGL_PPA_BLEND_V_RES             (LCD_PPA_BLEND_OUT_V_RES)
#define LVGL_PPA_BLEND_TICK_PERIOD_MS    (2)

/**
 * LVGL timer handle task related parameters, can be adjusted by users
 *
 */
#define LVGL_PPA_BLEND_TASK_MAX_DELAY_MS (500)    // The maximum delay of the LVGL timer task, in milliseconds
#define LVGL_PPA_BLEND_TASK_MIN_DELAY_MS (10)    // The minimum delay of the LVGL timer task, in milliseconds
#define LVGL_PPA_BLEND_TASK_STACK_SIZE   (64 * 1024) // The stack size of the LVGL timer task, in bytes
#define LVGL_PPA_BLEND_TASK_PRIORITY     (5)        // The priority of the LVGL timer task
#define LVGL_PPA_BLEND_TASK_CORE         (1)            // The core of the LVGL timer task,
// `-1` means the don't specify the core
/**
 *
 * LVGL buffer related parameters, can be adjusted by users:
 *  (These parameters will be useless if the avoid tearing function is enabled)
 *
 *  - Memory type for buffer allocation:
 *      - MALLOC_CAP_SPIRAM: Allocate LVGL buffer in PSRAM
 *      - MALLOC_CAP_INTERNAL: Allocate LVGL buffer in SRAM
 *      (The SRAM is faster than PSRAM, but the PSRAM has a larger capacity)
 *
 */
#define LVGL_PPA_BLEND_BUFFER_MALLOC_CAPS    (MALLOC_CAP_SPIRAM)
// #define LVGL_PPA_BLEND_BUFFER_MALLOC_CAPS    (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define LVGL_PPA_BLEND_BUFFER_HEIGHT         (LVGL_PPA_BLEND_V_RES)

#define LVGL_PPA_BLEND_FG_BUFFER_NUMS        (2)

/**
 * @brief Initialize LVGL port
 *
 * @param[in] lcd: LCD panel handle
 * @param[in] tp: Touch handle
 * @param[out] disp: LVGL display device
 * @param[out] indev: LVGL input device
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others: Fail
 */
esp_err_t lvgl_ppa_blend_init(esp_lcd_panel_handle_t lcd_in, esp_lcd_touch_handle_t tp_in, lv_display_t **disp_out, lv_indev_t **indev_out);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool lvgl_ppa_blend_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void lvgl_ppa_blend_unlock(void);

/**
 * @brief Change the display mode in LVGL (LittlevGL).
 *
 * This function changes the display mode of the LVGL display based on the mode parameter.
 * It locks the PPA blend, logs the mode change, and adjusts the display settings accordingly.
 *
 * @param mode The mode to change to. If non-zero, it switches to RGB565 color format with specific
 *             buffer configurations. If zero, it switches to ARGB8888 color format with different buffer configurations.
 */
void lvgl_mode_change(lvgl_ppa_blend_color_mode_t mode);

/**
 * @brief Register a callback function for user data flow switch on event.
 *
 * This function registers the provided callback function to be called when the user data flow switch on event occurs.
 *
 * @param callback Pointer to the callback function.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
esp_err_t lvgl_ppa_blend_register_switch_on_callback(user_data_flow_cb_t callback);

/**
 * @brief Register a callback function for user data flow switch off event.
 *
 * This function registers the provided callback function to be called when the user data flow switch off event occurs.
 *
 * @param callback Pointer to the callback function.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_ARG if the input parameter is NULL.
 */
esp_err_t lvgl_ppa_blend_register_switch_off_callback(user_data_flow_cb_t callback);

#ifdef __cplusplus
}
#endif
