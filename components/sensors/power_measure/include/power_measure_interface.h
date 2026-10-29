/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power measure driver interface
 *
 * This structure defines the abstract interface that all power measurement
 * chip drivers must implement.
 */
typedef struct power_measure_driver_t power_measure_driver_t;

struct power_measure_driver_t {
    /**
     * @brief Initialize the power measurement chip
     * @param driver Driver instance
     * @return ESP_OK on success
     */
    esp_err_t (*init)(power_measure_driver_t *driver);

    /**
     * @brief Deinitialize the power measurement chip
     * @param driver Driver instance
     * @return ESP_OK on success
     */
    esp_err_t (*deinit)(power_measure_driver_t *driver);

    /**
     * @brief Get voltage measurement
     * @param driver Driver instance
     * @param voltage Pointer to store voltage value (V)
     * @return ESP_OK on success
     */
    esp_err_t (*get_voltage)(power_measure_driver_t *driver, float *voltage);

    /**
     * @brief Get current measurement
     * @param driver Driver instance
     * @param current Pointer to store current value (A)
     * @return ESP_OK on success
     */
    esp_err_t (*get_current)(power_measure_driver_t *driver, float *current);

    /**
     * @brief Get active power measurement
     * @param driver Driver instance
     * @param power Pointer to store power value (W)
     * @return ESP_OK on success
     */
    esp_err_t (*get_active_power)(power_measure_driver_t *driver, float *power);

    /**
     * @brief Get power factor
     * @param driver Driver instance
     * @param power_factor Pointer to store power factor value
     * @return ESP_OK on success
     */
    esp_err_t (*get_power_factor)(power_measure_driver_t *driver, float *power_factor);

    /**
     * @brief Get energy measurement
     * @param driver Driver instance
     * @param energy Pointer to store energy value (kWh)
     * @return ESP_OK on success
     */
    esp_err_t (*get_energy)(power_measure_driver_t *driver, float *energy);

    /**
     * @brief Start energy calculation
     * @param driver Driver instance
     * @return ESP_OK on success
     */
    esp_err_t (*start_energy_calculation)(power_measure_driver_t *driver);

    /**
     * @brief Stop energy calculation
     * @param driver Driver instance
     * @return ESP_OK on success
     */
    esp_err_t (*stop_energy_calculation)(power_measure_driver_t *driver);

    /**
     * @brief Reset energy calculation
     * @param driver Driver instance
     * @return ESP_OK on success
     */
    esp_err_t (*reset_energy_calculation)(power_measure_driver_t *driver);

    /**
     * @brief Calibrate voltage (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param expected_voltage Expected voltage value for calibration
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*calibrate_voltage)(power_measure_driver_t *driver, float expected_voltage);

    /**
     * @brief Calibrate current (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param expected_current Expected current value for calibration
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*calibrate_current)(power_measure_driver_t *driver, float expected_current);

    /**
     * @brief Calibrate power (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param expected_power Expected power value for calibration
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*calibrate_power)(power_measure_driver_t *driver, float expected_power);

    /**
     * @brief Get apparent power (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param apparent_power Pointer to store apparent power value (VA)
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*get_apparent_power)(power_measure_driver_t *driver, float *apparent_power);

    /**
     * @brief Get voltage multiplier (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param multiplier Pointer to store voltage multiplier value
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*get_voltage_multiplier)(power_measure_driver_t *driver, float *multiplier);

    /**
     * @brief Get current multiplier (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param multiplier Pointer to store current multiplier value
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*get_current_multiplier)(power_measure_driver_t *driver, float *multiplier);

    /**
     * @brief Get power multiplier (optional, may return ESP_ERR_NOT_SUPPORTED)
     * @param driver Driver instance
     * @param multiplier Pointer to store power multiplier value
     * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if not supported
     */
    esp_err_t (*get_power_multiplier)(power_measure_driver_t *driver, float *multiplier);
};

#ifdef __cplusplus
}
#endif
