/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lv_adapter_display.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
#define HW_LCD_H_RES    1024
#define HW_LCD_V_RES    600
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0
#elif CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
#define HW_LCD_H_RES    360
#define HW_LCD_V_RES    360
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0
#elif CONFIG_EXAMPLE_LCD_INTERFACE_RGB
#define HW_LCD_H_RES    800
#define HW_LCD_V_RES    480
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
#define HW_LCD_H_RES    320
#define HW_LCD_V_RES    240
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0
#endif

esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle,
                      esp_lcd_panel_io_handle_t *io_handle,
                      esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                      esp_lv_adapter_rotation_t rotation);
esp_err_t hw_lcd_deinit(void);
int hw_lcd_get_te_gpio(void);

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

uint32_t hw_lcd_get_bus_freq_hz(void);
uint8_t hw_lcd_get_bus_data_lines(void);
uint8_t hw_lcd_get_bits_per_pixel(void);

#ifdef __cplusplus
}
#endif
