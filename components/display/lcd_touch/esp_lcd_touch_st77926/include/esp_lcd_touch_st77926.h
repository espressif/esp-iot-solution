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
 * @brief Create a new ST77926 touch driver
 *
 * @note The I2C communication should be initialized before use this function.
 * @note This driver implements the coordinate read path only. Host-download,
 *       firmware upgrade, gesture wakeup and self-test are not implemented.
 *
 * @param io LCD panel IO handle, it should be created by `esp_lcd_new_panel_io_i2c()`
 * @param config Touch panel configuration
 * @param tp Touch panel handle
 * @return
 *      - ESP_OK: on success
 */
esp_err_t esp_lcd_touch_new_i2c_st77926(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config,
                                        esp_lcd_touch_handle_t *tp);

/**
 * @brief I2C address of the ST77926 touch controller
 */
#define ESP_LCD_TOUCH_IO_I2C_ST77926_ADDRESS     (0x55)

/**
 * @brief Touch IO configuration structure
 */
#define ESP_LCD_TOUCH_IO_I2C_ST77926_CONFIG()               \
    {                                                       \
        .scl_speed_hz = 400000,                             \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_ST77926_ADDRESS,   \
        .control_phase_bytes = 1,                           \
        .lcd_cmd_bits = 16,                                 \
        .flags =                                            \
        {                                                   \
            .disable_control_phase = 1,                     \
        }                                                   \
    }

#ifdef __cplusplus
}
#endif
