/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEDC_MODE       CONFIG_LEDC_SPEED_MODE_VALUE
#define LEDC_DUTY_RES   CONFIG_LEDC_TIMER_BIT_NUM
#define LEDC_FREQ_HZ    CONFIG_LEDC_TIMER_FREQ_HZ

typedef struct {
    bool is_active_level_high;        /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    bool timer_inited;                /*!< Set true if LEDC timer is inited, otherwise false. */
    ledc_timer_t timer_num;           /*!< The timer source of channel */
    int32_t red_gpio_num;             /*!< Red LED pwm gpio number */
    int32_t green_gpio_num;           /*!< Green LED pwm gpio number */
    int32_t blue_gpio_num;            /*!< Blue LED pwm gpio number */
    ledc_channel_t red_channel;       /*!< Red LED LEDC channel */
    ledc_channel_t green_channel;     /*!< Green LED LEDC channel */
    ledc_channel_t blue_channel;      /*!< Blue LED LEDC channel */
} led_indicator_rgb_config_t;

/**
 * @brief Initialize the RGB LED indicator (WS2812 SK6812).
 *
 * @param param Pointer to initialization parameters.
 * @param ret_rgb Pointer to a variable that will hold the LED RGB instance.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Initialization failed
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t led_indicator_rgb_init(void *param, void **ret_rgb);

/**
 * @brief Deinitialize led RGB which is used by the LED indicator.
 *
 * @param rgb_handle LED indicator LED RGB operation handle
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Deinit fail
 */
esp_err_t led_indicator_rgb_deinit(void *rgb_handle);

/**
 * @brief Turn the LED indicator on or off.
 *
 * @param rgb_handle LED indicator LED RGB operation handle.
 * @param on_off Set to 0 or 1 to control the LED (0 for off, 1 for on).
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_FAIL: LEDC channel initialization failed
 */
esp_err_t led_indicator_rgb_set_on_off(void *rgb_handle, bool on_off);

/**
 * @brief Set the RGB color for the LED indicator.
 *
 * @param rgb_handle  LED indicator LED RGB operation handle.
 * @param rgb_value RGB color value to set. (R: 0-255, G: 0-255, B: 0-255)
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument provided
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_rgb_set_rgb(void *rgb_handle, uint32_t rgb_value);

/**
 * @brief Set the HSV color for the LED indicator.
 *
 * @param rgb_handle LED indicator LED RGB operation handle.
 * @param hsv_value HSV color value to set. (H: 0-360, S: 0-255, V: 0-255)
 * @return esp_err_t
 *        - ESP_OK: Success
 *        - ESP_ERR_INVALID_ARG: Invalid argument provided
 *        - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_rgb_set_hsv(void *rgb_handle, uint32_t hsv_value);

/**
 * @brief Set the brightness for the LED indicator.
 *
 * @param rgb_handle Pointer to the LED indicator handle.
 * @param brightness Brightness value to set.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failed to set brightness
 */
esp_err_t led_indicator_rgb_set_brightness(void *rgb_handle, uint32_t brightness);

#ifdef __cplusplus
}
#endif
