/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
#include "led_gpio.h"
#include "led_ledc.h"
#include "led_custom.h"

/**
 * @brief LED state: 0-100, only hardware that supports to set brightness can adjust brightness.
 *
 */
enum {
    LED_STATE_OFF = 0,          /*!< turn off the LED */
    LED_STATE_25_PERCENT = 64,  /*!< 25% brightness, must support to set brightness */
    LED_STATE_50_PERCENT = 128,  /*!< 50% brightness, must support to set brightness */
    LED_STATE_75_PERCENT = 191,  /*!< 75% brightness, must support to set brightness */
    LED_STATE_ON = UINT8_MAX,   /*!< turn on the LED */
};

/**
 * @brief actions in this type
 *
 */
typedef enum {
    LED_BLINK_STOP = -1,   /*!< stop the blink */
    LED_BLINK_HOLD,        /*!< hold the on-off state */
    LED_BLINK_BREATHE,     /*!< breathe state */
    LED_BLINK_BRIGHTNESS,  /*!< set the brightness */
    LED_BLINK_LOOP,        /*!< loop from first step */
} blink_step_type_t;

/**
 * @brief one blink step, a meaningful signal consists of a group of steps
 *
 */
typedef struct {
    blink_step_type_t type;          /*!< action type in this step */
    uint8_t value;                   /*!< hold on or off, set NULL if LED_BLINK_STOP or LED_BLINK_LOOP */
    uint32_t hold_time_ms;           /*!< hold time(ms), set NULL if not LED_BLINK_HOLD,*/
} blink_step_t;

/**
 * @brief LED indicator blink mode, as a member of led_indicator_config_t
 *
 */
typedef enum {
    LED_GPIO_MODE,         /*!< blink with max brightness*/
    LED_LEDC_MODE,         /*!< blink with LEDC driver*/
    LED_CUSTOM_MODE,       /*!< blink with custom driver*/
} led_indicator_mode_t;

/**
 * @brief LED indicator specified configurations, as a arg when create a new indicator
 *
 */
typedef struct {
    led_indicator_mode_t mode;                  /*!< LED work mode, eg. GPIO or pwm mode */
    union {
        led_indicator_gpio_config_t *led_indicator_gpio_config;     /*!< LED GPIO configuration */
        led_indicator_ledc_config_t *led_indicator_ledc_config;     /*!< LED LEDC configuration */
        led_indicator_custom_config_t *led_indicator_custom_config; /*!< LED custom configuration */
    }; /**< LED configuration */
    blink_step_t const **blink_lists;           /*!< user defined LED blink lists */
    uint16_t blink_list_num;                    /*!< number of blink lists */
} led_indicator_config_t;

typedef void *led_indicator_handle_t; /*!< LED indicator operation handle */

/**
 * @brief create a LED indicator instance with GPIO number and configuration
 *
 * @param config configuration of the LED, eg. GPIO level when LED off
 * @return led_indicator_handle_t handle of the LED indicator, NULL if create failed.
 */
led_indicator_handle_t led_indicator_create(const led_indicator_config_t *config);

/**
 * @brief get the handle of created led_indicator with hardware data
 *
 * @param hardware_data user hardware data for LED
 * @return led_indicator_handle_t handle of the created LED indicator, NULL if not created.
 */
led_indicator_handle_t led_indicator_get_handle(void *hardware_data);

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
 * @brief start a new blink_type on the LED indicator. if mutiple blink_type started simultaneously,
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

#ifdef __cplusplus
}
#endif

