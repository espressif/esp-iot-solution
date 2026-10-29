/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "esp_check.h"
#include "power_measure.h"
#include "power_measure_interface.h"

esp_err_t power_measure_get_voltage(power_measure_handle_t handle, float* voltage)
{
    ESP_RETURN_ON_FALSE(handle && voltage, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_voltage, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_voltage not supported");
    return driver->get_voltage(driver, voltage);
}

esp_err_t power_measure_get_current(power_measure_handle_t handle, float* current)
{
    ESP_RETURN_ON_FALSE(handle && current, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_current, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_current not supported");
    return driver->get_current(driver, current);
}

esp_err_t power_measure_get_active_power(power_measure_handle_t handle, float* active_power)
{
    ESP_RETURN_ON_FALSE(handle && active_power, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_active_power, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_active_power not supported");
    return driver->get_active_power(driver, active_power);
}

esp_err_t power_measure_get_power_factor(power_measure_handle_t handle, float* power_factor)
{
    ESP_RETURN_ON_FALSE(handle && power_factor, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_power_factor, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_power_factor not supported");
    return driver->get_power_factor(driver, power_factor);
}

esp_err_t power_measure_get_energy(power_measure_handle_t handle, float* energy)
{
    ESP_RETURN_ON_FALSE(handle && energy, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_energy, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_energy not supported");
    return driver->get_energy(driver, energy);
}

esp_err_t power_measure_start_energy_calculation(power_measure_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->start_energy_calculation, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "start_energy_calculation not supported");
    return driver->start_energy_calculation(driver);
}

esp_err_t power_measure_stop_energy_calculation(power_measure_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->stop_energy_calculation, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "stop_energy_calculation not supported");
    return driver->stop_energy_calculation(driver);
}

esp_err_t power_measure_reset_energy_calculation(power_measure_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->reset_energy_calculation, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "reset_energy_calculation not supported");
    return driver->reset_energy_calculation(driver);
}

esp_err_t power_measure_calibrate_voltage(power_measure_handle_t handle, float expected_voltage)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->calibrate_voltage, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "calibrate_voltage not supported");
    return driver->calibrate_voltage(driver, expected_voltage);
}

esp_err_t power_measure_calibrate_current(power_measure_handle_t handle, float expected_current)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->calibrate_current, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "calibrate_current not supported");
    return driver->calibrate_current(driver, expected_current);
}

esp_err_t power_measure_calibrate_power(power_measure_handle_t handle, float expected_power)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->calibrate_power, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "calibrate_power not supported");
    return driver->calibrate_power(driver, expected_power);
}

esp_err_t power_measure_get_apparent_power(power_measure_handle_t handle, float* apparent_power)
{
    ESP_RETURN_ON_FALSE(handle && apparent_power, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_apparent_power, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_apparent_power not supported");
    return driver->get_apparent_power(driver, apparent_power);
}

esp_err_t power_measure_get_voltage_multiplier(power_measure_handle_t handle, float* multiplier)
{
    ESP_RETURN_ON_FALSE(handle && multiplier, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_voltage_multiplier, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_voltage_multiplier not supported");
    return driver->get_voltage_multiplier(driver, multiplier);
}

esp_err_t power_measure_get_current_multiplier(power_measure_handle_t handle, float* multiplier)
{
    ESP_RETURN_ON_FALSE(handle && multiplier, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_current_multiplier, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_current_multiplier not supported");
    return driver->get_current_multiplier(driver, multiplier);
}

esp_err_t power_measure_get_power_multiplier(power_measure_handle_t handle, float* multiplier)
{
    ESP_RETURN_ON_FALSE(handle && multiplier, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid arguments");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;
    ESP_RETURN_ON_FALSE(driver->get_power_multiplier, ESP_ERR_NOT_SUPPORTED, "POWER_MEASURE", "get_power_multiplier not supported");
    return driver->get_power_multiplier(driver, multiplier);
}

esp_err_t power_measure_delete(power_measure_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, "POWER_MEASURE", "Invalid handle");

    power_measure_driver_t *driver = (power_measure_driver_t *)handle;

    // Call driver deinit if available
    if (driver->deinit) {
        driver->deinit(driver);
    }

    // Free the device memory
    free(handle);

    return ESP_OK;
}
