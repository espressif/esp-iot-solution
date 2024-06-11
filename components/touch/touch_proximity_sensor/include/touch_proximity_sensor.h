/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdio.h>
#include "driver/touch_pad.h"

#define TOUCH_PROXIMITY_NUM_MAX    SOC_TOUCH_PROXIMITY_CHANNEL_NUM

typedef enum {
    PROXI_EVT_INACTIVE = 0,
    PROXI_EVT_ACTIVE,
} proxi_evt_t;

/**
 * @brief Configuration structure for touch proximity sensor
 *
 * This structure defines the configuration parameters for a touch proximity sensor.
 *
 */
typedef struct {
    uint32_t channel_num;                           /*!< Number of touch proximity sensor channels */
    uint32_t channel_list[TOUCH_PROXIMITY_NUM_MAX]; /*!< Touch proximity sensor channel list */
    uint32_t meas_count;                            /*!< Accumulated measurement count */
    float smooth_coef;                              /*!< Smoothing coefficient */
    float baseline_coef;                            /*!< Baseline coefficient */
    float max_p;                                    /*!< Maximum effective positive change rate */
    float min_n;                                    /*!< Minimum effective negative change rate */
    float threshold_p[TOUCH_PROXIMITY_NUM_MAX];     /*!< Positive threshold */
    float threshold_n[TOUCH_PROXIMITY_NUM_MAX];     /*!< Negative threshold */
    float hysteresis_p;                             /*!< Hysteresis for positive threshold */
    float noise_p;                                  /*!< Positive noise threshold */
    float noise_n;                                  /*!< Negative noise threshold */
    uint32_t debounce_p;                            /*!< Debounce times for positive threshold */
    uint32_t debounce_n;                            /*!< Debounce times for negative threshold */
    uint32_t reset_p;                               /*!< Baseline reset positive threshold*/
    uint32_t reset_n;                               /*!< Baseline reset negative threshold*/
    uint32_t gold_value[TOUCH_PROXIMITY_NUM_MAX];   /*!< Gold value */
} proxi_config_t;

typedef struct touch_proximity_sensor_t *touch_proximity_handle_t;

#define DEFAULTS_PROX_CONFIGS()\
{\
    .meas_count = 50,\
    .smooth_coef = 0.2,\
    .baseline_coef = 0.1,\
    .max_p = 0.2,\
    .min_n = 0.08,\
    .threshold_n[0] = 0.002,\
    .hysteresis_p = 0.2,\
    .noise_p = 0.001,\
    .noise_n = 0.001,\
    .debounce_p = 2,\
    .debounce_n = 1,\
    .reset_p = 1000,\
    .reset_n = 3,\
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * proximity sensor user callback type
 */
typedef void(*proxi_cb_t)(uint32_t channel, proxi_evt_t event, void *cb_arg);

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
esp_err_t touch_proximity_sensor_create(proxi_config_t *config, touch_proximity_handle_t *sensor_handle, proxi_cb_t cb, void *cb_arg);

/**
 * @brief Start the touch proximity sensor.
 *
 * This function starts the touch proximity sensor operation.
 *
 * @param proxi_sensor Pointer to the handle of the touch proximity sensor instance.
 * @return
 *      - ESP_OK: Start the touch proximity sensor successfully
 *      - ESP_ERR_INVALID_ARG: The touch proximity sensor failed to start (touch_proximity_handle_t is NULL, or channel_num is zero).
 *      - ESP_FAIL: The touch proximity sensor failed to start (failed to create queue for touch pad).
 */
esp_err_t touch_proximity_sensor_start(touch_proximity_handle_t proxi_sensor);

/**
 * @brief Stop the touch proximity sensor.
 *
 * This function stops the operation of the touch proximity sensor associated with the provided sensor handle.
 *
 * @param proxi_sensor Pointer to the handle of the touch proximity sensor instance.
 * @return
 *      - ESP_OK: Stop the touch proximity sensor successfully
 */
esp_err_t touch_proximity_sensor_stop(touch_proximity_handle_t proxi_sensor);

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

#ifdef __cplusplus
}
#endif
