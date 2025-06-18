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
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "led_convert.h"

#define LED_INDICATOR_CHECK(a, str, action) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

#define LED_INDICATOR_CHECK_WARNING(a, str, action) if(!(a)) { \
        ESP_LOGW(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }
/**
 * @brief LED state: 0-100, only hardware that supports to set brightness can adjust brightness.
 *
 */
enum {
    LED_STATE_OFF = 0,           /*!< turn off the LED */
    LED_STATE_25_PERCENT = 64,   /*!< 25% brightness, must support to set brightness */
    LED_STATE_50_PERCENT = 128,  /*!< 50% brightness, must support to set brightness */
    LED_STATE_75_PERCENT = 191,  /*!< 75% brightness, must support to set brightness */
    LED_STATE_ON = UINT8_MAX,    /*!< turn on the LED */
};

/**
 * @brief actions in this type
 *
 */
typedef enum {
    LED_BLINK_STOP = -1,   /*!< stop the blink */
    LED_BLINK_HOLD,        /*!< hold the on-off state */
    LED_BLINK_BREATHE,     /*!< breathe state */
    LED_BLINK_BRIGHTNESS,  /*!< set the brightness, it will transition from the old brightness to the new brightness */
    LED_BLINK_RGB,         /*!< color change with R(0-255) G(0-255) B(0-255) */
    LED_BLINK_RGB_RING,    /*!< Gradual color transition from old color to new color in a color ring */
    LED_BLINK_HSV,         /*!< color change with H(0-360) S(0-255) V(0-255) */
    LED_BLINK_HSV_RING,    /*!< Gradual color transition from old color to new color in a color ring */
    LED_BLINK_LOOP,        /*!< loop from first step */
} blink_step_type_t;

/**
 * @brief one blink step, a meaningful signal consists of a group of steps
 *
 */
typedef struct {
    blink_step_type_t type;          /*!< action type in this step */
    uint32_t value;                  /*!< hold on or off, set 0 if LED_BLINK_STOP() or LED_BLINK_LOOP */
    uint32_t hold_time_ms;           /*!< hold time(ms), set 0 if not LED_BLINK_HOLD */
} blink_step_t;

/**
 * @brief LED indicator blink mode, as a member of led_indicator_config_t
 *
 */
typedef enum {
    LED_GPIO_MODE,         /*!< blink with max brightness */
    LED_LEDC_MODE,         /*!< blink with LEDC driver */
    LED_RGB_MODE,          /*!< blink with RGB driver */
    LED_STRIPS_MODE,       /*!< blink with LEDC strips driver */
    LED_CUSTOM_MODE,       /*!< blink with custom driver */
} led_indicator_mode_t;

typedef struct {
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;                           /*!< Hardware data of the LED indicator */
    led_indicator_mode_t mode;                     /*!< LED work mode, eg. GPIO or pwm mode */
    int active_blink;                              /*!< Active blink list*/
    int preempt_blink;                             /*!< Highest priority blink list*/
    int *p_blink_steps;                            /*!< Stage of each blink list */
    led_indicator_ihsv_t current_fade_value;       /*!< Current fade value */
    led_indicator_ihsv_t last_fade_value;          /*!< Save the last value. */
    uint16_t fade_value_count;                     /*!< Count the number of fade */
    uint16_t fade_step;                            /*!< Step of fade */
    uint16_t fade_total_step;                      /*!< Total step of fade */
    uint32_t max_duty;                             /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    SemaphoreHandle_t mutex;                       /*!< Mutex to achieve thread-safe */
    TimerHandle_t h_timer;                         /*!< LED timer handle, invalid if works in pwm mode */
    blink_step_t const **blink_lists;              /*!< User defined LED blink lists */
    uint16_t blink_list_num;                       /*!< Number of blink lists */
} _led_indicator_t;

typedef struct _led_indicator_com_config {
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;                  /*!< GPIO number of the LED indicator */
    blink_step_t const **blink_lists;     /*!< User defined LED blink lists */
    uint16_t blink_list_num;              /*!< Number of blink lists */
    led_indicator_duty_t duty_resolution; /*!< Resolution of duty setting in number of bits. The range of duty values is [0, (2**duty_resolution) -1]. If the brightness cannot be set, set this as 1. */
} _led_indicator_com_config_t;

typedef struct {
    blink_step_t const **blink_lists;           /*!< user defined LED blink lists */
    uint16_t blink_list_num;                    /*!< number of blink lists */
} led_config_t;

typedef void *led_indicator_handle_t; /*!< LED indicator operation handle */

_led_indicator_t *_led_indicator_create_com(_led_indicator_com_config_t *cfg);

esp_err_t _led_indicator_add_node(_led_indicator_t *p_led_indicator);

/**
 * @brief create a LED indicator instance with GPIO number and configuration
 *
 * @param config configuration of the LED, eg. GPIO level when LED off
 * @return led_indicator_handle_t handle of the LED indicator, NULL if create failed.
 */

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
