/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_ops.h"

typedef enum {
    DISPLAY_MODE_NORMAL,
    DISPLAY_MODE_STANDBY,
} display_mode_t;

typedef struct {
    esp_lcd_panel_handle_t handle;
    uint16_t *pixels;
    int width;
    int height;
} clock_panel_t;

// Call from the clock task between draws. The driver owns the framebuffer.
// Always use the returned handle and pixels after switching modes.
clock_panel_t clock_panel_set_mode(display_mode_t mode);
