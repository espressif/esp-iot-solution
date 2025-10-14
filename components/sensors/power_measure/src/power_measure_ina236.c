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
#include "power_measure_ina236.h"
#include "ina236.h"

static const char *TAG = "power_measure_ina236";

/**
 * @brief INA236 power measure device structure
 */
typedef struct {
    power_measure_driver_t base;        /*!< Base driver interface */
    power_measure_config_t config;      /*!< Common configuration */
    power_measure_ina236_config_t ina236_config; /*!< INA236 specific configuration */
    ina236_handle_t ina236_handle;      /*!< INA236 chip handle */
} power_measure_ina236_t;

/**
 * @brief INA236 driver implementation functions
 */
static esp_err_t ina236_driver_init(power_measure_driver_t *driver)
{
    power_measure_ina236_t *ina236_dev = __containerof(driver, power_measure_ina236_t, base);

    driver_ina236_config_t ina236_cfg = {
        .bus = ina236_dev->ina236_config.i2c_bus,
        .alert_en = ina236_dev->ina236_config.alert_en,
        .dev_addr = ina236_dev->ina236_config.i2c_addr,
        .alert_pin = (uint8_t)ina236_dev->ina236_config.alert_pin,
        .alert_cb = (int236_alert_cb_t)ina236_dev->ina236_config.alert_cb,
    };

    return ina236_create(&ina236_dev->ina236_handle, &ina236_cfg);
}

static esp_err_t ina236_driver_deinit(power_measure_driver_t *driver)
{
    power_measure_ina236_t *ina236_dev = __containerof(driver, power_measure_ina236_t, base);

    if (ina236_dev->ina236_handle) {
        return ina236_delete(ina236_dev->ina236_handle);
    }
    return ESP_OK;
}

static esp_err_t ina236_driver_get_voltage(power_measure_driver_t *driver, float *voltage)
{
    power_measure_ina236_t *ina236_dev = __containerof(driver, power_measure_ina236_t, base);
    return ina236_get_voltage(ina236_dev->ina236_handle, voltage);
}

static esp_err_t ina236_driver_get_current(power_measure_driver_t *driver, float *current)
{
    power_measure_ina236_t *ina236_dev = __containerof(driver, power_measure_ina236_t, base);
    return ina236_get_current(ina236_dev->ina236_handle, current);
}

static esp_err_t ina236_driver_get_active_power(power_measure_driver_t *driver, float *power)
{
    // INA236 doesn't directly provide power, calculate from V*I
    float voltage, current;
    esp_err_t ret = ina236_driver_get_voltage(driver, &voltage);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ina236_driver_get_current(driver, &current);
    if (ret != ESP_OK) {
        return ret;
    }

    *power = voltage * current;
    return ESP_OK;
}

static esp_err_t ina236_driver_get_power_factor(power_measure_driver_t *driver, float *power_factor)
{
    // INA236 doesn't provide power factor, assume 1.0 for DC
    *power_factor = 1.0f;
    return ESP_OK;
}

static esp_err_t ina236_driver_get_apparent_power(power_measure_driver_t *driver, float *apparent_power)
{
    // For INA236, apparent power equals active power (DC)
    return ina236_driver_get_active_power(driver, apparent_power);
}

/**
 * @brief INA236 factory function
 */
esp_err_t power_measure_new_ina236_device(const power_measure_config_t *config,
                                          const power_measure_ina236_config_t *ina236_config,
                                          power_measure_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && ina236_config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    power_measure_ina236_t *ina236_dev = calloc(1, sizeof(power_measure_ina236_t));
    ESP_RETURN_ON_FALSE(ina236_dev, ESP_ERR_NO_MEM, TAG, "Memory allocation failed");

    // Copy configurations
    ina236_dev->config = *config;
    ina236_dev->ina236_config = *ina236_config;

    // Initialize function pointers
    ina236_dev->base.init = ina236_driver_init;
    ina236_dev->base.deinit = ina236_driver_deinit;
    ina236_dev->base.get_voltage = ina236_driver_get_voltage;
    ina236_dev->base.get_current = ina236_driver_get_current;
    ina236_dev->base.get_active_power = ina236_driver_get_active_power;
    ina236_dev->base.get_power_factor = ina236_driver_get_power_factor;
    ina236_dev->base.get_energy = NULL;  // INA236 doesn't support energy measurement
    ina236_dev->base.start_energy_calculation = NULL;  // Not supported
    ina236_dev->base.stop_energy_calculation = NULL;   // Not supported
    ina236_dev->base.reset_energy_calculation = NULL; // Not supported
    ina236_dev->base.calibrate_voltage = NULL;        // Not supported
    ina236_dev->base.calibrate_current = NULL;        // Not supported
    ina236_dev->base.calibrate_power = NULL;           // Not supported
    ina236_dev->base.get_apparent_power = ina236_driver_get_apparent_power;
    ina236_dev->base.get_voltage_multiplier = NULL;    // Not supported
    ina236_dev->base.get_current_multiplier = NULL;   // Not supported
    ina236_dev->base.get_power_multiplier = NULL;      // Not supported

    // Initialize the driver
    esp_err_t ret = ina236_dev->base.init(&ina236_dev->base);
    if (ret != ESP_OK) {
        free(ina236_dev);
        return ret;
    }

    *ret_handle = (power_measure_handle_t)ina236_dev;
    return ESP_OK;
}
