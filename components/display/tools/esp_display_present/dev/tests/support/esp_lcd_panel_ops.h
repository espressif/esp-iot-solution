/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"

typedef struct fake_lcd *esp_lcd_panel_handle_t;
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void *);

esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t);
