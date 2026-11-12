/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_lcd_panel_ops.h"
/* Private platform/BSP boundary, not a presenter application configuration.
 * Describe the initialized panel, never a preferred completion protocol. */
typedef struct {
    uint32_t width, height;
    size_t frame_buffer_count, frame_buffer_bytes, bounce_buffer_bytes;
    unsigned bits_per_pixel;
    bool continuous_refresh, custom_bounce_callback, transformed;
} present_lcd_rgb_info_t;
typedef struct {
    present_lcd_rgb_info_t panel;
    bool retirement;
} present_lcd_rgb_facts_t;
/* Driver query has precedence. NOT_FOUND means no facts are available, not
 * that a known configuration is unsupported. Other errors fail closed. */
esp_err_t present_lcd_rgb_get_facts(esp_lcd_panel_handle_t panel, present_lcd_rgb_facts_t *facts);
/* Optional private BSP binding for IDF versions without a driver query.
 * Default returns NOT_FOUND. A binding must match the handle and panel lifetime,
 * report actual initialization and subsequent transforms, and return NOT_FOUND
 * for handles it does not own. It does not intercept or alter LCD APIs. */
esp_err_t present_lcd_rgb_get_board_info(esp_lcd_panel_handle_t panel, present_lcd_rgb_info_t *info);
