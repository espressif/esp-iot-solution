/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Host driver boundary double; never on ESP-IDF's include path. */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef struct fake_lcd *esp_lcd_panel_io_handle_t;
typedef struct {
    unsigned unused;
} esp_lcd_panel_io_event_data_t;
typedef bool (*esp_lcd_panel_io_color_trans_done_cb_t)(esp_lcd_panel_io_handle_t,
                                                       esp_lcd_panel_io_event_data_t *, void *);
typedef struct {
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done;
} esp_lcd_panel_io_callbacks_t;
esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t,
                                                    const esp_lcd_panel_io_callbacks_t *, void *);
