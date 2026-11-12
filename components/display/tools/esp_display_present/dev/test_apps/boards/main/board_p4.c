/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
/* Pins/timing: esp32_p4_function_ev_board_noglib 5.2.3, 1024x600 panel. */
#include "board.h"
#include "driver/gpio.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_ek79007.h"
#include "esp_ldo_regulator.h"
#include "esp_check.h"

static esp_ldo_channel_handle_t ldo;
static esp_lcd_dsi_bus_handle_t bus;
static esp_lcd_panel_io_handle_t io;

esp_err_t board_panel_create(esp_lcd_panel_handle_t *panel, size_t buffer_count)
{
    esp_ldo_channel_config_t power = {.chan_id = 3, .voltage_mv = 2500};
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&power, &ldo), "board", "DSI power");
    esp_lcd_dsi_bus_config_t dsi = {.bus_id = 0, .num_data_lanes = 2, .lane_bit_rate_mbps = 1000};
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&dsi, &bus), "board", "DSI bus");
    esp_lcd_dbi_io_config_t dbi = {.virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8};
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dbi, &io), "board", "DBI IO");
    esp_lcd_dpi_panel_config_t dpi = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
    dpi.num_fbs = buffer_count;
    ek79007_vendor_config_t vendor = {.mipi_config = {.dsi_bus = bus, .dpi_config = &dpi}};
    esp_lcd_panel_dev_config_t dev = {.reset_gpio_num = 27, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                      .bits_per_pixel = 16, .vendor_config = &vendor
                                     };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ek79007(io, &dev, panel), "board", "EK79007");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), "board", "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), "board", "init");
    gpio_config_t backlight = {.pin_bit_mask = 1ULL << 26, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&backlight), "board", "backlight config");
    return gpio_set_level(26, 1);
}

esp_err_t board_resources_destroy(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(26, 0), "board", "backlight off");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(io), "board", "delete DBI");
    io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_del_dsi_bus(bus), "board", "delete DSI");
    bus = NULL;
    ESP_RETURN_ON_ERROR(esp_ldo_release_channel(ldo), "board", "release LDO");
    ldo = NULL;
    return gpio_reset_pin(26);
}
