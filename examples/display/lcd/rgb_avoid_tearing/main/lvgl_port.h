/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LVGL related parameters, can be adjusted by users
 *
 */
#define LVGL_H_RES                  (480)
#define LVGL_V_RES                  (480)
#define LVGL_BUFFER_HEIGHT          (CONFIG_EXAMPLE_LVGL_BUF_HEIGHT)
#define LVGL_TICK_PERIOD_MS         (CONFIG_EXAMPLE_LVGL_TICK)

/**
 * LVGL timer handle task related parameters, can be adjusted by users
 *
 */
#define LVGL_PORT_TASK_MAX_DELAY_MS             (CONFIG_EXAMPLE_LVGL_PORT_TASK_MAX_DELAY_MS)       // The maximum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_MIN_DELAY_MS             (CONFIG_EXAMPLE_LVGL_PORT_TASK_MIN_DELAY_MS)         // The minimum delay of the LVGL timer task, in milliseconds
#define LVGL_PORT_TASK_STACK_SIZE               (CONFIG_EXAMPLE_LVGL_TASK_STACK_SIZE_KB * 1024)  // The stack size of the LVGL timer task, in bytes
#define LVGL_PORT_TASK_PRIORITY                 (CONFIG_EXAMPLE_LVGL_TASK_PRIORITY)         // The priority of the LVGL timer task
#define LVGL_PORT_TASK_CORE                     (CONFIG_EXAMPLE_LVGL_PORT_TASK_CORE)        // The core of the LVGL timer task, `-1` means the don't specify the core
// This can be set to `1` only if the SoCs support dual-core,
// otherwise it should be set to `-1` or `0`
/**
 *
 * LVGL buffer related parameters, can be adjusted by users:
 *
 *  (These parameters will be useless if the avoid tearing function is enabled)
 *
 *  - Memory type for buffer allocation:
 *      - MALLOC_CAP_SPIRAM: Allocate LVGL buffer in PSRAM
 *      - MALLOC_CAP_INTERNAL: Allocate LVGL buffer in SRAM
 *
 *      (The SRAM is faster than PSRAM, but the PSRAM has a larger capacity)
 *      (For SPI/QSPI LCD, it is recommended to allocate the buffer in SRAM, because the SPI DMA does not directly support PSRAM now)
 *
 */
#if CONFIG_EXAMPLE_DISPLAY_LVGL_PSRAM
#define LVGL_BUFFER_MALLOC          (MALLOC_CAP_SPIRAM)
#else
#define LVGL_BUFFER_MALLOC          (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif

/**
 * Here, some important configurations will be set based on different anti-tearing modes and rotation angles.
 * No modification is required here.
 *
 * Users should use `lcd_bus->configRgbFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);` to set the buffer number before
 * initializing the LCD bus
 *
 */
#define EXAMPLE_LVGL_ROTATION_DEGREE              (CONFIG_EXAMPLE_LVGL_ROTATION_DEGREE)
#if EXAMPLE_LVGL_ROTATION_DEGREE == 0
#define EXAMPLE_LVGL_ROTATION_NONE    (1)
#define EXAMPLE_LVGL_ROTATION_90      (0)
#define EXAMPLE_LVGL_ROTATION_180     (0)
#define EXAMPLE_LVGL_ROTATION_270     (0)
#elif EXAMPLE_LVGL_ROTATION_DEGREE == 1
#define EXAMPLE_LVGL_ROTATION_NONE    (0)
#define EXAMPLE_LVGL_ROTATION_90      (1)
#define EXAMPLE_LVGL_ROTATION_180     (0)
#define EXAMPLE_LVGL_ROTATION_270     (0)
#elif EXAMPLE_LVGL_ROTATION_DEGREE == 2
#define EXAMPLE_LVGL_ROTATION_NONE    (0)
#define EXAMPLE_LVGL_ROTATION_90      (0)
#define EXAMPLE_LVGL_ROTATION_180     (1)
#define EXAMPLE_LVGL_ROTATION_270     (0)
#elif EXAMPLE_LVGL_ROTATION_DEGREE == 3
#define EXAMPLE_LVGL_ROTATION_NONE    (0)
#define EXAMPLE_LVGL_ROTATION_90      (0)
#define EXAMPLE_LVGL_ROTATION_180     (0)
#define EXAMPLE_LVGL_ROTATION_270     (1)
#endif

/**
 * Avoid tering related configurations, can be adjusted by users.
 *
 *  (Currently, This function only supports RGB LCD)
 *
 */
/**
 * Set the avoid tearing mode:
 *      - 0: Disable avoid tearing function
 *      - 1: LCD double-buffer & LVGL full-refresh
 *      - 2: LCD triple-buffer & LVGL full-refresh
 *      - 3: LCD double-buffer or triple-buffer & LVGL direct-mode (recommended)
 *
 */
#define EXAMPLE_LVGL_AVOID_TEAR                      (CONFIG_EXAMPLE_LVGL_AVOID_TEAR)
#if EXAMPLE_LVGL_AVOID_TEAR
#define EXAMPLE_LVGL_PORT_AVOID_TEARING_MODE         (CONFIG_EXAMPLE_LVGL_PORT_AVOID_TEARING_MODE)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT         (CONFIG_EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT)
#if LVGL_PORT_AVOID_TEARING_MODE == 1
#define EXAMPLE_LCD_RGB_BUFFER_NUMS   (2)
#define EXAMPLE_LVGL_FULL_REFRESH     (1)
#define EXAMPLE_LVGL_DIRECT_MODE      (0)

#elif EXAMPLE_LVGL_PORT_AVOID_TEARING_MODE == 2
#define EXAMPLE_LCD_RGB_BUFFER_NUMS   (3)
#define EXAMPLE_LVGL_FULL_REFRESH     (1)
#define EXAMPLE_LVGL_DIRECT_MODE      (0)

#elif EXAMPLE_LVGL_PORT_AVOID_TEARING_MODE == 3
#if EXAMPLE_LVGL_ROTATION_DEGREE != 0
#define EXAMPLE_LCD_RGB_BUFFER_NUMS   (3)
#define EXAMPLE_LVGL_DIRECT_MODE      (1)
#define EXAMPLE_LVGL_FULL_REFRESH     (0)
#else
#define EXAMPLE_LCD_RGB_BUFFER_NUMS   (2)
#define EXAMPLE_LVGL_DIRECT_MODE      (1)
#define EXAMPLE_LVGL_FULL_REFRESH     (0)
#endif
#endif

#else
#define EXAMPLE_LCD_RGB_BUFFER_NUMS   (1)
#define EXAMPLE_LVGL_FULL_REFRESH     (0)
#define EXAMPLE_LVGL_DIRECT_MODE      (0)
#endif

/**
 * @brief Initialize LVGL port
 *
 * @param[in] panel_handle: LCD panel handle
 * @param[in] tp: Touch panel handle
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others: Fail
 */
esp_err_t lvgl_port_init(esp_lcd_panel_handle_t panel_handle, esp_lcd_touch_handle_t tp);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void lvgl_port_unlock(void);

#if EXAMPLE_LVGL_AVOID_TEAR
/**
 * @brief Notifies the LVGL task when the transmission of the RGB frame buffer is completed.
 *
 * @param[in] handle: LCD panel handle
 *
 * @return
 *      - true:  Transfer is done
 *      - false: Transfer is NOT done
 */
bool lvgl_port_notify_rgb_vsync(void);
#endif

#ifdef __cplusplus
}
#endif
