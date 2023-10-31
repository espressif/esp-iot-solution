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

#define MAX_HUE 360
#define MAX_SATURATION 255
#define MAX_BRIGHTNESS 255
#define MAX_INDEX 127

#define SET_RGB(r, g, b) \
        ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

#define SET_IRGB(index, r, g, b) \
        ((((index) & 0x7F) << 25) | SET_RGB(r, g, b))

#define SET_HSV(h, s, v) \
        ((((h) > 360 ? 360 : (h)) & 0x1FF) << 16) | (((s) & 0xFF) << 8) | ((v) & 0xFF)

#define SET_IHSV(index, h, s, v) \
        ((((index) & 0x7F) << 25) | SET_HSV(h, s, v))

#define INSERT_INDEX(index, brightness) \
        ((((index) & 0x7F) << 25) | ((brightness) & 0xFF))

#define SET_INDEX(variable, value) \
        variable = (variable & 0x1FFFFFF) | (((value) & 0x7F) << 25)

#define SET_HUE(variable, value) \
        variable = (variable & 0xFE00FFFF) | (((value) & 0x1FF) << 16)

#define SET_SATURATION(variable, value) \
        variable = (variable & 0xFFFF00FF) | (((value) & 0xFF) << 8)

#define SET_BRIGHTNESS(variable, value) \
        variable = (variable & 0xFFFFFF00) | ((value) & 0xFF)

#define GET_INDEX(variable) \
        ((variable >> 25) & 0x7F)

#define GET_HUE(variable) \
        ((variable >> 16) & 0x1FF)

#define GET_SATURATION(variable) \
        ((variable >> 8) & 0xFF)

#define GET_BRIGHTNESS(variable) \
        (variable & 0xFF)

#define GET_RED(variable) \
        ((variable >> 16) & 0xFF)

#define GET_GREEN(variable) \
        ((variable >> 8) & 0xFF)

#define GET_BLUE(variable) \
        (variable & 0xFF)

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
    bool is_active_level_high;                          /*!< Set true if GPIO level is high when light is ON, otherwise false. */
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
 * @param rgb_value RGB color value to set. (R: 0-255, G: 0-255, B: 0-255)
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_rgb(void *strips, uint32_t rgb_value);

/**
 * @brief Set the HSV color for the LED indicator.
 *
 * @param strips LED indicator LED Strips operation handle.
 * @param hsv_value HSV color value to set. (H: 0-360, S: 0-255, V: 0-255)
 * @return esp_err_t
 *        - ESP_OK: Success
 *        - ESP_ERR_INVALID_ARG: Invalid argument provided
 *        - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_hsv(void *strips, uint32_t hsv_value);

/**
 * @brief Set the brightness for the LED indicator.
 *
 * @param strips Pointer to the LED indicator handle.
 * @param brightness Brightness value to set.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_strips_set_brightness(void *strips, uint32_t brightness);

/**
 * @brief Convert an RGB color value to an HSV color value.
 *
 * @param rgb_value RGB color value to convert.
 *        R: 0-255, G: 0-255, B: 0-255
 * @return uint32_t HSV color value.
 */
uint32_t led_indicator_strips_rgb2hsv(uint32_t rgb_value);

/**
 * @brief Convert an HSV color value to its RGB components (R, G, B).
 *
 * @param hsv HSV color value.
 *        H: 0-360, S: 0-255, V: 0-255
 * @param r Pointer to store the resulting R component.
 * @param g Pointer to store the resulting G component.
 * @param b Pointer to store the resulting B component.
 */
void led_indicator_strips_hsv2rgb(uint32_t hsv, uint32_t *r, uint32_t *g, uint32_t *b);


#ifdef __cplusplus
}
#endif
