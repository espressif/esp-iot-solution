/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdio.h>
#include "driver/touch_pad.h"
#include "esp_err.h"

/**
 * @brief Touch Proximity Sensor State
 */
typedef enum {
    PROXI_STATE_INACTIVE = 0,
    PROXI_STATE_ACTIVE,
} proxi_state_t;

/**
 * @brief Configuration structure for touch proximity sensor
 */
typedef struct {
    uint32_t channel_num;                           /*!< Number of touch proximity sensor channels */
    uint32_t *channel_list;                         /*!< Touch channel list */
    float *channel_threshold;                       /*!< Threshold for touch detection for each channel */
    uint32_t debounce_times;                        /*!< Number of consecutive readings needed to confirm state change */
    uint32_t *channel_gold_value;                   /*!< Reference values for touch channels */
    bool skip_lowlevel_init;                        /*!< Skip low level initialization when working with existing touch driver */
} touch_proxi_config_t;

typedef struct touch_proximity_sensor_t *touch_proximity_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * proximity sensor user callback type
 */
typedef void(*proxi_cb_t)(uint32_t channel, proxi_state_t event, void *cb_arg);

/**
 * @brief Create a touch proximity sensor instance.
 *
 * @param config The touch pad channel configuration.
 * @param sensor_handle The handle of the successfully created touch proximity sensor.
 * @param cb Callback function to handle proximity events.
 * @param cb_arg The callback function argument.
 * @return
 *      - ESP_OK: Create the touch proximity sensor successfully.
 *      - ESP_ERR_NO_MEM: Failed to create the touch proximity sensor (memory allocation failed).
 */
esp_err_t touch_proximity_sensor_create(touch_proxi_config_t *config, touch_proximity_handle_t *sensor_handle, proxi_cb_t cb, void *cb_arg);

/**
 * @brief Delete the touch proximity sensor instance.
 *
 * This function deletes the touch proximity sensor instance associated with the provided sensor handle.
 *
 * @param proxi_sensor Pointer to the handle of the touch proximity sensor instance to be deleted.
 * @return
 *      - ESP_OK: Delete the touch proximity sensor instance successfully
 */
esp_err_t touch_proximity_sensor_delete(touch_proximity_handle_t proxi_sensor);

/**
 * @brief Get touch proximity sensor data
 *
 * Retrieves the smoothed touch sensor reading from the specified channel.
 *
 * @param[in] handle        Touch proximity sensor handle
 * @param[in] channel      Touch channel number
 * @param[out] data        Pointer to store the smoothed touch sensor data
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle or data is NULL
 *     - ESP_ERR_INVALID_STATE if sensor not initialized
 *     - ESP_ERR_NOT_FOUND if channel not found
 */
esp_err_t touch_proximity_sensor_get_data(touch_proximity_handle_t handle, uint32_t channel, uint32_t *data);

/**
 * @brief Get current state of a proximity channel
 *
 * Returns whether the proximity channel is currently considered active (object detected)
 * based on the current sensor readings and configured thresholds.
 *
 * @param[in] handle    Touch proximity sensor handle
 * @param[in] channel   Touch channel number
 * @param[out] state    Pointer to store the channel state
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle or state is NULL
 *     - ESP_ERR_INVALID_STATE if sensor not initialized
 *     - ESP_ERR_NOT_FOUND if channel not found
 */
esp_err_t touch_proximity_sensor_get_state(touch_proximity_handle_t handle, uint32_t channel, proxi_state_t *state);

/**
 * @brief Handle pending proximity sensor events
 *
 * Processes events from the FSM. This function should be called
 * periodically to update proximity states and trigger callbacks.
 *
 * @param[in] handle    Touch proximity sensor handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_ERR_INVALID_STATE if sensor not initialized
 */
esp_err_t touch_proximity_sensor_handle_events(touch_proximity_handle_t handle);

#ifdef __cplusplus
}
#endif
