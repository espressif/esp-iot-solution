/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"

#define LCD_PPA_BLEND_OUT_H_RES             (1024)
#define LCD_PPA_BLEND_OUT_V_RES             (600)

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define LCD_PPA_BLEND_OUT_COLOR_BITS        (16)
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define LCD_PPA_BLEND_OUT_COLOR_BITS        (24)
#endif

#define LCD_PPA_BLEND_AVOID_TEAR_ENABLE     (CONFIG_EXAMPLE_LCD_PPA_BLEND_AVOID_TEAR)
#if LCD_PPA_BLEND_AVOID_TEAR_ENABLE
#define LCD_PPA_BLEND_FRAME_BUFFER_NUMS     (2)
#else
#define LCD_PPA_BLEND_FRAME_BUFFER_NUMS     (1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LCD PPA blend
 *
 * @param[in] display_panel: LCD panel handle
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed
 */
esp_err_t lcd_ppa_blend_init(esp_lcd_panel_handle_t display_panel);

#ifdef __cplusplus
}
#endif
