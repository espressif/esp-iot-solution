/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new ILI2118 touch driver
 *
 * @param io LCD panel IO handle
 * @param config Touch panel configuration
 * @param tp Touch panel handle
 * @return
 *      - ESP_OK: on success
 */
esp_err_t esp_lcd_touch_new_i2c_ili2118(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *tp);

/**
 * @brief I2C address of the ILI2118A controller
 */
#define ESP_LCD_TOUCH_IO_I2C_ILI2118A_ADDRESS     (0x4C >> 1)

/**
 * @brief Touch IO configuration structure
 */
#define ESP_LCD_TOUCH_IO_I2C_ILI2118A_CONFIG()             \
    {                                                      \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_ILI2118A_ADDRESS, \
        .control_phase_bytes = 1,                          \
        .lcd_cmd_bits = 16,                                \
        .flags = {                                         \
            .disable_control_phase = 1,                    \
        }                                                  \
    }

#ifdef __cplusplus
}
#endif
