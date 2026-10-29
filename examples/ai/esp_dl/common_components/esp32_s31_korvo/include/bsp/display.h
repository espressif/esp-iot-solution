/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lv_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES                 (800)
#define BSP_LCD_V_RES                 (480)
#define BSP_LCD_PIXEL_CLOCK_HZ        (20 * 1000 * 1000)
#define BSP_LCD_DATA_WIDTH            (16)
#define BSP_LCD_BITS_PER_PIXEL        (16)

#define BSP_LCD_HSYNC_PULSE_WIDTH     (1)
#define BSP_LCD_HSYNC_BACK_PORCH      (40)
#define BSP_LCD_HSYNC_FRONT_PORCH     (20)
#define BSP_LCD_VSYNC_PULSE_WIDTH     (1)
#define BSP_LCD_VSYNC_BACK_PORCH      (10)
#define BSP_LCD_VSYNC_FRONT_PORCH     (5)

#define BSP_LCD_ROTATION_DEFAULT      ESP_LV_ADAPTER_ROTATE_0
#define BSP_LCD_TEAR_AVOID_MODE       ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB

/**
 * @brief BSP display configuration
 */
typedef struct {
    esp_lv_adapter_rotation_t rotation;               /*!< LVGL display rotation */
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode; /*!< LVGL/RGB tear avoidance mode */
    uint16_t buffer_height;                           /*!< LVGL draw buffer height, 0 uses adapter default */
    uint32_t task_stack_size;                         /*!< LVGL task stack size, 0 uses adapter default */
    bool enable_ppa_accel;                            /*!< Enable PPA acceleration, false uses adapter default */
    bool enable_touch;                                /*!< Enable GT1151 touch registration */
} bsp_display_config_t;

#define BSP_DISPLAY_DEFAULT_CONFIG() {                                      \
    .rotation        = BSP_LCD_ROTATION_DEFAULT,                            \
    .tear_avoid_mode = BSP_LCD_TEAR_AVOID_MODE,                             \
    .buffer_height   = 0,                                                   \
    .task_stack_size = 0,                                                   \
    .enable_ppa_accel = false,                                              \
    .enable_touch    = true,                                                \
}

/**
 * @brief Create and initialize the RGB LCD panel.
 *
 * @param[in] config Display configuration. NULL selects BSP_DISPLAY_DEFAULT_CONFIG().
 * @param[out] ret_panel Returned RGB panel handle.
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief Initialize LCD and register it to the local esp_lvgl_adapter.
 *
 * @return LVGL display handle on success, NULL on failure.
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize LCD and register it to the local esp_lvgl_adapter with custom configuration.
 *
 * @param[in] config Display configuration. NULL selects BSP_DISPLAY_DEFAULT_CONFIG().
 *
 * @return LVGL display handle on success, NULL on failure.
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config);

/**
 * @brief Get the LVGL display created by bsp_display_start().
 */
lv_display_t *bsp_display_get(void);

/**
 * @brief Get the RGB LCD panel handle created by the BSP.
 */
esp_lcd_panel_handle_t bsp_display_get_panel(void);

/**
 * @brief Create the GT1151 touch controller used by the RGB 800x480 panel.
 */
esp_err_t bsp_touch_new(esp_lv_adapter_rotation_t rotation, esp_lcd_touch_handle_t *ret_touch);

/**
 * @brief Get the LVGL touch input device registered by bsp_display_start().
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Turn on LCD backlight if the control GPIO is available.
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn off LCD backlight if the control GPIO is available.
 */
esp_err_t bsp_display_backlight_off(void);

/**
 * @brief Take the LVGL adapter lock before calling LVGL APIs.
 *
 * @param timeout_ms Timeout in milliseconds. Use -1 to wait forever.
 *
 * @return true if the lock was taken.
 */
bool bsp_display_lock(int32_t timeout_ms);

/**
 * @brief Release the LVGL adapter lock.
 */
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
