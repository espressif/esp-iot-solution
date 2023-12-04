/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "led_strip.h"
#include "led_strip_types.h"
#include "esp_idf_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief led strip driver type
 *
 */
typedef enum {
    LED_STRIP_RMT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    LED_STRIP_SPI,
#endif
    LED_STRIP_MAX,
} led_strip_driver_t;

typedef struct {
    led_strip_config_t led_strip_cfg;                   /*!< LED Strip Configuration. */
    led_strip_driver_t led_strip_driver;                /*!< led strip control type */
    union {
        led_strip_rmt_config_t led_strip_rmt_cfg;       /*!< RMT (Remote Control) Configuration for the LED strip. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        led_strip_spi_config_t led_strip_spi_cfg;       /*!< SPI Configuration for the LED strip. */
#endif
    };
} led_indicator_strips_config_t;

/**
 * @brief Initialize the RGB LED indicator (WS2812 SK6812).
 *
 * @param param Pointer to initialization parameters.
 * @param ret_strips Pointer to a variable that will hold the LED strip instance.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Initialization failed
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t led_indicator_strips_init(void *param, void **ret_strips);

/**
 * @brief Deinitialize led strip which is used by the LED indicator.
 *
 * @param strips LED indicator LED Strips operation handle
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Deinit fail
 */
esp_err_t led_indicator_strips_deinit(void *strips);

/**
 * @brief Turn the LED indicator on or off.
 *
 * @param strips LED indicator LED Strips operation handle.
 * @param on_off Set to 0 or 1 to control the LED (0 for off, 1 for on).
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_FAIL: LEDC channel initialization failed
 */
esp_err_t led_indicator_strips_set_on_off(void *strips, bool on_off);

/**
 * @brief Set the RGB color for the LED indicator.
 *
 * @param strips  LED indicator LED Strips operation handle.
 * @param rgb_value RGB color value to set. (I: 0-127 R: 0-255, G: 0-255, B: 0-255)
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_rgb(void *strips, uint32_t irgb_value);

/**
 * @brief Set the HSV color for the LED indicator.
 *
 * @param strips LED indicator LED Strips operation handle.
 * @param hsv_value HSV color value to set. (I: 0-127 H: 0-360, S: 0-255, V: 0-255)
 * @return esp_err_t
 *        - ESP_OK: Success
 *        - ESP_ERR_INVALID_ARG: Invalid argument provided
 *        - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_hsv(void *strips, uint32_t ihsv_value);

/**
 * @brief Set the brightness for the LED indicator.
 *
 * @param strips Pointer to the LED indicator handle.
 * @param brightness Brightness value to set. (I: 0-127 B: 0-255)
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_brightness(void *strips, uint32_t ibrightness);

#ifdef __cplusplus
}
#endif
