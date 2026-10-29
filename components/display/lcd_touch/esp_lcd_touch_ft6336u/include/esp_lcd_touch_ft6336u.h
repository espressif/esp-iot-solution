/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new FT6336U touch driver.
 *
 * @param[in] io I2C panel IO handle created by `esp_lcd_new_panel_io_i2c()`
 * @param[in] config Touch panel configuration
 * @param[out] out_touch Returned touch handle
 * @return ESP_OK on success
 */
esp_err_t esp_lcd_touch_new_i2c_ft6336u(const esp_lcd_panel_io_handle_t io,
                                        const esp_lcd_touch_config_t *config,
                                        esp_lcd_touch_handle_t *out_touch);

#define ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS     (0x38)  /*!< Standard FT6336U hardware default */
#define ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS_ALT (0x48)  /*!< Alternate FT6336U module address */

#define ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG_WITH_ADDR(i2c_addr) \
    {                                                            \
        .scl_speed_hz = 400000,                                  \
        .dev_addr = (i2c_addr),                                  \
        .control_phase_bytes = 1,                                \
        .lcd_cmd_bits = 8,                                       \
        .flags =                                                 \
        {                                                        \
            .disable_control_phase = 1,                          \
        },                                                       \
    }

#define ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG() \
    ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG_WITH_ADDR(ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS)

#ifdef __cplusplus
}
#endif
