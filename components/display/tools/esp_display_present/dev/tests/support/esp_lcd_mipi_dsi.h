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
} esp_lcd_dpi_panel_event_data_t;
typedef bool (*dpi_cb_t)(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t *, void *);
typedef struct {
    dpi_cb_t on_color_trans_done, on_frame_buf_complete, on_vsync;
} esp_lcd_dpi_panel_event_callbacks_t;
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t, const esp_lcd_dpi_panel_event_callbacks_t *, void *);
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t, uint32_t, void **, ...);
