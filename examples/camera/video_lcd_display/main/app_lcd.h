/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_LCD_H
#define APP_LCD_H

#include <stddef.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define EXAMPLE_LCD_H_RES        1024
#define EXAMPLE_LCD_V_RES        600
#define EXAMPLE_LCD_COPY_BUF_NUM   2
#define EXAMPLE_LCD_SHARED_BUF_NUM 3

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define EXAMPLE_LCD_BIT_PER_PIXEL 16
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define EXAMPLE_LCD_BIT_PER_PIXEL 24
#endif

#define EXAMPLE_LCD_BUF_SIZE     ((size_t)EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_LCD_BIT_PER_PIXEL / 8)

/** Initialize the panel. Pin the sole producer to the returned DMA interrupt core. */
esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle, bool shared, int *draw_core);

/** Shared mode returns the retired LCD index, retaining the submitted buffer.
 * Copy mode consumes the source before return and sets retired_index to -1. */
esp_err_t app_lcd_draw_bitmap(esp_lcd_panel_handle_t panel, int width, int height, const void *buffer, int *retired_index);

#endif
