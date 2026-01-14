/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lcd_init.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_ek79007.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "lcd_pattern_test.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_IO_RST                      (GPIO_NUM_27)             // -1 if not used
#define EXAMPLE_PIN_NUM_BK_LIGHT                (-1)                      // -1 if not used
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL           (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL          !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)              // LDO_VO3 is connected to VDD_MIPI_DPHY
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)
#define EXAMPLE_LCD_MIPI_DSI_LANE_NUM           (2)              // 2 data lanes
#define EXAMPLE_LCD_MIPI_DSI_LANE_BITRATE_MBPS  (1000)           // 1Gbps

static const char *TAG = "example_lcd_init";

static void example_wait_for_refresh(void *user_ctx);

IRAM_ATTR static bool example_notify_refresh_ready(esp_lcd_panel_handle_t panel,
                                                   esp_lcd_dpi_panel_event_data_t *edata,
                                                   void *user_ctx)
{
    (void)panel;
    (void)edata;
    SemaphoreHandle_t refresh_done = (SemaphoreHandle_t)user_ctx;
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(refresh_done, &need_yield);
    return (need_yield == pdTRUE);
}

static esp_err_t example_enable_dsi_phy_power(void)
{
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");

    return ESP_OK;
}

esp_err_t example_lcd_init(esp_lcd_panel_handle_t *panel,
                           esp_lcd_panel_io_handle_t *io)
{
    ESP_RETURN_ON_FALSE(panel && io, ESP_ERR_INVALID_ARG, TAG, "invalid args");

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif

#if EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN > 0
    ESP_LOGI(TAG, "Enable MIPI DSI PHY power");
    ESP_ERROR_CHECK(example_enable_dsi_phy_power());
#endif

    ESP_LOGI(TAG, "Create MIPI DSI bus");
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = EXAMPLE_LCD_MIPI_DSI_LANE_NUM,
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = EXAMPLE_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, io));

    ESP_LOGI(TAG, "Install EK79007 LCD control panel");
#if EXAMPLE_LCD_BIT_PER_PIXEL == 24
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif

    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = EXAMPLE_LCD_MIPI_DSI_LANE_NUM,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .rgb_ele_order = EXAMPLE_LCD_COLOR_SPACE,
        .reset_gpio_num = EXAMPLE_LCD_IO_RST,
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(*io, &lcd_dev_config, panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel));

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    SemaphoreHandle_t refresh_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(refresh_done, ESP_ERR_NO_MEM, TAG, "no mem for semaphore");
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_notify_refresh_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(*panel, &cbs, refresh_done));
    lcd_pattern_test_set_wait_callback(example_wait_for_refresh, refresh_done);

    return ESP_OK;
}

static void example_wait_for_refresh(void *user_ctx)
{
    SemaphoreHandle_t refresh_done = (SemaphoreHandle_t)user_ctx;
    xSemaphoreTake(refresh_done, portMAX_DELAY);
}
