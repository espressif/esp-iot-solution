/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the display, touch controller and LVGL adapter. */
esp_err_t app_lcd_init(void);

/** Lock LVGL; timeout_ms == 0 waits indefinitely. */
bool app_lcd_lock(uint32_t timeout_ms);

void app_lcd_unlock(void);

#ifdef __cplusplus
}
#endif
