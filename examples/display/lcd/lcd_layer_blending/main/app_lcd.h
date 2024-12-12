/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APP_LCD_H
#define APP_LCD_H

#include "esp_lcd_touch.h"
#include "esp_lcd_panel_ops.h"

#define EXAMPLE_LCD_H_RES                   (1024)
#define EXAMPLE_LCD_V_RES                   (600)

#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define EXAMPLE_LCD_BIT_PER_PIXEL           (16)
#define EXAMPLE_MIPI_DPI_PX_FORMAT          (LCD_COLOR_PIXEL_FORMAT_RGB565)
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define EXAMPLE_LCD_BIT_PER_PIXEL           (24)
#define EXAMPLE_MIPI_DPI_PX_FORMAT          (LCD_COLOR_PIXEL_FORMAT_RGB888)
#endif

/**
 * @brief Initialize the LCD panel.
 *
 * This function initializes the LCD panel with the provided panel handle. It powers on the LCD,
 * installs the LCD driver, configures the bus, and sets up the panel.
 *
 * @param panel_handle Pointer to the LCD panel handle
 * @return
 *    - ESP_OK: Success
 *    - ESP_FAIL: Failure
 */
esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle);

esp_err_t app_touch_init(esp_lcd_touch_handle_t *tp);

i2c_master_bus_handle_t app_get_i2c_handle(void);

#endif
