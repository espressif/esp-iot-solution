/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_err.h"
#include "lcd_pattern_test.h"
#include "lcd_init.h"

void app_main()
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;

    ESP_ERROR_CHECK(example_lcd_init(&panel, &io));
    (void)io;

    lcd_pattern_test_config_t test_config = {
        .h_res = EXAMPLE_LCD_H_RES,
        .v_res = EXAMPLE_LCD_V_RES,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .color_space = EXAMPLE_LCD_COLOR_SPACE,
    };

    lcd_pattern_test_run(panel, &test_config);
}
