/* SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
* SPDX-License-Identifier: Apache-2.0
*/

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "power_measure_bl0942.h"
#include "power_measure_interface.h"
#include "bl0942.h"

static const char *TAG = "power_measure_bl0942";

typedef struct {
    power_measure_driver_t base;               /*!< Base driver interface */
    power_measure_config_t config;             /*!< Common configuration */
    power_measure_bl0942_config_t bl0942_config; /*!< BL0942 specific configuration */
    bl0942_handle_t bl0942_handle;                      /*!< BL0942 chip handle */
} power_measure_bl0942_t;

static esp_err_t bl0942_driver_init(power_measure_driver_t *driver)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    bl0942_config_t cfg = {
        .addr = dev->bl0942_config.addr,
        .shunt_resistor_ohm = dev->bl0942_config.shunt_resistor,
        .voltage_div_ratio = dev->bl0942_config.divider_ratio,
        .use_spi = dev->bl0942_config.use_spi,
    };

    if (dev->bl0942_config.use_spi) {
        cfg.spi.spi_host = dev->bl0942_config.spi.spi_host;
        cfg.spi.mosi_io_num = dev->bl0942_config.spi.mosi_io;
        cfg.spi.miso_io_num = dev->bl0942_config.spi.miso_io;
        cfg.spi.sclk_io_num = dev->bl0942_config.spi.sclk_io;
        cfg.spi.cs_io_num = dev->bl0942_config.spi.cs_io;
    } else {
        cfg.uart.uart_num = dev->bl0942_config.uart.uart_num;
        cfg.uart.tx_io_num = dev->bl0942_config.uart.tx_io;
        cfg.uart.rx_io_num = dev->bl0942_config.uart.rx_io;
        cfg.uart.sel_io_num = dev->bl0942_config.uart.sel_io;
        cfg.uart.baud_rate = dev->bl0942_config.uart.baud;
    }

    return bl0942_create(&dev->bl0942_handle, &cfg);
}

static esp_err_t bl0942_driver_deinit(power_measure_driver_t *driver)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    if (dev->bl0942_handle) {
        return bl0942_delete(dev->bl0942_handle);
    }

    return ESP_OK;
}

static esp_err_t bl0942_driver_get_voltage(power_measure_driver_t *driver, float *voltage)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    return bl0942_get_voltage(dev->bl0942_handle, voltage);
}

static esp_err_t bl0942_driver_get_current(power_measure_driver_t *driver, float *current)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    return bl0942_get_current(dev->bl0942_handle, current);
}

static esp_err_t bl0942_driver_get_active_power(power_measure_driver_t *driver, float *power)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    return bl0942_get_active_power(dev->bl0942_handle, power);
}

static esp_err_t bl0942_driver_get_power_factor(power_measure_driver_t *driver, float *power_factor)
{
    /* Not directly provided: approximate as P/(V*I) */
    float v = 0, i = 0, p = 0;

    esp_err_t r;
    r = bl0942_driver_get_voltage(driver, &v); if (r != ESP_OK) return r;
    r = bl0942_driver_get_current(driver, &i); if (r != ESP_OK) return r;
    r = bl0942_driver_get_active_power(driver, &p); if (r != ESP_OK) return r;

    if (v <= 0 || i <= 0) {
        *power_factor = 0.0f;
    } else {
        float s = v * i;
        *power_factor = (s > 0) ? (p / s) : 0.0f;
        if (*power_factor > 1.0f) {
            *power_factor = 1.0f;
        }
        if (*power_factor < -1.0f) {
            *power_factor = -1.0f;
        }
    }

    return ESP_OK;
}

static esp_err_t bl0942_driver_get_energy(power_measure_driver_t *driver, float *energy)
{
    power_measure_bl0942_t *dev = __containerof(driver, power_measure_bl0942_t, base);

    uint32_t pulses = 0;
    esp_err_t r = bl0942_get_energy_pulses(dev->bl0942_handle, &pulses);
    if (r != ESP_OK) {
        return r;
    }

    /* Caller can post-scale pulses to kWh based on calibration; here return pulse count as Wh proxy */
    *energy = (float)pulses; /* raw pulses */

    return ESP_OK;
}

static esp_err_t bl0942_driver_start_energy_calculation(power_measure_driver_t *driver)
{
    return ESP_OK;
}

static esp_err_t bl0942_driver_get_apparent_power(power_measure_driver_t *driver, float *apparent_power)
{
    float v = 0, i = 0;

    esp_err_t r;
    r = bl0942_driver_get_voltage(driver, &v); if (r != ESP_OK) return r;
    r = bl0942_driver_get_current(driver, &i); if (r != ESP_OK) return r;

    *apparent_power = v * i;

    return ESP_OK;
}

/* Factory function aligned with other devices */
esp_err_t power_measure_new_bl0942_device(const power_measure_config_t *config,
                                          const power_measure_bl0942_config_t *bl0942_config,
                                          power_measure_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(config && bl0942_config && ret_handle, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    power_measure_bl0942_t *dev = calloc(1, sizeof(power_measure_bl0942_t));
    ESP_RETURN_ON_FALSE(dev, ESP_ERR_NO_MEM, TAG, "Memory allocation failed");

    dev->config = *config;
    dev->bl0942_config = *bl0942_config;

    dev->base.init = bl0942_driver_init;
    dev->base.deinit = bl0942_driver_deinit;
    dev->base.get_voltage = bl0942_driver_get_voltage;
    dev->base.get_current = bl0942_driver_get_current;
    dev->base.get_active_power = bl0942_driver_get_active_power;
    dev->base.get_power_factor = bl0942_driver_get_power_factor;
    dev->base.get_energy = bl0942_driver_get_energy;
    dev->base.start_energy_calculation = bl0942_driver_start_energy_calculation;
    dev->base.stop_energy_calculation = NULL;
    dev->base.reset_energy_calculation = NULL;
    dev->base.calibrate_voltage = NULL;
    dev->base.calibrate_current = NULL;
    dev->base.calibrate_power = NULL;
    dev->base.get_apparent_power = bl0942_driver_get_apparent_power;
    dev->base.get_voltage_multiplier = NULL;
    dev->base.get_current_multiplier = NULL;
    dev->base.get_power_multiplier = NULL;

    esp_err_t ret = dev->base.init(&dev->base);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }
    *ret_handle = (power_measure_handle_t)dev;

    return ESP_OK;
}
