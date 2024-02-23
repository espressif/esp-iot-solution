/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVGL_H_RES                         (320)
#define LVGL_V_RES                         (480)

#define EXAMPLE_LVGL_TICK_PERIOD_MS        (2)
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS     (500)
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS     (1)
#define EXAMPLE_LVGL_TASK_STACK_SIZE       (8 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY         (2)

#define EXAMPLE_LVGL_BUFFER_HEIGHT         (100)
#define EXAMPLE_LVGL_BUFFER_MALLOC         (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

#if CONFIG_LV_COLOR_DEPTH == 24
#define EXAMPLE_LCD_BIT_PER_PIXEL       24
#else
#error "Unsupported color depth"
#endif

/**
 * @brief Initialize LVGL port
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others: Fail
 */
esp_err_t lv_port_init(esp_lcd_panel_handle_t panel_handle);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool lv_port_lock(int timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void lv_port_unlock(void);

#ifdef __cplusplus
}
#endif
