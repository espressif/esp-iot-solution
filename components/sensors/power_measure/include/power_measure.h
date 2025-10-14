/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POWER_MEASURE__
#define __POWER_MEASURE__

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "power_measure_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common power measure configuration
 */
typedef struct {
    uint16_t overcurrent;            /*!< Overcurrent threshold */
    uint16_t overvoltage;            /*!< Overvoltage threshold */
    uint16_t undervoltage;           /*!< Undervoltage threshold */
    bool enable_energy_detection;    /*!< Flag to enable energy detection */
} power_measure_config_t;

/**
 * @brief Power measure device handle
 */
typedef struct power_measure_dev_t *power_measure_handle_t;

// ============================================================================
// Common API Functions
// ============================================================================

/**
 * @brief Delete a power measurement device
 *
 * @param handle Power measure device handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t power_measure_delete(power_measure_handle_t handle);

// ============================================================================
// Measurement API Functions
// ============================================================================

/**
 * @brief Get current voltage (V)
 *
 * @param handle Power measure device handle
 * @param voltage Pointer to store the voltage value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 */
esp_err_t power_measure_get_voltage(power_measure_handle_t handle, float* voltage);

/**
 * @brief Get current current (A)
 *
 * @param handle Power measure device handle
 * @param current Pointer to store the current value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 */
esp_err_t power_measure_get_current(power_measure_handle_t handle, float* current);

/**
 * @brief Get current active power (W)
 *
 * @param handle Power measure device handle
 * @param power Pointer to store the power value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 */
esp_err_t power_measure_get_active_power(power_measure_handle_t handle, float* power);

/**
 * @brief Get power factor
 *
 * @param handle Power measure device handle
 * @param power_factor Pointer to store the power factor value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_power_factor(power_measure_handle_t handle, float* power_factor);

/**
 * @brief Get energy consumption (kWh)
 *
 * @param handle Power measure device handle
 * @param energy Pointer to store the energy value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_energy(power_measure_handle_t handle, float* energy);

/**
 * @brief Reset energy calculation
 *
 * @param handle Power measure device handle
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid handle
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_reset_energy_calculation(power_measure_handle_t handle);

/**
 * @brief Get apparent power measurement
 *
 * @param handle Power measure device handle
 * @param apparent_power Pointer to store the apparent power value in volt-amperes (VA)
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_apparent_power(power_measure_handle_t handle, float* apparent_power);

// ============================================================================
// Calibration API Functions (BL0937 specific)
// ============================================================================

/**
 * @brief Dynamic runtime calibration for voltage (BL0937 only)
 *
 * @param handle Power measure device handle
 * @param expected_voltage The known reference voltage value in volts
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_calibrate_voltage(power_measure_handle_t handle, float expected_voltage);

/**
 * @brief Dynamic runtime calibration for current (BL0937 only)
 *
 * @param handle Power measure device handle
 * @param expected_current The known reference current value in amperes
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_calibrate_current(power_measure_handle_t handle, float expected_current);

/**
 * @brief Dynamic runtime calibration for power (BL0937 only)
 *
 * @param handle Power measure device handle
 * @param expected_power The known reference power value in watts
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_calibrate_power(power_measure_handle_t handle, float expected_power);

// ============================================================================
// Advanced API Functions (BL0937 specific)
// ============================================================================

/**
 * @brief Get voltage multiplier (BL0937 only, advanced users)
 *
 * @param handle Power measure device handle
 * @param multiplier Pointer to store the voltage multiplier value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_voltage_multiplier(power_measure_handle_t handle, float* multiplier);

/**
 * @brief Get current multiplier (BL0937 only, advanced users)
 *
 * @param handle Power measure device handle
 * @param multiplier Pointer to store the current multiplier value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_current_multiplier(power_measure_handle_t handle, float* multiplier);

/**
 * @brief Get power multiplier (BL0937 only, advanced users)
 *
 * @param handle Power measure device handle
 * @param multiplier Pointer to store the power multiplier value
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_INVALID_STATE: Device not initialized
 *     - ESP_ERR_NOT_SUPPORTED: Chip doesn't support this feature
 */
esp_err_t power_measure_get_power_multiplier(power_measure_handle_t handle, float* multiplier);

#ifdef __cplusplus
}
#endif

#endif /**< __POWER_MEASURE__ */
