/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lv_adapter_display.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#define CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI 1
#elif CONFIG_IDF_TARGET_ESP32S3
#define CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM 1
#elif CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM 1
#else
#error "Unsupported target for LVGL adapter test app"
#endif

#ifndef CONFIG_EXAMPLE_DISPLAY_ROTATION_0
#define CONFIG_EXAMPLE_DISPLAY_ROTATION_0 1
#endif

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
#define HW_LCD_H_RES    1024
#define HW_LCD_V_RES    600
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
#define HW_LCD_H_RES    320
#define HW_LCD_V_RES    240
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0
#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM
#define HW_LCD_H_RES    240
#define HW_LCD_V_RES    240
#define HW_USE_TOUCH    0
#define HW_USE_ENCODER  1
#else
#error "No LCD interface selected!"
#endif

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle,
                      esp_lcd_panel_io_handle_t *io_handle,
                      esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                      esp_lv_adapter_rotation_t rotation);

#if HW_USE_TOUCH
#include "esp_lcd_touch.h"

esp_err_t hw_touch_init(esp_lcd_touch_handle_t *touch_handle,
                        esp_lv_adapter_rotation_t rotation);

#endif

#if HW_USE_ENCODER
#include "iot_knob.h"
#include "iot_button.h"

const knob_config_t *hw_knob_get_config(void);
button_handle_t hw_knob_get_button(void);

#endif

#ifdef __cplusplus
}
#endif
