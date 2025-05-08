/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef struct {
    float voltage;                                        /*!< Battery voltage in volts */
    int capacity;                                         /*!< Battery capacity in percentage (0-100) */
} battery_point_t;

// Charging state detection callback function type
typedef bool (*adc_battery_charging_detect_cb_t)(void *user_data);

typedef struct {
    union {
        struct {
            adc_oneshot_unit_handle_t adc_handle;         /*!< External ADC handle */
            adc_cali_handle_t adc_cali_handle;            /*!< External ADC calibration handle */
        } external;                                       /*!< Use external handles */
        struct {
            adc_unit_t adc_unit;                          /*!< ADC unit number */
            adc_bitwidth_t adc_bitwidth;                  /*!< ADC bit width */
            adc_atten_t adc_atten;                        /*!< ADC attenuation */
        } internal;                                       /*!< Create new ADC configuration */
    };                                                    /*!< Use external or internal handles */
    adc_channel_t adc_channel;                            /*!< ADC channel number */

    // Resistor configuration
    float upper_resistor;                                 /*!< Upper resistor value in ohms */
    float lower_resistor;                                 /*!< Lower resistor value in ohms */

    // Battery voltage-capacity mapping configuration
    const battery_point_t *battery_points;                /*!< Array of voltage-capacity mapping points, NULL for default */
    size_t battery_points_count;                          /*!< Number of points in the array, 0 for default */

    // Charging state detection configuration
    adc_battery_charging_detect_cb_t charging_detect_cb;  /*!< Callback function to detect charging state */
    void *charging_detect_user_data;                      /*!< User data passed to the callback function */
} adc_battery_estimation_t;

typedef void *adc_battery_estimation_handle_t;

// Default battery voltage-capacity mapping points
#if CONFIG_OCV_SOC_MODEL_1
#define DEFAULT_POINTS_COUNT 11
static const battery_point_t default_battery_points[DEFAULT_POINTS_COUNT] = {
    {4.16, 100},
    {4.07, 90},
    {3.99, 80},
    {3.90, 70},
    {3.82, 60},
    {3.72, 50},
    {3.61, 40},
    {3.53, 30},
    {3.38, 20},
    {3.20, 10},
    {2.85, 0},
};
#elif CONFIG_OCV_SOC_MODEL_2
#define DEFAULT_POINTS_COUNT 21
static const battery_point_t default_battery_points[DEFAULT_POINTS_COUNT] = {
    {4.177454, 100},
    {4.129486, 95},
    {4.085934, 90},
    {4.045427, 85},
    {4.008118, 80},
    {3.974769, 75},
    {3.945074, 70},
    {3.917968, 65},
    {3.884009, 60},
    {3.841219, 55},
    {3.820965, 50},
    {3.805737, 45},
    {3.79325, 40},
    {3.783504, 35},
    {3.775129, 30},
    {3.762185, 25},
    {3.741018, 20},
    {3.7098, 15},
    {3.686654, 10},
    {3.674776, 5},
    {3.305545, 0},
};
#endif

/**
 * @brief Create a new ADC battery estimation handle
 *
 * @param config Pointer to the ADC battery estimation configuration
 * @return adc_battery_estimation_handle_t Return the ADC battery estimation handle if created successfully, NULL if failed
 */
adc_battery_estimation_handle_t adc_battery_estimation_create(adc_battery_estimation_t *config);

/**
 * @brief Destroy the ADC battery estimation handle
 *
 * @param handle Pointer to the ADC battery estimation handle
 * @return esp_err_t Return ESP_OK if destroyed successfully, ESP_ERR_INVALID_ARG if invalid argument, ESP_FAIL if failed
 */
esp_err_t adc_battery_estimation_destroy(adc_battery_estimation_handle_t handle);

/**
 * @brief Get the battery capacity in percentage
 *
 * @param handle Pointer to the ADC battery estimation handle
 * @param capacity Pointer to the battery capacity in percentage
 * @return esp_err_t Return ESP_OK if get capacity successfully, ESP_ERR_INVALID_ARG if invalid argument, ESP_FAIL if failed
 */
esp_err_t adc_battery_estimation_get_capacity(adc_battery_estimation_handle_t handle, float *capacity);

/**
 * @brief Get the battery charging state
 *
 * @param handle Pointer to the ADC battery estimation handle
 * @param is_charging Pointer to the battery charging state
 * @return esp_err_t Return ESP_OK if get charging state successfully, ESP_ERR_INVALID_ARG if invalid argument, ESP_FAIL if failed
 */
esp_err_t adc_battery_estimation_get_charging_state(adc_battery_estimation_handle_t handle, bool *is_charging);

#ifdef __cplusplus
}
#endif
