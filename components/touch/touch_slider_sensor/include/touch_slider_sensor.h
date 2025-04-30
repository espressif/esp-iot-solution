/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdio.h>
#include "esp_err.h"
#include "touch_sensor_lowlevel.h"

/**
 * @brief Touch slider event
 */
typedef enum {
    TOUCH_SLIDER_EVENT_NONE,        /*!< Touch slider is stationary */
    TOUCH_SLIDER_EVENT_RIGHT_SWIPE, /*!< Touch slider is swiped right (from 0 to position_range) */
    TOUCH_SLIDER_EVENT_LEFT_SWIPE,  /*!< Touch slider is swiped left (from position_range to 0) */
    TOUCH_SLIDER_EVENT_RELEASE,     /*!< Touch slider is released */
    TOUCH_SLIDER_EVENT_POSITION,    /*!< Touch slider position is calculated */
} touch_slider_event_t;

/**
 * @brief Configuration structure for touch slider sensor
 */
typedef struct {
    uint32_t channel_num;         /*!< Number of touch slider sensor channels */
    uint32_t *channel_list;       /*!< Touch channel list */
    float *channel_threshold;     /*!< Threshold for touch detection for each channel */
    uint32_t *channel_gold_value; /*!< (Optional) Reference values for touch channels */
    uint32_t debounce_times;      /*!< Number of consecutive readings needed to confirm state change */
    uint32_t filter_reset_times;  /*!< Number of consecutive readings to reset position filter */
    uint32_t position_range;       /*!< The right region of touch slider position range, [0, position_range (less than or equal to 255)] */
    float swipe_threshold;        /*!< The speed threshold for identifying swiping */
    float swipe_hysterisis;       /*!< The speed hysterisis for identifying swiping */
    float swipe_alpha;            /*!< Filter parameter for estimating speed */
    bool skip_lowlevel_init;      /*!< Skip low level initialization when working with existing touch driver */
} touch_slider_config_t;

typedef struct touch_slider_sensor_t *touch_slider_handle_t;

/**
 * @brief Touch slider sensor event callback function type
 *
 * @param handle    Touch slider sensor handle
 * @param event     Touch slider event
 * @param data      Touch slider data. Position if event == TOUCH_SLIDER_EVENT_POSITION, displacement if event == TOUCH_SLIDER_EVENT_RELEASE
 * @param cb_arg    User data passed to the callback
 */
typedef void (*touch_slider_event_cb_t)(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg);

/**
 * @brief Create a touch slider sensor instance
 *
 * This function initializes the touch sensor hardware (unless skip_lowlevel_init is true),
 * sets up FSM instances for touch detection, and registers callbacks for touch events.
 *
 * @param config   Touch slider sensor configuration
 * @param handle   Pointer to receive the created touch slider sensor handle
 * @param cb       Callback function for touch slider events
 * @param cb_arg   User data to pass to callback function
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if config, handle, or required config fields are NULL
 *     - ESP_ERR_NO_MEM if memory allocation fails
 *     - ESP_FAIL if touch sensor or FSM initialization fails
 */
esp_err_t touch_slider_sensor_create(touch_slider_config_t *config, touch_slider_handle_t *handle, touch_slider_event_cb_t cb, void *cb_arg);

/**
 * @brief Delete a touch slider sensor instance
 *
 * Stops all FSMs, unregisters callbacks, frees resources, and optionally
 * deinitializes the touch sensor hardware.
 *
 * @param handle   Touch slider sensor handle to delete
 *
 * @return
 *     - ESP_OK on success (or if handle is NULL)
 *     - ESP_ERR_INVALID_STATE if sensor state is invalid
 */
esp_err_t touch_slider_sensor_delete(touch_slider_handle_t handle);

/**
 * @brief Get touch sensor data for a specific channel and frequency
 *
 * Retrieves the smoothed touch sensor reading from the specified channel
 * and frequency instance.
 *
 * @param handle        Touch slider sensor handle
 * @param channel       Touch channel number
 * @param channel_alt   Frequency instance index (0 to SOC_TOUCH_SAMPLE_CFG_NUM-1)
 * @param data         Pointer to store the smoothed touch sensor data
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle or data is NULL
 *     - ESP_ERR_INVALID_STATE if sensor not initialized
 *     - ESP_ERR_NOT_FOUND if channel not configured
 */
esp_err_t touch_slider_sensor_get_data(touch_slider_handle_t handle, uint32_t channel, uint32_t channel_alt, uint32_t *data);

/**
 * @brief Get touch sensor data for a specific channel and frequency
 *
 * Retrieves the smoothed touch sensor reading from the specified channel
 * and frequency instance.
 *
 * @param handle        Touch slider sensor handle
 * @param pressed       Touch slider is pressed or not
 * @param pos           Touch slider speed
 * @param speed         Touch slider speed
 *
 */
void touch_slider_sensor_get_state(touch_slider_handle_t handle, bool *pressed, uint32_t *pos, float *speed);

/**
 * @brief Handle pending touch sensor events
 *
 * Processes events from all FSM instances. This function should be called
 * periodically to update touch states and trigger callbacks.
 *
 * @param handle    Touch slider sensor handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG if handle is NULL
 *     - ESP_ERR_INVALID_STATE if sensor not initialized
 */
esp_err_t touch_slider_sensor_handle_events(touch_slider_handle_t handle);
