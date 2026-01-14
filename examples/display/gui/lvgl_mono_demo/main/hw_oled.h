/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HW_OLED_H_RES    128
#define HW_OLED_V_RES    64

esp_err_t hw_oled_init(esp_lcd_panel_handle_t *panel_handle,
                       esp_lcd_panel_io_handle_t *io_handle);

#ifdef __cplusplus
}
#endif
