/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lcd_ek79007.h"
#include "app_lcd.h"

static const char *TAG = "app_lcd";

#define EXAMPLE_LDO_MIPI_CHAN               (3)
#define EXAMPLE_LDO_MIPI_VOLTAGE_MV         (2500)

#define EXAMPLE_PIN_NUM_LCD_RST             (27)

static esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
static esp_lcd_panel_io_handle_t mipi_dbi_io;
static esp_lcd_panel_handle_t display_handle;

static void lcd_ldo_power_on(void)
{
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_LDO_MIPI_CHAN,
        .voltage_mv = EXAMPLE_LDO_MIPI_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
}

esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle)
{
    lcd_ldo_power_on();

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install LCD driver of ek79007");
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(EXAMPLE_MIPI_DPI_PX_FORMAT);
    dpi_config.num_fbs = EXAMPLE_LCD_BUF_NUM;
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, &display_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(display_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(display_handle));

    *panel_handle = display_handle;

    return ESP_OK;
}
