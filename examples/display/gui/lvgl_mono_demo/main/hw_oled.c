/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hw_oled.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "hw_oled";

#define HW_OLED_I2C_HOST            I2C_NUM_0
#define HW_OLED_I2C_SDA_GPIO        8
#define HW_OLED_I2C_SCL_GPIO        7
#define HW_OLED_I2C_FREQ_HZ         (400 * 1000)
#define HW_OLED_I2C_ADDR            0x3C
#define HW_OLED_INVERT_COLOR        false
#define HW_OLED_RESET_GPIO          (-1)
#define HW_OLED_CMD_BITS            8
#define HW_OLED_PARAM_BITS          8
#define HW_OLED_CTRL_PHASE_BYTES    1
#define HW_OLED_DC_BIT_OFFSET       6

esp_err_t hw_oled_init(esp_lcd_panel_handle_t *panel_handle,
                       esp_lcd_panel_io_handle_t *io_handle)
{
    ESP_RETURN_ON_FALSE(panel_handle, ESP_ERR_INVALID_ARG, TAG, "panel_handle is NULL");
    ESP_RETURN_ON_FALSE(io_handle, ESP_ERR_INVALID_ARG, TAG, "io_handle is NULL");

    *panel_handle = NULL;
    *io_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HW_OLED_I2C_SDA_GPIO,
        .scl_io_num = HW_OLED_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = HW_OLED_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(HW_OLED_I2C_HOST, &i2c_conf), TAG, "i2c_param_config failed");
    esp_err_t ret = i2c_driver_install(HW_OLED_I2C_HOST, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed");
        return ret;
    }

    ESP_LOGI(TAG, "Install SSD1306 panel IO");
    const esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = HW_OLED_I2C_ADDR,
        .control_phase_bytes = HW_OLED_CTRL_PHASE_BYTES,
        .lcd_cmd_bits = HW_OLED_CMD_BITS,
        .lcd_param_bits = HW_OLED_PARAM_BITS,
        .dc_bit_offset = HW_OLED_DC_BIT_OFFSET,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(HW_OLED_I2C_HOST, &io_config, io_handle),
                        TAG, "esp_lcd_new_panel_io_i2c failed");

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = HW_OLED_RESET_GPIO,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = HW_OLED_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;

    ret = esp_lcd_new_panel_ssd1306(*io_handle, &panel_config, panel_handle);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(*io_handle);
        *io_handle = NULL;
        i2c_driver_delete(HW_OLED_I2C_HOST);
        return ret;
    }

    ret = esp_lcd_panel_reset(*panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel reset failed");
        goto cleanup;
    }
    ret = esp_lcd_panel_init(*panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed");
        goto cleanup;
    }
    ret = esp_lcd_panel_invert_color(*panel_handle, HW_OLED_INVERT_COLOR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel invert failed");
        goto cleanup;
    }
    ret = esp_lcd_panel_disp_on_off(*panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel disp on failed");
        goto cleanup;
    }

    return ESP_OK;

cleanup:
    if (*panel_handle) {
        esp_lcd_panel_del(*panel_handle);
        *panel_handle = NULL;
    }
    if (*io_handle) {
        esp_lcd_panel_io_del(*io_handle);
        *io_handle = NULL;
    }
    i2c_driver_delete(HW_OLED_I2C_HOST);
    return ret;
}
