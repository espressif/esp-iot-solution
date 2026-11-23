/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "panel.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st7701.h"

#define BOUNCE_BUFFER_LINES  20
#define LCD_BACKLIGHT_GPIO   GPIO_NUM_38
#define STANDBY_PCLK_HZ       (8 * 1000 * 1000)

static const esp_lcd_rgb_timing_t normal_timing = ST7701_480_480_PANEL_60HZ_RGB_TIMING();

// Initialization sequence for the ST7701 module used by rgb_avoid_tearing.
static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t []){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t []){0x31, 0x05}, 2, 0},
    {0xCD, (uint8_t []){0x00}, 1, 0},
    {0xB0, (uint8_t []){0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}, 16, 0},
    {0xB1, (uint8_t []){0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t []){0x60}, 1, 0},
    {0xB1, (uint8_t []){0x32}, 1, 0},
    {0xB2, (uint8_t []){0x07}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x49}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x21}, 1, 0},
    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x1B, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t []){0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}, 16, 0},
    {0xE6, (uint8_t []){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}, 16, 0},
    {0xEB, (uint8_t []){0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}, 7, 0},
    {0xEC, (uint8_t []){0x3C, 0x00}, 2, 0},
    {0xED, (uint8_t []){0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}, 16, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE5, (uint8_t []){0xE4}, 1, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 0},
};

static clock_panel_t open_panel(void)
{
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = GPIO_NUM_39,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = GPIO_NUM_48,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = GPIO_NUM_47,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dma_burst_size = 64,
        .data_width = 16, // The RGB driver defaults to RGB565 for a 16-bit bus.
        .de_gpio_num = GPIO_NUM_18,
        .pclk_gpio_num = GPIO_NUM_21,
        .vsync_gpio_num = GPIO_NUM_17,
        .hsync_gpio_num = GPIO_NUM_16,
        .disp_gpio_num = -1,
        .data_gpio_nums = { // D0 through D15 in bus order
            GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
            GPIO_NUM_15, GPIO_NUM_8, GPIO_NUM_20, GPIO_NUM_3,
            GPIO_NUM_46, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11,
            GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_0,
        },
        .timings = normal_timing,
        .flags.fb_in_psram = 1,
        .num_fbs = 1,
        .bounce_buffer_size_px = normal_timing.h_res * BOUNCE_BUFFER_LINES,
    };
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags.mirror_by_cmd = 1,
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 18,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t lcd_handle;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_handle, true));

    ESP_ERROR_CHECK(gpio_set_direction(LCD_BACKLIGHT_GPIO, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LCD_BACKLIGHT_GPIO, 1));
    void *framebuffer;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(lcd_handle, 1, &framebuffer));
    return (clock_panel_t) {
        .handle = lcd_handle,
        .pixels = framebuffer,
        .width = normal_timing.h_res,
        .height = normal_timing.v_res,
    };
}

clock_panel_t clock_panel_set_mode(display_mode_t mode)
{
    static clock_panel_t panel;
    if (!panel.handle) {
        panel = open_panel();
    }
    // The RGB driver applies the new clock at a frame boundary.
    const uint32_t pixel_clock_hz = mode == DISPLAY_MODE_STANDBY ? STANDBY_PCLK_HZ : normal_timing.pclk_hz;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_set_pclk(panel.handle, pixel_clock_hz));
    return panel;
}
