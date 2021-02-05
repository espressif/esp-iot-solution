// Copyright 2020-2021 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __LED_INDICATOR_H__
#define __LED_INDICATOR_H__

#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_err.h"

/**
 * @brief led on-off state
 * 
 */
typedef enum {
    LED_STATE_OFF = 0, /**< turn off the led */ 
    LED_STATE_ON = 1, /**< turn on the led */ 
} led_indicator_state_t;

/**
 * @brief actions in this type
 * 
 */
typedef enum {
    LED_BLINK_STOP = -1, /**< stop the blink */ 
    LED_BLINK_HOLD, /**< hold the on-off state */ 
    LED_BLINK_LOOP, /**< loop from first step */ 
} blink_step_type_t;

/**
 * @brief one blink step, a meaningful signal consists of a group of steps
 * 
 */
typedef struct {
    blink_step_type_t type;          /**< actions is this step */ 
    led_indicator_state_t on_off;    /**< hold on or off, set NULL if not LED_BLINK_HOLD*/ 
    uint32_t hold_time_ms;           /**< hold time(ms), set NULL if not LED_BLINK_HOLD,*/ 
} led_indicator_blink_step_t;

/**
 * @brief defined LED indicator blink steps
 * 
 */
extern const led_indicator_blink_step_t connecting[]; /**< connecting to AP (or Cloud) */
extern const led_indicator_blink_step_t connected[]; /**<  connected to AP (or Cloud) succeed */
extern const led_indicator_blink_step_t reconnecting[]; /**< reconnecting to AP (or Cloud), if lose connection */ 
extern const led_indicator_blink_step_t updating[]; /**< updating software */ 
extern const led_indicator_blink_step_t factory_reset[]; /**< restoring factory settings */ 
extern const led_indicator_blink_step_t provisioning[]; /**< provisioning */ 
extern const led_indicator_blink_step_t provisioned[]; /**< provision done */ 

/**
 * @brief led indicator blink mode, as a member of led_indicator_config_t
 * 
 */
typedef enum {
    LED_GPIO_MODE,        /**< blink with max brightness*/
    LED_PWM_MODE,         /**< brightness gradient */ 
}led_indicator_mode_t;

/**
 * @brief led indicator specified configurations, as a arg when create a new indicator
 * 
 */
typedef struct {
    bool off_level; /*!< gpio level of turn off. 0 if attach led positive side to esp32 gpio pin, 1 if attach led negative side*/
    led_indicator_mode_t mode; /*!< led work mode, eg. gpio or pwm mode */
}led_indicator_config_t;

typedef void* led_indicator_handle_t; /*!< led indicator operation handle */

/**
 * @brief create a led indicator instance with gpio number and configuration
 * 
 * @param io_num gpio number of the led
 * @param config configuration of the led, eg. gpio level when led off
 * @return led_indicator_handle_t handle of the led indicator, NULL if create failed.
 */
led_indicator_handle_t led_indicator_create(int io_num, const led_indicator_config_t* config);

/**
 * @brief get the handle of created led_indicator with gpio number
 * 
 * @param io_num  gpio number of the led
 * @return led_indicator_handle_t handle of the created led indicator, NULL if not created
 */
led_indicator_handle_t led_indicator_get_handle(int io_num);

/**
 * @brief delete the led indicator and release resource
 * 
 * @param p_handle pointer to led indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG   if parameter is invalid
 *     - ESP_FAIL Fail
 *     - ESP_OK Success
 */
esp_err_t led_indicator_delete(led_indicator_handle_t* p_handle);

/**
 * @brief start a blink group, if 
 * 
 * @param handle led indicator handle
 * @param blink_steps predefined blink steps
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG   if parameter is invalid
 *     - ESP_ERR_NOT_FOUND no predefined blink_steps found
 *     - ESP_OK Success
 */
esp_err_t led_indicator_start(led_indicator_handle_t handle, const led_indicator_blink_step_t blink_steps[]);

/**
 * @brief stop a blink group
 * 
 * @param handle led indicator handle
 * @param blink_steps predefined blink steps
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG   if parameter is invalid
 *     - ESP_ERR_NOT_FOUND no predefined blink_steps found
 *     - ESP_OK Success
 */
esp_err_t led_indicator_stop(led_indicator_handle_t handle, const led_indicator_blink_step_t blink_steps[]);


#endif
