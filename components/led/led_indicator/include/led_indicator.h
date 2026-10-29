/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "led_convert.h"
#include "led_types.h"

#define LED_INDICATOR_CHECK(a, str, action) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

#define LED_INDICATOR_CHECK_WARNING(a, str, action) if(!(a)) { \
        ESP_LOGW(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

/**
 * @brief delete the LED indicator and release resource
 *
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Delete fail
 */
esp_err_t led_indicator_delete(led_indicator_handle_t handle);

/**
 * @brief start a new blink_type on the LED indicator. if multiple blink_type started simultaneously,
 * it will be executed according to priority.
 *
 * @param handle LED indicator handle
 * @param blink_type predefined blink type
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_ERR_NOT_FOUND no predefined blink_type found
 *     - ESP_OK Success
 */
esp_err_t led_indicator_start(led_indicator_handle_t handle, int blink_type);

/**
 * @brief stop a blink_type. you can stop a blink_type at any time, no matter it is executing or waiting to be executed.
 *
 * @param handle LED indicator handle
 * @param blink_type predefined blink type
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_ERR_NOT_FOUND no predefined blink_type found
 *     - ESP_OK Success
 */
esp_err_t led_indicator_stop(led_indicator_handle_t handle, int blink_type);

/**
 * @brief Immediately execute an action of any priority. Until the action is executed, or call led_indicator_preempt_stop().
 *
 * @param handle LED indicator handle
 * @param blink_type predefined blink type
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t led_indicator_preempt_start(led_indicator_handle_t handle, int blink_type);

/**
 * @brief Stop the current preemptive action.
 *
 * @param handle LED indicator handle
 * @param blink_type predefined blink type
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t led_indicator_preempt_stop(led_indicator_handle_t handle, int blink_type);

/**
 * @brief Get the current brightness value of the LED indicator.
 *
 * @param handle LED indicator handle
 * @return uint8_t Current brightness value: 0-255
 *         if handle is null return 0
 */
uint8_t led_indicator_get_brightness(led_indicator_handle_t handle);

/**
 * @brief Set the LED indicator on or off.
 *
 * @param handle LED indicator handle.
 * @param on_off true: on, false: off
 * @note If you have an RGB/Strips type of light, this API will control the last LED
 *       index you set, and the color will be displayed based on the last color you set.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 *     - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t led_indicator_set_on_off(led_indicator_handle_t handle, bool on_off);

/**
 * @brief Set the brightness for the LED indicator.
 *
 * @param handle LED indicator handle.
 * @param brightness Brightness value to set (0 to 255).
 *        You can control a specific LED by specifying the index using `SET_IB`,
 *        and set it to MAX_INDEX 127 to control all LEDs. This feature is only
 *        supported for LEDs of type LED_RGB_MODE.
 *        Index: (0-126), set (127) to control all.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 *     - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t led_indicator_set_brightness(led_indicator_handle_t handle, uint32_t brightness);

/**
 * @brief Get the HSV color of the LED indicator.
 *
 * @param handle LED indicator handle.
 * @return HSV color value
 *         H: 0-360, S: 0-255, V: 0-255
 * @note Index settings are only supported for LED_RGB_MODE.
 */
uint32_t led_indicator_get_hsv(led_indicator_handle_t handle);

/**
 * @brief Set the HSV color for the LED indicator.
 *
 * @param handle LED indicator handle.
 * @param ihsv_value HSV color value to set.
 *       I: 0-126, set 127 to control all H: 0-360, S: 0-255, V: 0-255
 * @note Index settings are only supported for LED_RGB_MODE.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 *     - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t led_indicator_set_hsv(led_indicator_handle_t handle, uint32_t ihsv_value);

/**
 * @brief Get the RGB color of the LED indicator.
 *
 * @param handle LED indicator handle.
 * @return RGB color value (0xRRGGBB)
 *         R: 0-255, G: 0-255, B: 0-255
 * @note Index settings are only supported for LED_RGB_MODE.
 */
uint32_t led_indicator_get_rgb(led_indicator_handle_t handle);

/**
 * @brief Set the RGB color for the LED indicator.
 *
 * @param handle LED indicator handle.
 * @param irgb_value RGB color value to set (0xRRGGBB).
 *        I: 0-126, set 127 to control all R: 0-255, G: 0-255, B: 0-255
 * @note Index settings are only supported for LED_RGB_MODE.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 *     - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t led_indicator_set_rgb(led_indicator_handle_t handle, uint32_t irgb_value);

/**
 * @brief Set the color temperature for the LED indicator.
 *
 * @param handle LED indicator handle.
 * @param temperature Color temperature of LED (0xIITTTTTT)
 *        I: 0-126, set 127 to control all, TTTTTT: 0-1000000

 * @note Index settings are only supported for LED_RGB_MODE.
 * @return esp_err_t
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 *     - ESP_ERR_INVALID_ARG: Invalid parameter
 */
esp_err_t led_indicator_set_color_temperature(led_indicator_handle_t handle, const uint32_t temperature);

#ifdef __cplusplus
}
#endif
