/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_LCD_H_RES                       (1024)
#define EXAMPLE_LCD_V_RES                       (600)
#define EXAMPLE_LCD_BIT_PER_PIXEL               (16)
#define EXAMPLE_LCD_COLOR_SPACE                 (LCD_RGB_ELEMENT_ORDER_RGB)

esp_err_t example_lcd_init(esp_lcd_panel_handle_t *panel,
                           esp_lcd_panel_io_handle_t *io);

#ifdef __cplusplus
}
#endif
