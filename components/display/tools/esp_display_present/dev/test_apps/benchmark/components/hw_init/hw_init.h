/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#define HW_LCD_H_RES BENCH_WIDTH
#define HW_LCD_V_RES BENCH_HEIGHT
typedef enum {HW_ROTATE_0 = 0} hw_rotation_t;
esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *, esp_lcd_panel_io_handle_t *, uint8_t, hw_rotation_t);
