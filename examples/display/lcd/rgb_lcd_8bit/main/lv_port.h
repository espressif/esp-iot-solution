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

#define EXAMPLE_LCD_RGB_H_RES              (320)
#define EXAMPLE_LCD_RGB_V_RES              (480)
#define EXAMPLE_LCD_RGB_DATA_WIDTH         (8)
#define EXAMPLE_RGB_BIT_PER_PIXEL          (24)

#define EXAMPLE_PIN_NUM_LCD_RGB_VSYNC      (GPIO_NUM_3)
#define EXAMPLE_PIN_NUM_LCD_RGB_HSYNC      (GPIO_NUM_46)
#define EXAMPLE_PIN_NUM_LCD_RGB_DE         (GPIO_NUM_17)
#define EXAMPLE_PIN_NUM_LCD_RGB_DISP       (GPIO_NUM_NC)
#define EXAMPLE_PIN_NUM_LCD_RGB_PCLK       (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA0      (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA1      (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA2      (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA3      (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA4      (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA5      (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA6      (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_LCD_RGB_DATA7      (GPIO_NUM_48)

#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_LCD_SPI_CS         (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_LCD_SPI_SCK        (GPIO_NUM_40)
#define EXAMPLE_PIN_NUM_LCD_SPI_SDO        (GPIO_NUM_42)
#define EXAMPLE_PIN_NUM_BK_LIGHT          (GPIO_NUM_0)

#define EXAMPLE_PIN_NUM_LCD_RST            (GPIO_NUM_2)

#define EXAMPLE_DELAY_TIME_MS              (3000)

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
