/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_adapter.h"
#include "lv_demos.h"

// LCD controller drivers
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
#include "esp_lcd_spd2010.h"
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
#include "esp_lcd_gc9b71.h"
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
#include "esp_lcd_sh8601.h"
#endif

// Touch controller drivers
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_SPD2010
#include "esp_lcd_touch_spd2010.h"
#elif CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S
#include "esp_lcd_touch_cst816s.h"
#endif

static const char *TAG = "example";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// LCD Configuration ////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define EXAMPLE_LCD_HOST               (SPI2_HOST)
#define EXAMPLE_PIN_NUM_LCD_CS         (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_PCLK       (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA0      (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA1      (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA2      (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA3      (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST        (GPIO_NUM_0)
#define EXAMPLE_PIN_NUM_BK_LIGHT       (GPIO_NUM_1)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  (1)

// LCD resolution configuration
#if CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
#define EXAMPLE_LCD_H_RES              (412)
#define EXAMPLE_LCD_V_RES              (412)
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
#define EXAMPLE_LCD_H_RES              (320)
#define EXAMPLE_LCD_V_RES              (386)
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
#define EXAMPLE_LCD_H_RES              (454)
#define EXAMPLE_LCD_V_RES              (454)
#endif

// Color depth configuration
#if CONFIG_LV_COLOR_DEPTH_32
#define LCD_BIT_PER_PIXEL              (24)
#elif CONFIG_LV_COLOR_DEPTH_16
#define LCD_BIT_PER_PIXEL              (16)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Touch Configuration //////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#define EXAMPLE_TOUCH_HOST             (I2C_NUM_0)
#define EXAMPLE_PIN_NUM_TOUCH_SCL      (GPIO_NUM_18)
#define EXAMPLE_PIN_NUM_TOUCH_SDA      (GPIO_NUM_8)
#define EXAMPLE_PIN_NUM_TOUCH_RST      (GPIO_NUM_NC)  // Not used
#define EXAMPLE_PIN_NUM_TOUCH_INT      (GPIO_NUM_2)
#endif

void app_main(void)
{
    // Turn off backlight before LCD initialization
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    // Initialize SPI bus
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg =
#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
        GC9B71_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0,
                                     EXAMPLE_PIN_NUM_LCD_DATA1, EXAMPLE_PIN_NUM_LCD_DATA2,
                                     EXAMPLE_PIN_NUM_LCD_DATA3, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
        SPD2010_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0,
                                      EXAMPLE_PIN_NUM_LCD_DATA1, EXAMPLE_PIN_NUM_LCD_DATA2,
                                      EXAMPLE_PIN_NUM_LCD_DATA3, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
        SH8601_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK, EXAMPLE_PIN_NUM_LCD_DATA0,
                                     EXAMPLE_PIN_NUM_LCD_DATA1, EXAMPLE_PIN_NUM_LCD_DATA2,
                                     EXAMPLE_PIN_NUM_LCD_DATA3, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
#endif
    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Install panel IO
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =
#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
        GC9B71_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
        SPD2010_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
        SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
#endif
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

    // Configure vendor-specific settings
#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
    gc9b71_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
    spd2010_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
    sh8601_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,
        },
    };
#endif

    // Install LCD driver
    ESP_LOGI(TAG, "Install LCD driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

#if CONFIG_EXAMPLE_LCD_CONTROLLER_GC9B71
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9b71(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SPD2010
    ESP_ERROR_CHECK(esp_lcd_new_panel_spd2010(io_handle, &panel_config, &panel_handle));
#elif CONFIG_EXAMPLE_LCD_CONTROLLER_SH8601
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // Initialize touch controller if enabled
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    esp_lcd_touch_handle_t tp_handle = NULL;
    ESP_LOGI(TAG, "Initialize I2C bus for touch");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400 * 1000,
    };
    ESP_ERROR_CHECK(i2c_param_config(EXAMPLE_TOUCH_HOST, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(EXAMPLE_TOUCH_HOST, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_SPD2010
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG();
#elif CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
#endif
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_HOST, &tp_io_config, &tp_io_handle));

    ESP_LOGI(TAG, "Initialize touch controller");
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

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_SPD2010
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_spd2010(tp_io_handle, &tp_cfg, &tp_handle));
#elif CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_CST816S
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp_handle));
#endif
#endif // CONFIG_EXAMPLE_LCD_TOUCH_ENABLED

    // Initialize LVGL adapter
    ESP_LOGI(TAG, "Initialize LVGL adapter");
    const esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_config));

    // Register display (LCD with GRAM uses SPI with PSRAM default config)
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                         panel_handle,
                                                         io_handle,
                                                         EXAMPLE_LCD_H_RES,
                                                         EXAMPLE_LCD_V_RES,
                                                         ESP_LV_ADAPTER_ROTATE_0  // Rotation not supported for QSPI LCD
                                                     );

    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to register display");
        return;
    }

    // Register touch input device if available
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&touch_config);
    if (!touch) {
        ESP_LOGE(TAG, "Failed to register touch input");
        return;
    }
#endif

    // Start LVGL adapter task
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    // Turn on backlight
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
#endif

    // Display LVGL demo
    ESP_LOGI(TAG, "Display LVGL demo");
    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lv_demo_music();  // You can also try: lv_demo_widgets(), lv_demo_benchmark()
        esp_lv_adapter_unlock();
    }
}
