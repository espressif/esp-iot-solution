/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Stock Korvo-1 800x480 panel. Pins/timings follow ESP32-S31-Korvo BSP.
 * This is not the GC9503 SUB2 panel, which requires command initialization.
 */
#include "board.h"
#ifdef PRESENT_BOARD_RGB_INFO
#include "board_rgb_info.h"
#endif
#include "esp_lcd_panel_rgb.h"
#include "esp_check.h"
#include "esp_log.h"

#ifndef PRESENT_RGB_BOUNCE_LINES
#define PRESENT_RGB_BOUNCE_LINES 10
#endif

esp_err_t board_s31_rgb_create(esp_lcd_panel_handle_t *panel, size_t buffer_count, unsigned bounce_lines)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 18000000, .h_res = WIDTH, .v_res = HEIGHT,
            .hsync_pulse_width = 1, .hsync_back_porch = 40, .hsync_front_porch = 20,
            .vsync_pulse_width = 1, .vsync_back_porch = 10, .vsync_front_porch = 5,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16, .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = buffer_count, .dma_burst_size = 64,
        .bounce_buffer_size_px = WIDTH * bounce_lines,
        .hsync_gpio_num = 44, .vsync_gpio_num = 45,
        .de_gpio_num = 43, .pclk_gpio_num = 40, .disp_gpio_num = -1,
        .data_gpio_nums = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 33, 34, 35, 36},
        .flags.fb_in_psram = true,
    };
    ESP_LOGI("s31_rgb", "stock panel 800x480 pclk=18000000 fbs=%u bounce_lines=%u",
             (unsigned)buffer_count, bounce_lines);
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, panel), "s31_rgb", "create");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), "s31_rgb", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), "board", "panel init");
#ifdef PRESENT_BOARD_RGB_INFO
    board_rgb_created(*panel, &cfg);
#endif
    return ESP_OK;
}

esp_err_t board_resources_destroy(void)
{
#ifdef PRESENT_BOARD_RGB_INFO
    board_rgb_forget();
#endif
    return ESP_OK; /* No command bus, expander, or backlight GPIO on stock panel. */
}

esp_err_t board_panel_create(esp_lcd_panel_handle_t *panel, size_t buffer_count)
{
    return board_s31_rgb_create(panel, buffer_count, PRESENT_RGB_BOUNCE_LINES);
}
