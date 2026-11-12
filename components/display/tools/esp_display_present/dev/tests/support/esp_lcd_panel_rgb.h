/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_lcd_panel_ops.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    unsigned unused;
} esp_lcd_rgb_panel_event_data_t;
typedef bool (*rgb_cb_t)(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *);
typedef struct {
    rgb_cb_t on_color_trans_done, on_frame_buf_complete, on_vsync;
} esp_lcd_rgb_panel_event_callbacks_t;
esp_err_t esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_callbacks_t *, void *);
esp_err_t esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t, uint32_t, void **, ...);

#include <stddef.h>
#ifndef PRESENT_HOST_RGB_NO_FRAME_BUFFER_INFO
#define ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO 1
typedef struct {
    uint32_t width, height;
    size_t frame_buffer_count, frame_buffer_bytes, bounce_buffer_bytes;
    unsigned bits_per_pixel;
    bool continuous_refresh, custom_bounce_callback, transformed;
    bool frame_buffer_retirement;
} esp_lcd_rgb_panel_frame_buffer_info_t;
esp_err_t esp_lcd_rgb_panel_get_frame_buffer_info(esp_lcd_panel_handle_t panel,
                                                  esp_lcd_rgb_panel_frame_buffer_info_t *info);

#endif /* PRESENT_HOST_RGB_NO_FRAME_BUFFER_INFO */
