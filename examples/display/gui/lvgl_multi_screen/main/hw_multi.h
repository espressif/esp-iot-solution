/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lv_adapter_display.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES   (240)
#define BSP_LCD_V_RES   (240)

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#define BSP_LCD_DATA0   (GPIO_NUM_38)
#define BSP_LCD_PCLK    (GPIO_NUM_45)
#define BSP_LCD_DC      (GPIO_NUM_47)
#define BSP_LCD_RST     (GPIO_NUM_21)
#define BSP_LCD_CS_0    (GPIO_NUM_41)
#define BSP_LCD_CS_1    (GPIO_NUM_48)
#define BSP_LCD_BL      (GPIO_NUM_39)
#elif CONFIG_IDF_TARGET_ESP32C3
#define BSP_LCD_DATA0   (GPIO_NUM_0)
#define BSP_LCD_PCLK    (GPIO_NUM_1)
#define BSP_LCD_DC      (GPIO_NUM_7)
#define BSP_LCD_RST     (GPIO_NUM_2)
#define BSP_LCD_CS_0    (GPIO_NUM_5)
#define BSP_LCD_CS_1    (GPIO_NUM_8)
#define BSP_LCD_BL      (GPIO_NUM_3)
#else
#error "Unsupported target for lvgl_multi_screen example"
#endif

#define HW_MULTI_DISPLAY_COUNT   (2)

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
} hw_multi_panel_t;

esp_err_t hw_multi_lcd_init(hw_multi_panel_t *out_panels,
                            size_t panel_slots,
                            esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                            esp_lv_adapter_rotation_t rotation,
                            size_t *out_count);

#ifdef __cplusplus
}
#endif
