/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* knob_cb_t)(void *, void *);
typedef void *knob_handle_t;

/**
 * @brief Knob events
 *
 */
typedef enum {
    KNOB_LEFT = 0,                     /*!< EVENT: Rotate to the left */
    KNOB_RIGHT,                        /*!< EVENT: Rotate to the right */
    KNOB_H_LIM,                        /*!< EVENT: Count reaches maximum limit */
    KNOB_L_LIM,                        /*!< EVENT: Count reaches the minimum limit */
    KNOB_ZERO,                         /*!< EVENT: Count back to 0 */
    KNOB_EVENT_MAX,                    /*!< EVENT: Number of events */
} knob_event_t;

/**
 * @brief Knob config
 * 
 */
typedef struct {
    uint8_t default_direction;          /*!< 0:positive increase   1:negative increase */
    uint8_t gpio_encoder_a;             /*!< Encoder Pin A */
    uint8_t gpio_encoder_b;             /*!< Encoder Pin B */
} knob_config_t;

/**
 * @brief create a knob
 *
 * @param config pointer of knob configuration
 *
 * @return A handle to the created knob
 */
knob_handle_t iot_knob_create(const knob_config_t *config);

/**
 * @brief Delete a knob
 *
 * @param knob_handle A knob handle to delete
 *
 * @return
 *         - ESP_OK  Success
 *         - ESP_FAIL Failure
 */
esp_err_t iot_knob_delete(knob_handle_t knob_handle);

/**
 * @brief Register the knob event callback function
 *
 * @param knob_handle A knob handle to register
 * @param event Knob event
 * @param cb Callback function
 * @param usr_data user data
 *
 * @return
 *         - ESP_OK  Success
 *         - ESP_FAIL Failure
 */
esp_err_t iot_knob_register_cb(knob_handle_t knob_handle, knob_event_t event, knob_cb_t cb, void *usr_data);

/**
 * @brief Unregister the knob event callback function
 *
 * @param knob_handle A knob handle to register
 * @param event Knob event
 *
 * @return
 *         - ESP_OK  Success
 *         - ESP_FAIL Failure
 */
esp_err_t iot_knob_unregister_cb(knob_handle_t knob_handle, knob_event_t event);

/**
 * @brief Get knob event
 * 
 * @param knob_handle A knob handle to register
 * @return knob_event_t Knob event
 */
knob_event_t iot_knob_get_event(knob_handle_t knob_handle);

/**
 * @brief Get knob count value
 *
 * @param knob_handle A knob handle to register
 *
 * @return int32_t count_value
 */
int32_t iot_knob_get_count_value(knob_handle_t knob_handle);

/**
 * @brief Clear knob cout value to zero
 *
 * @param knob_handle A knob handle to register
 *
 * @return
 *         - ESP_OK  Success
 *         - ESP_FAIL Failure
 */
esp_err_t iot_knob_clear_count_value(knob_handle_t knob_handle);

#ifdef __cplusplus
}
#endif