/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_lcd_panel_ops.h"

/**
 * @brief Initialize the LCD panel
 *
 * @param panel_handle Pointer to the LCD panel handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 */
esp_err_t lcd_init(esp_lcd_panel_handle_t *panel_handle);
