/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_LCD_H_RES           (400)
#define EXAMPLE_LCD_V_RES           (400)
#define EXAMPLE_LCD_HOST            (SPI2_HOST)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL (!EXAMPLE_LCD_BK_LIGHT_ON_LEVEL)
#define EXAMPLE_LCD_FB_BUF_NUM      (CONFIG_EXAMPLE_LCD_BUFFER_NUMS)

#define EXAMPLE_PIN_NUM_LCD_CS      (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_SCK     (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_D0      (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_D1      (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_D2      (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_D3      (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_BK_LIGHT    (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_LCD_RST     (GPIO_NUM_15)

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (5 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#define EXAMPLE_LVGL_BUFFER_HEIGHT          (CONFIG_EXAMPLE_DISPLAY_LVGL_BUF_HEIGHT)
#if CONFIG_EXAMPLE_DISPLAY_LVGL_PSRAM
#define EXAMPLE_LVGL_BUFFER_MALLOC          (MALLOC_CAP_SPIRAM)
#else
#define EXAMPLE_LVGL_BUFFER_MALLOC          (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif

#if CONFIG_LV_COLOR_DEPTH == 32
#define EXAMPLE_LCD_BIT_PER_PIXEL       24
#if CONFIG_EXAMPLE_DISPLAY_LVGL_AVOID_TEAR
#error "Anti-tearing is not supported when CONFIG_LV_COLOR_DEPTH == 32"
#endif
#elif CONFIG_LV_COLOR_DEPTH == 16
#define EXAMPLE_LCD_BIT_PER_PIXEL       16
#else
#error "Unsupported color depth"
#endif

void lv_port_init(void);

bool lv_port_lock(uint32_t timeout_ms);

void lv_port_unlock(void);

#ifdef __cplusplus
}
#endif
