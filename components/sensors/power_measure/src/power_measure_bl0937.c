/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "power_measure_interface.h"
#include "power_measure_bl0937.h"
#include "bl0937.h"

static const char *TAG = "power_measure_bl0937";

/**
 * @brief BL0937 power measure device structure
 */
typedef struct {
    power_measure_driver_t base;        /*!< Base driver interface */
    power_measure_config_t config;      /*!< Common configuration */
    power_measure_bl0937_config_t bl0937_config; /*!< BL0937 specific configuration */
    bl0937_handle_t bl0937_handle;   /*!< BL0937 device handle */
} power_measure_bl0937_t;

/**
 * @brief BL0937 driver implementation functions
 */
static esp_err_t bl0937_driver_init(power_measure_driver_t *driver)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);

    driver_bl0937_config_t chip_cfg = {
        .factor = {
            .ki = bl0937_dev->bl0937_config.ki,
            .ku = bl0937_dev->bl0937_config.ku,
            .kp = bl0937_dev->bl0937_config.kp,
        },
        .sel_gpio = bl0937_dev->bl0937_config.sel_gpio,
        .cf1_gpio = bl0937_dev->bl0937_config.cf1_gpio,
        .cf_gpio = bl0937_dev->bl0937_config.cf_gpio,
        .pin_mode = bl0937_dev->bl0937_config.pin_mode,
        .sampling_resistor = bl0937_dev->bl0937_config.sampling_resistor,
        .divider_resistor = bl0937_dev->bl0937_config.divider_resistor,
    };

    return bl0937_create(&bl0937_dev->bl0937_handle, &chip_cfg);
}

static esp_err_t bl0937_driver_deinit(power_measure_driver_t *driver)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);

    if (bl0937_dev->bl0937_handle) {
        return bl0937_delete(bl0937_dev->bl0937_handle);
    }

    return ESP_OK;
}

static esp_err_t bl0937_driver_get_voltage(power_measure_driver_t *driver, float *voltage)
{
    ESP_RETURN_ON_FALSE(voltage, ESP_ERR_INVALID_ARG, TAG, "Invalid voltage pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    float raw_voltage = bl0937_get_voltage(bl0937_dev->bl0937_handle);

    // Apply calibration factor
    *voltage = raw_voltage * bl0937_dev->bl0937_config.ku;
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_current(power_measure_driver_t *driver, float *current)
{
    ESP_RETURN_ON_FALSE(current, ESP_ERR_INVALID_ARG, TAG, "Invalid current pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    float raw_current = bl0937_get_current(bl0937_dev->bl0937_handle);

    // Apply calibration factor
    *current = raw_current * bl0937_dev->bl0937_config.ki;
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_active_power(power_measure_driver_t *driver, float *power)
{
    ESP_RETURN_ON_FALSE(power, ESP_ERR_INVALID_ARG, TAG, "Invalid power pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    float raw_power = bl0937_get_active_power(bl0937_dev->bl0937_handle);

    // Apply calibration factor
    *power = raw_power * bl0937_dev->bl0937_config.kp;
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_power_factor(power_measure_driver_t *driver, float *power_factor)
{
    ESP_RETURN_ON_FALSE(power_factor, ESP_ERR_INVALID_ARG, TAG, "Invalid power_factor pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *power_factor = bl0937_get_power_factor(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_energy(power_measure_driver_t *driver, float *energy)
{
    ESP_RETURN_ON_FALSE(energy, ESP_ERR_INVALID_ARG, TAG, "Invalid energy pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *energy = bl0937_get_energy(bl0937_dev->bl0937_handle) / 3600000;
    return ESP_OK;
}

static esp_err_t bl0937_driver_start_energy_calculation(power_measure_driver_t *driver)
{
    // BL0937 energy calculation is always running
    return ESP_OK;
}

static esp_err_t bl0937_driver_stop_energy_calculation(power_measure_driver_t *driver)
{
    // BL0937 energy calculation cannot be stopped
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t bl0937_driver_reset_energy_calculation(power_measure_driver_t *driver)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    reset_energe(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

static esp_err_t bl0937_driver_calibrate_voltage(power_measure_driver_t *driver, float expected_voltage)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    bl0937_expected_voltage(bl0937_dev->bl0937_handle, expected_voltage);
    return ESP_OK;
}

static esp_err_t bl0937_driver_calibrate_current(power_measure_driver_t *driver, float expected_current)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    bl0937_expected_current(bl0937_dev->bl0937_handle, expected_current);
    return ESP_OK;
}

static esp_err_t bl0937_driver_calibrate_power(power_measure_driver_t *driver, float expected_power)
{
    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    bl0937_expected_active_power(bl0937_dev->bl0937_handle, expected_power);
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_apparent_power(power_measure_driver_t *driver, float *apparent_power)
{
    ESP_RETURN_ON_FALSE(apparent_power, ESP_ERR_INVALID_ARG, TAG, "Invalid apparent_power pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *apparent_power = bl0937_get_apparent_power(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_voltage_multiplier(power_measure_driver_t *driver, float *multiplier)
{
    ESP_RETURN_ON_FALSE(multiplier, ESP_ERR_INVALID_ARG, TAG, "Invalid multiplier pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *multiplier = bl0937_get_voltage_multiplier(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_current_multiplier(power_measure_driver_t *driver, float *multiplier)
{
    ESP_RETURN_ON_FALSE(multiplier, ESP_ERR_INVALID_ARG, TAG, "Invalid multiplier pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *multiplier = bl0937_get_current_multiplier(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

static esp_err_t bl0937_driver_get_power_multiplier(power_measure_driver_t *driver, float *multiplier)
{
    ESP_RETURN_ON_FALSE(multiplier, ESP_ERR_INVALID_ARG, TAG, "Invalid multiplier pointer");

    power_measure_bl0937_t *bl0937_dev = __containerof(driver, power_measure_bl0937_t, base);
    *multiplier = bl0937_get_power_multiplier(bl0937_dev->bl0937_handle);
    return ESP_OK;
}

/**
 * @brief BL0937 factory function
 */
esp_err_t power_measure_new_bl0937_device(const power_measure_config_t *config,
                                          const power_measure_bl0937_config_t *bl0937_config,
                                          power_measure_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && bl0937_config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    power_measure_bl0937_t *bl0937_dev = calloc(1, sizeof(power_measure_bl0937_t));
    ESP_RETURN_ON_FALSE(bl0937_dev, ESP_ERR_NO_MEM, TAG, "Memory allocation failed");

    // Copy configurations
    bl0937_dev->config = *config;
    bl0937_dev->bl0937_config = *bl0937_config;

    // Initialize function pointers
    bl0937_dev->base.init = bl0937_driver_init;
    bl0937_dev->base.deinit = bl0937_driver_deinit;
    bl0937_dev->base.get_voltage = bl0937_driver_get_voltage;
    bl0937_dev->base.get_current = bl0937_driver_get_current;
    bl0937_dev->base.get_active_power = bl0937_driver_get_active_power;
    bl0937_dev->base.get_power_factor = bl0937_driver_get_power_factor;
    bl0937_dev->base.get_energy = bl0937_driver_get_energy;
    bl0937_dev->base.start_energy_calculation = bl0937_driver_start_energy_calculation;
    bl0937_dev->base.stop_energy_calculation = bl0937_driver_stop_energy_calculation;
    bl0937_dev->base.reset_energy_calculation = bl0937_driver_reset_energy_calculation;
    bl0937_dev->base.calibrate_voltage = bl0937_driver_calibrate_voltage;
    bl0937_dev->base.calibrate_current = bl0937_driver_calibrate_current;
    bl0937_dev->base.calibrate_power = bl0937_driver_calibrate_power;
    bl0937_dev->base.get_apparent_power = bl0937_driver_get_apparent_power;
    bl0937_dev->base.get_voltage_multiplier = bl0937_driver_get_voltage_multiplier;
    bl0937_dev->base.get_current_multiplier = bl0937_driver_get_current_multiplier;
    bl0937_dev->base.get_power_multiplier = bl0937_driver_get_power_multiplier;

    // Initialize the driver
    esp_err_t ret = bl0937_dev->base.init(&bl0937_dev->base);
    if (ret != ESP_OK) {
        free(bl0937_dev);
        return ret;
    }

    *ret_handle = (power_measure_handle_t)bl0937_dev;
    return ESP_OK;
}
