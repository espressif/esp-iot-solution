/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_lcd_panel_rgb.h"
/* Test BSP owns one panel. Record after successful initialization and forget
 * after deletion; no concurrent panel creation/destruction is permitted. */
void board_rgb_created(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_config_t *config);
void board_rgb_forget(void);
