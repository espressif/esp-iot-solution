/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVGL_PORT_USE_GPU           (CONFIG_LVGL_PORT_USE_GPU)
// #define LVGL_PORT_USE_GPU           0

/**
 * LVGL related parameters, can be adjusted by users
 *
 */
#define LVGL_PORT_H_RES             (BSP_LCD_H_RES)
#define LVGL_PORT_V_RES             (BSP_LCD_V_RES)
#define LVGL_PORT_TICK_PERIOD_MS    (CONFIG_BSP_DISPLAY_LVGL_TICK)

/**
 * LVGL timer handle task related parameters, can be adjusted by users
 *
 */
#define LVGL_PORT_TASK_MAX_DELAY_MS (CONFIG_BSP_DISPLAY_LVGL_TASK_MAX_DELAY_MS)    // The maximum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_MIN_DELAY_MS (CONFIG_BSP_DISPLAY_LVGL_TASK_MIN_DELAY_MS)    // The minimum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_STACK_SIZE   (CONFIG_BSP_DISPLAY_LVGL_TASK_STACK_SIZE_KB * 1024) // The stack size of the LVGL timer task, in bytes
#define LVGL_PORT_TASK_PRIORITY     (CONFIG_BSP_DISPLAY_LVGL_TASK_PRIORITY)        // The priority of the LVGL timer task
#define LVGL_PORT_TASK_CORE         (CONFIG_BSP_DISPLAY_LVGL_TASK_CORE)            // The core of the LVGL timer task,
// `-1` means the don't specify the core
/**
 *
 * LVGL buffer related parameters, can be adjusted by users:
 *  (These parameters will be useless if the avoid tearing function is enabled)
 *
 *  - Memory type for buffer allocation:
 *      - MALLOC_CAP_SPIRAM: Allocate LVGL buffer in PSRAM
 *      - MALLOC_CAP_INTERNAL: Allocate LVGL buffer in SRAM
 *      (The SRAM is faster than PSRAM, but the PSRAM has a larger capacity)
 *
 */
#if CONFIG_BSP_DISPLAY_LVGL_BUF_PSRAM
#define LVGL_PORT_BUFFER_MALLOC_CAPS    (MALLOC_CAP_SPIRAM)
#elif CONFIG_BSP_DISPLAY_LVGL_BUF_INTERNAL
#define LVGL_PORT_BUFFER_MALLOC_CAPS    (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif
#define LVGL_PORT_BUFFER_HEIGHT         (CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT)
// #define LVGL_PORT_BUFFER_HEIGHT         (1280)

/**
 * Avoid tering related configurations, can be adjusted by users.
 *
 */
#define LVGL_PORT_AVOID_TEAR_ENABLE      (CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR_ENABLE)   // Set to 1 to enable
#if LVGL_PORT_AVOID_TEAR_ENABLE
/**
 * Set the avoid tearing mode:
 *      - 0: LCD double-buffer & LVGL full-refresh
 *      - 1: LCD triple-buffer & LVGL full-refresh
 *      - 2: LCD double-buffer & LVGL direct-mode (recommended)
 *
 */
#define LVGL_PORT_AVOID_TEAR_MODE       (CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR_ENABLE_MODE)
// #define LVGL_PORT_AVOID_TEAR_MODE       (2)

/**
 * Below configurations are automatically set according to the above configurations, users do not need to modify them.
 *
 */
#if LVGL_PORT_AVOID_TEAR_MODE == 0
#define LVGL_PORT_LCD_DPI_BUFFER_NUMS   (2)
#define LVGL_PORT_FULL_REFRESH          (1)
#elif LVGL_PORT_AVOID_TEAR_MODE == 1
#define LVGL_PORT_LCD_DPI_BUFFER_NUMS   (3)
#define LVGL_PORT_FULL_REFRESH          (1)
#elif LVGL_PORT_AVOID_TEAR_MODE == 2
#define LVGL_PORT_LCD_DPI_BUFFER_NUMS   (2)
#define LVGL_PORT_DIRECT_MODE           (1)
#endif /* LVGL_PORT_AVOID_TEAR_MODE */

#else
#define LVGL_PORT_LCD_DPI_BUFFER_NUMS   (1)
#define LVGL_PORT_FULL_REFRESH          (0)
#define LVGL_PORT_DIRECT_MODE           (0)
#endif /* LVGL_PORT_AVOID_TEAR_ENABLE */

/**
 * @brief Initialize LVGL port
 *
 * @param[in] lcd: LCD panel handle
 * @param[in] tp: Touch handle
 * @param[out] disp: LVGL display device
 * @param[out] indev: LVGL input device
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others: Fail
 */
esp_err_t bsp_lvgl_port_init(esp_lcd_panel_handle_t lcd, esp_lcd_touch_handle_t tp, lv_disp_t **disp, lv_indev_t **indev);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool bsp_lvgl_port_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif
