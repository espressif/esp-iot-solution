/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch: CST9220
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a new CST9220 touch driver
 *
 * @note The I2C panel IO must be initialized before calling this function.
 * @note Each report is read from `0xD000` in one burst on the `D101` page.
 *       Current firmware is acknowledged with `0xAB`; the confirmed legacy
 *       project skips ACK.
 * @note Sleep control is available through the common esp_lcd_touch sleep APIs.
 *
 * @param io LCD panel IO handle created by `esp_lcd_new_panel_io_i2c()`
 * @param config Touch panel configuration
 * @param out_touch Touch panel handle
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if an argument or GPIO is invalid
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_ERR_NOT_SUPPORTED if the detected controller is not CST9220
 *      - Other errors propagated from GPIO or panel IO operations
 */
esp_err_t esp_lcd_touch_new_i2c_cst9220(const esp_lcd_panel_io_handle_t io,
                                        const esp_lcd_touch_config_t *config,
                                        esp_lcd_touch_handle_t *out_touch);

/**
 * @brief I2C address of the CST9220 touch controller
 */
#define ESP_LCD_TOUCH_IO_I2C_CST9220_ADDRESS    (0x5A)

/**
 * @brief Maximum number of simultaneous touch points supported by CST9220
 */
#define ESP_LCD_TOUCH_CST9220_MAX_POINTS        (2)

/**
 * @brief Default I2C panel IO configuration for CST9220
 */
#define ESP_LCD_TOUCH_IO_I2C_CST9220_CONFIG()             \
    {                                                     \
        .scl_speed_hz = 400000,                           \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST9220_ADDRESS, \
        .control_phase_bytes = 1,                         \
        .dc_bit_offset = 0,                               \
        .lcd_cmd_bits = 8,                                \
        .lcd_param_bits = 8,                              \
        .flags =                                          \
        {                                                 \
            .disable_control_phase = 1,                   \
        },                                                \
    }

#ifdef __cplusplus
}
#endif
