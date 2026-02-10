/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_ek79007.h"
#include "esp_lv_adapter.h"
#include "lv_demos.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_H_RES                       (1024)
#define EXAMPLE_LCD_V_RES                       (600)
#define EXAMPLE_LCD_BIT_PER_PIXEL               (16)
#define EXAMPLE_LCD_COLOR_SPACE                 (LCD_RGB_ELEMENT_ORDER_RGB)
#define EXAMPLE_LCD_IO_RST                      (GPIO_NUM_27)             // -1 if not used
#define EXAMPLE_PIN_NUM_BK_LIGHT                (-1)                      // -1 if not used
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL           (1)
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL          !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL

#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)              // LDO_VO3 is connected to VDD_MIPI_DPHY
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)
#define EXAMPLE_LCD_MIPI_DSI_LANE_NUM           (2)              // 2 data lanes
#define EXAMPLE_LCD_MIPI_DSI_LANE_BITRATE_MBPS  (1000)           // 1Gbps

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your touch spec ////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
#define TOUCH_HOST                              (I2C_NUM_0)
#define EXAMPLE_PIN_NUM_TOUCH_SCL               (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_TOUCH_SDA               (GPIO_NUM_7)
#define EXAMPLE_PIN_NUM_TOUCH_RST               (-1)            // -1 if not used
#define EXAMPLE_PIN_NUM_TOUCH_INT               (-1)            // -1 if not used
#endif

static const char *TAG = "example";

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

void app_main()
{
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
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = EXAMPLE_LCD_MIPI_DSI_LANE_NUM,
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = EXAMPLE_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    esp_lcd_panel_io_handle_t io = NULL;
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io));

    ESP_LOGI(TAG, "Install EK79007 LCD control panel");
#if EXAMPLE_LCD_BIT_PER_PIXEL == 24
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
#endif

    // Calculate required frame buffer count based on rotation
    esp_lv_adapter_rotation_t rotation = CONFIG_EXAMPLE_LCD_ROTATION;
    dpi_config.num_fbs = esp_lv_adapter_get_required_frame_buffer_count(
                             ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI, rotation);

    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
        .rgb_ele_order = EXAMPLE_LCD_COLOR_SPACE,
        .reset_gpio_num = EXAMPLE_LCD_IO_RST,
        .vendor_config = &vendor_config,
    };

    esp_lcd_panel_handle_t disp_panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &disp_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(disp_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(disp_panel));

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
    esp_lcd_touch_handle_t tp_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_param_config(TOUCH_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(TOUCH_HOST, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    ESP_LOGI(TAG, "Initialize I2C panel IO");
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

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
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle));
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911

    // Initialize LVGL adapter
    ESP_LOGI(TAG, "Initialize LVGL adapter");
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    // Register display to LVGL adapter
    ESP_LOGI(TAG, "Register display to LVGL adapter");
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         disp_panel,
                                                         io,
                                                         EXAMPLE_LCD_H_RES,
                                                         EXAMPLE_LCD_V_RES,
                                                         rotation
                                                     );
    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

    // Register touch input device (if available)
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
    ESP_LOGI(TAG, "Register touch input to LVGL adapter");
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (touch == NULL) {
        ESP_LOGE(TAG, "Failed to register touch");
        return;
    }
#endif

    // Start LVGL adapter task
    ESP_LOGI(TAG, "Start LVGL adapter");
    ESP_ERROR_CHECK(esp_lv_adapter_start());

#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    ESP_LOGI(TAG, "Display LVGL demos");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        // lv_demo_stress();
        lv_demo_benchmark();
        // lv_demo_music();
        // lv_demo_widgets();

        // Release the mutex
        esp_lv_adapter_unlock();
    }
}
