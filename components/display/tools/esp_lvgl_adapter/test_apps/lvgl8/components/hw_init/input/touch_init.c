/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/i2c_master.h"
#include <stdbool.h>
#include "esp_log.h"
#include "hw_init.h"
#include "input/touch_rotation_helper.h"

#if HW_USE_TOUCH

#if CONFIG_EXAMPLE_LCD_INTERFACE_RGB
#include "esp_lcd_touch_gt1151.h"
#define HW_LCD_TOUCH_RST                (GPIO_NUM_NC)
#define HW_LCD_TOUCH_INT                (GPIO_NUM_NC)
#define HW_I2C_SDA                      (GPIO_NUM_47)
#define HW_I2C_SCL                      (GPIO_NUM_48)
#define TOUCH_CONTROLLER_NAME           "GT1151"
#define TOUCH_ROTATION_TYPE             TOUCH_ROTATION_STANDARD
#define TOUCH_CONTROLLER_TYPE_GT1151    1

#elif CONFIG_EXAMPLE_LCD_INTERFACE_MIPI_DSI
#include "esp_lcd_touch_gt911.h"
#define HW_LCD_TOUCH_RST                (GPIO_NUM_NC)
#define HW_LCD_TOUCH_INT                (GPIO_NUM_NC)
#define HW_I2C_SDA                      (GPIO_NUM_7)
#define HW_I2C_SCL                      (GPIO_NUM_8)
#define TOUCH_CONTROLLER_NAME           "GT911"
#define TOUCH_ROTATION_TYPE             TOUCH_ROTATION_MIPI_DSI
#define TOUCH_CONTROLLER_TYPE_GT911     1

#elif CONFIG_EXAMPLE_LCD_INTERFACE_QSPI
#include "esp_lcd_touch_cst816s.h"
#define HW_LCD_TOUCH_RST                (GPIO_NUM_NC)
#define HW_LCD_TOUCH_INT                (GPIO_NUM_10)
#define HW_I2C_SDA                      (GPIO_NUM_2)
#define HW_I2C_SCL                      (GPIO_NUM_1)
#define TOUCH_CONTROLLER_NAME           "CST816S"
#define TOUCH_ROTATION_TYPE             TOUCH_ROTATION_STANDARD
#define TOUCH_CONTROLLER_TYPE_CST816S   1

#elif CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM
#include "esp_lcd_touch_gt911.h"
#define HW_LCD_TOUCH_RST                (GPIO_NUM_NC)
#define HW_LCD_TOUCH_INT                (GPIO_NUM_3)
#define HW_I2C_SDA                      (GPIO_NUM_8)
#define HW_I2C_SCL                      (GPIO_NUM_18)
#define TOUCH_CONTROLLER_NAME           "GT911"
#define TOUCH_ROTATION_TYPE             TOUCH_ROTATION_STANDARD
#define TOUCH_CONTROLLER_TYPE_GT911     1

#else
#include "esp_lcd_touch_gt911.h"
#define HW_LCD_TOUCH_RST                (GPIO_NUM_NC)
#define HW_LCD_TOUCH_INT                (GPIO_NUM_NC)
#define HW_I2C_SDA                      (GPIO_NUM_7)
#define HW_I2C_SCL                      (GPIO_NUM_8)
#define TOUCH_CONTROLLER_NAME           "GT911"
#define TOUCH_ROTATION_TYPE             TOUCH_ROTATION_STANDARD
#define TOUCH_CONTROLLER_TYPE_GT911     1
#endif

#define HW_I2C_NUM                          (I2C_NUM_0)
#define HW_I2C_CLK_SPEED_HZ                 (400000)

static const char *TAG = "hw_touch_init";
static i2c_master_bus_handle_t s_touch_i2c_bus = NULL;

static esp_err_t hw_touch_i2c_init(void)
{
    if (s_touch_i2c_bus) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = HW_I2C_SDA,
        .scl_io_num = HW_I2C_SCL,
        .i2c_port = HW_I2C_NUM,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &s_touch_i2c_bus));

    return ESP_OK;
}

esp_err_t hw_touch_init(esp_lcd_touch_handle_t *ret_touch, esp_lv_adapter_rotation_t rotation)
{
    ESP_LOGI(TAG, "Initializing touch controller (%s)", TOUCH_CONTROLLER_NAME);

    ESP_ERROR_CHECK(hw_touch_i2c_init());

    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    touch_get_rotation_flags(TOUCH_ROTATION_TYPE, rotation, &swap_xy, &mirror_x, &mirror_y);

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = HW_LCD_H_RES,
        .y_max = HW_LCD_V_RES,
        .rst_gpio_num = HW_LCD_TOUCH_RST,
        .int_gpio_num = HW_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    /* Initialize touch controller based on type */
#if defined(TOUCH_CONTROLLER_TYPE_GT1151)
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT1151_CONFIG();
    tp_io_config.scl_speed_hz = HW_I2C_CLK_SPEED_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &tp_io_config, &tp_io_handle));
    return esp_lcd_touch_new_i2c_gt1151(tp_io_handle, &tp_cfg, ret_touch);

#elif defined(TOUCH_CONTROLLER_TYPE_CST816S)
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_config.scl_speed_hz = HW_I2C_CLK_SPEED_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &tp_io_config, &tp_io_handle));
    return esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, ret_touch);

#elif defined(TOUCH_CONTROLLER_TYPE_GT911)
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = HW_I2C_CLK_SPEED_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &tp_io_config, &tp_io_handle));
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);

#else
#error "No touch controller type defined"
#endif
}

#endif
