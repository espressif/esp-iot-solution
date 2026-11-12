/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Pin mapping and panel setup: esp32_s3_lcd_ev_board R8/R16 modules. */
#include "board.h"
#ifdef PRESENT_BOARD_RGB_INFO
#include "board_rgb_info.h"
#endif
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_psram.h"
#include "esp_lcd_gc9503.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_io_expander_tca9554.h"
#include "esp_check.h"

static i2c_master_bus_handle_t bus;
static esp_io_expander_handle_t expander;

esp_err_t board_panel_create(esp_lcd_panel_handle_t *panel, size_t buffer_count)
{
    bool r16 = esp_psram_get_size() > 8 * 1024 * 1024;
    int i2c_sda = r16 ? 47 : 8;
    int i2c_scl = r16 ? 48 : 18;
    int rgb_d6 = r16 ? 8 : 47;
    int rgb_d7 = r16 ? 18 : 48;
    gpio_config_t pins = {.pin_bit_mask = 1ULL << 3, .mode = GPIO_MODE_OUTPUT, .pull_up_en = true};
    ESP_RETURN_ON_ERROR(gpio_config(&pins), "board", "VSYNC config");
    ESP_RETURN_ON_ERROR(gpio_set_level(3, 1), "board", "VSYNC high");
    i2c_master_bus_config_t i2c = {.i2c_port = 0, .sda_io_num = i2c_sda, .scl_io_num = i2c_scl,
                                   .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true
                                  };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c, &bus), "board", "I2C");
    ESP_RETURN_ON_ERROR(esp_io_expander_new_i2c_tca9554(bus, 0x20, &expander), "board", "expander");
    spi_line_config_t lines = {.cs_io_type = IO_TYPE_EXPANDER, .cs_expander_pin = IO_EXPANDER_PIN_NUM_1,
                               .scl_io_type = IO_TYPE_EXPANDER, .scl_expander_pin = IO_EXPANDER_PIN_NUM_2,
                               .sda_io_type = IO_TYPE_EXPANDER, .sda_expander_pin = IO_EXPANDER_PIN_NUM_3, .io_expander = expander
                              };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = GC9503_PANEL_IO_3WIRE_SPI_CONFIG(lines, 0);
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io), "board", "panel IO");
    esp_lcd_rgb_panel_config_t rgb = {
        .clk_src = LCD_CLK_SRC_PLL160M, .dma_burst_size = 64, .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565, .de_gpio_num = 17, .pclk_gpio_num = 9,
        .vsync_gpio_num = 3, .hsync_gpio_num = 46, .disp_gpio_num = -1,
        .data_gpio_nums = {10, 11, 12, 13, 14, 21, rgb_d6, rgb_d7, 45, 38, 39, 40, 41, 42, 2, 1},
        .timings = GC9503_480_480_PANEL_60HZ_RGB_TIMING(), .flags.fb_in_psram = true,
        .num_fbs = buffer_count, .bounce_buffer_size_px = WIDTH * 10,
    };
    gc9503_vendor_config_t vendor = {.rgb_config = &rgb, .flags.auto_del_panel_io = true};
    esp_lcd_panel_dev_config_t dev = {.reset_gpio_num = -1, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                      .bits_per_pixel = 18, .vendor_config = &vendor
                                     };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9503(io, &dev, panel), "board", "GC9503");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), "board", "panel init");
#ifdef PRESENT_BOARD_RGB_INFO
    board_rgb_created(*panel, &rgb);
#endif
    return ESP_OK;
}

esp_err_t board_resources_destroy(void)
{
#ifdef PRESENT_BOARD_RGB_INFO
    board_rgb_forget();
#endif
    ESP_RETURN_ON_ERROR(esp_io_expander_del(expander), "board", "delete expander");
    expander = NULL;
    ESP_RETURN_ON_ERROR(i2c_del_master_bus(bus), "board", "delete I2C");
    bus = NULL;
    return ESP_OK;
}
