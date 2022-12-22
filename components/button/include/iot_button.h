// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _IOT_BUTTON_H_
#define _IOT_BUTTON_H_

#include "button_adc.h"
#include "button_gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* button_cb_t)(void *button_handle, void *usr_data);
typedef void *button_handle_t;

/**
 * @brief Button events
 *
 */
typedef enum {
    BUTTON_PRESS_DOWN = 0,
    BUTTON_PRESS_UP,
    BUTTON_PRESS_REPEAT,
    BUTTON_PRESS_REPEAT_DONE,
    BUTTON_SINGLE_CLICK,
    BUTTON_DOUBLE_CLICK,
    BUTTON_LONG_PRESS_START,
    BUTTON_LONG_PRESS_HOLD,
    BUTTON_EVENT_MAX,
    BUTTON_NONE_PRESS,
} button_event_t;

/**
 * @brief Supported button type
 *
 */
typedef enum {
    BUTTON_TYPE_GPIO,
    BUTTON_TYPE_ADC,
    BUTTON_TYPE_CUSTOM
} button_type_t;

/**
 * @brief custom button configuration
 * 
 */
typedef struct {
    uint8_t active_level;                                   /**< active level when press down */
    esp_err_t (*button_custom_init)(void *param);           /**< user defined button init */
    uint8_t (*button_custom_get_key_value)(void *param);    /**< user defined button get key value */
    esp_err_t (*button_custom_deinit)(void *param);         /**< user defined button deinit */
    void *priv;                                             /**< private data used for custom button, MUST be allocated dynamically and will be auto freed in iot_button_delete*/
} button_custom_config_t;

/**
 * @brief Button configuration
 *
 */
typedef struct {
    button_type_t type;                           /**< button type, The corresponding button configuration must be filled */
    uint16_t long_press_time;                     /**< Trigger time(ms) for long press, if 0 default to BUTTON_LONG_PRESS_TIME_MS */
    uint16_t short_press_time;                    /**< Trigger time(ms) for short press, if 0 default to BUTTON_SHORT_PRESS_TIME_MS */
    union {
        button_gpio_config_t gpio_button_config;  /**< gpio button configuration */
        button_adc_config_t adc_button_config;    /**< adc button configuration */
        button_custom_config_t custom_button_config;   /**< custom button configuration */
    }; /**< button configuration */
} button_config_t;

/**
 * @brief Create a button
 *
 * @param config pointer of button configuration, must corresponding the button type
 *
 * @return A handle to the created button, or NULL in case of error.
 */
button_handle_t iot_button_create(const button_config_t *config);

/**
 * @brief Delete a button
 *
 * @param btn_handle A button handle to delete
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t iot_button_delete(button_handle_t btn_handle);

/**
 * @brief Register the button event callback function.
 *
 * @param btn_handle A button handle to register
 * @param event Button event
 * @param cb Callback function.
 * @param usr_data user data
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_cb_t cb, void *usr_data);

/**
 * @brief Unregister the button event callback function.
 *
 * @param btn_handle A button handle to unregister
 * @param event Button event
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event);

/**
 * @brief how many Callbacks are still registered.
 *
 * @param btn_handle A button handle to unregister
 *
 * @return 0 if no callbacks registered, or 1 .. (BUTTON_EVENT_MAX-1) for the number of Registered Buttons.
 */
size_t iot_button_count_cb(button_handle_t btn_handle);

/**
 * @brief Get button event
 *
 * @param btn_handle Button handle
 *
 * @return Current button event. See button_event_t
 */
button_event_t iot_button_get_event(button_handle_t btn_handle);

/**
 * @brief Get button repeat times
 *
 * @param btn_handle Button handle
 *
 * @return button pressed times. For example, double-click return 2, triple-click return 3, etc.
 */
uint8_t iot_button_get_repeat(button_handle_t btn_handle);

/**
 * @brief Get button ticks time
 *
 * @param btn_handle Button handle
 *
 * @return Actual time from press down to up (ms).
 */
uint16_t iot_button_get_ticks_time(button_handle_t btn_handle);

/**
 * @brief Get button long press hold count
 *
 * @param btn_handle Button handle
 *
 * @return Count of trigger cb(BUTTON_LONG_PRESS_HOLD)
 */
uint16_t iot_button_get_long_press_hold_cnt(button_handle_t btn_handle);

#ifdef __cplusplus
}
#endif

#endif
