/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_gsp.h"

esp_gsp_err_t showcase_ui_init(esp_gsp_handle_t ui, const uint8_t *badge, size_t badge_size);
void showcase_ui_deinit(esp_gsp_handle_t ui);

/* These are also used by the touch-free 240x240 board's encoder. */
void showcase_ui_knob_turn(esp_gsp_handle_t ui, int direction);
void showcase_ui_knob_click(esp_gsp_handle_t ui);
void showcase_ui_knob_long(esp_gsp_handle_t ui);
