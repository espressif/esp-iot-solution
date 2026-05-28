/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *dummy;
} bsp_touch_config_t;

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
