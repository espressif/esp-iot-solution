/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"
#include "lcd_ppa_blend.h"
#include "lvgl_ppa_blend.h"
#include "app_lcd.h"

static const char *TAG = "app_lcd";

#define EXAMPLE_LDO_MIPI_CHAN                   (3)
#define EXAMPLE_LDO_MIPI_VOLTAGE_MV             (2500)

#define EXAMPLE_PIN_NUM_LCD_RST                 (27)

#define TOUCH_HOST                              (I2C_NUM_0)
#define EXAMPLE_PIN_NUM_TOUCH_SCL               (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_TOUCH_SDA               (GPIO_NUM_7)
#define EXAMPLE_PIN_NUM_TOUCH_RST               (-1)            // -1 if not used
#define EXAMPLE_PIN_NUM_TOUCH_INT               (-1)            // -1 if not used

static i2c_master_bus_handle_t i2c_handle = NULL;               // I2C Handle

static void lcd_ldo_power_on(void)
{
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_LDO_MIPI_CHAN,
        .voltage_mv = EXAMPLE_LDO_MIPI_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
}

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .i2c_port = TOUCH_HOST,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    return ESP_OK;
}

esp_err_t app_lcd_init(esp_lcd_panel_handle_t *panel_handle)
{
    lcd_ldo_power_on();

    ESP_LOGI(TAG, "Install LCD driver");
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install LCD driver of ek79007");
    esp_lcd_panel_handle_t display_handle;
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(EXAMPLE_MIPI_DPI_PX_FORMAT);
    dpi_config.num_fbs = LCD_PPA_BLEND_FRAME_BUFFER_NUMS;
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

esp_err_t app_touch_init(esp_lcd_touch_handle_t *tp)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    ESP_ERROR_CHECK(i2c_init());

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 400000;

    ESP_LOGI(TAG, "Initialize I2C panel IO");
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle));

    ESP_LOGI(TAG, "Initialize touch controller GT911");
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_touch_handle_t tp_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle));

    *tp = tp_handle;

    return ESP_OK;
}

i2c_master_bus_handle_t app_get_i2c_handle(void)
{
    return i2c_handle;
}
