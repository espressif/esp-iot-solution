/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_err.h"
#include "button_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* button_cb_t)(void *button_handle, void *usr_data);

typedef void (* button_power_save_cb_t)(void *usr_data);

/**
 * @brief Structs to store power save callback info
 *
 */
typedef struct {
    button_power_save_cb_t enter_power_save_cb;  /**< Callback function when entering power save mode */
    void *usr_data;                              /**< User data for the callback */
} button_power_save_config_t;

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
    BUTTON_MULTIPLE_CLICK,
    BUTTON_LONG_PRESS_START,
    BUTTON_LONG_PRESS_HOLD,
    BUTTON_LONG_PRESS_UP,
    BUTTON_PRESS_END,
    BUTTON_EVENT_MAX,
    BUTTON_NONE_PRESS,
} button_event_t;

/**
 * @brief Button events arg
 *
 */
typedef union {
    /**
     * @brief Long press time event data
     *
     */
    struct long_press_t {
        uint16_t press_time;    /**< press time(ms) for the corresponding callback to trigger */
    } long_press;               /**< long press struct, for event BUTTON_LONG_PRESS_START and BUTTON_LONG_PRESS_UP */

    /**
     * @brief Multiple clicks event data
     *
     */
    struct multiple_clicks_t {
        uint16_t clicks;        /**< number of clicks, to trigger the callback */
    } multiple_clicks;          /**< multiple clicks struct, for event BUTTON_MULTIPLE_CLICK */
} button_event_args_t;

/**
 * @brief Button parameter
 *
 */
typedef enum {
    BUTTON_LONG_PRESS_TIME_MS = 0,
    BUTTON_SHORT_PRESS_TIME_MS,
    BUTTON_PARAM_MAX,
} button_param_t;

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
 * @param event_args Button event arguments
 * @param cb Callback function.
 * @param usr_data user data
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_INVALID_STATE The Callback is already registered. No free Space for another Callback.
 *      - ESP_ERR_NO_MEM        No more memory allocation for the event
 */
esp_err_t iot_button_register_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args, button_cb_t cb, void *usr_data);

/**
 * @brief Unregister all the callbacks associated with the event.
 *
 * @param btn_handle A button handle to unregister
 * @param event Button event
 * @param event_args Used for unregistering a specific callback.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_INVALID_STATE No callbacks registered for the event
 */
esp_err_t iot_button_unregister_cb(button_handle_t btn_handle, button_event_t event, button_event_args_t *event_args);

/**
 * @brief counts total callbacks registered
 *
 * @param btn_handle A button handle to the button
 *
 * @return
 *      - 0 if no callbacks registered, or 1 .. (BUTTON_EVENT_MAX-1) for the number of Registered Buttons.
 *      - ESP_ERR_INVALID_ARG if btn_handle is invalid
 */
size_t iot_button_count_cb(button_handle_t btn_handle);

/**
 * @brief how many callbacks are registered for the event
 *
 * @param btn_handle A button handle to the button
 *
 * @param event Button event
 *
 * @return
 *      - 0 if no callbacks registered, or 1 .. (BUTTON_EVENT_MAX-1) for the number of Registered Buttons.
 *      - ESP_ERR_INVALID_ARG if btn_handle is invalid
 */
size_t iot_button_count_event_cb(button_handle_t btn_handle, button_event_t event);

/**
 * @brief Get button event
 *
 * @param btn_handle Button handle
 *
 * @return Current button event. See button_event_t
 */
button_event_t iot_button_get_event(button_handle_t btn_handle);

/**
 * @brief Get the string representation of a button event.
 *
 * This function returns the corresponding string for a given button event.
 * If the event value is outside the valid range, the function returns error string "event value is invalid".
 *
 * @param[in] event The button event to be converted to a string.
 *
 * @return
 *      - Pointer to the event string if the event is valid.
 *      - "invalid event" if the event value is invalid.
 */
const char *iot_button_get_event_str(button_event_t event);

/**
 * @brief Log the current button event as a string.
 *
 * This function prints the string representation of the current event associated with the button.
 *
 * @param[in] btn_handle Handle to the button object.
 *
 * @return
 *      - ESP_OK: Successfully logged the event string.
 *      - ESP_FAIL: Invalid button handle.
 */
esp_err_t iot_button_print_event(button_handle_t btn_handle);

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
uint32_t iot_button_get_ticks_time(button_handle_t btn_handle);

/**
 * @brief Get button long press hold count
 *
 * @param btn_handle Button handle
 *
 * @return Count of trigger cb(BUTTON_LONG_PRESS_HOLD)
 */
uint16_t iot_button_get_long_press_hold_cnt(button_handle_t btn_handle);

/**
 * @brief Dynamically change the parameters of the iot button
 *
 * @param btn_handle Button handle
 * @param param Button parameter
 * @param value new value
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t iot_button_set_param(button_handle_t btn_handle, button_param_t param, void *value);

/**
 * @brief Get button key level
 *
 * @param btn_handle Button handle
 * @return
 *      - 1 if key is pressed
 *      - 0 if key is released or invalid button handle
 */
uint8_t iot_button_get_key_level(button_handle_t btn_handle);

/**
 * @brief resume button timer, if button timer is stopped. Make sure iot_button_create() is called before calling this API.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE   timer state is invalid.
 */
esp_err_t iot_button_resume(void);

/**
 * @brief stop button timer, if button timer is running. Make sure iot_button_create() is called before calling this API.
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_STATE   timer state is invalid
 */
esp_err_t iot_button_stop(void);

/**
 * @brief Register a callback function for power saving.
 *        The config->enter_power_save_cb function will be called when all keys stop working.
 *
 * @param config Button power save config
 * @return
 *     - ESP_OK                  on success
 *     - ESP_ERR_INVALID_STATE   No button registered
 *     - ESP_ERR_INVALID_ARG     Arguments is invalid
 *     - ESP_ERR_NO_MEM          Not enough memory
 */
esp_err_t iot_button_register_power_save_cb(const button_power_save_config_t *config);

#ifdef __cplusplus
}
#endif
