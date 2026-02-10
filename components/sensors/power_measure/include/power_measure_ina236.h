/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POWER_MEASURE_INA236__
#define __POWER_MEASURE_INA236__

#include "power_measure.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief INA236 specific configuration
 */
typedef struct {
    i2c_bus_handle_t i2c_bus;       /*!< I2C bus handle */
    uint8_t i2c_addr;               /*!< I2C address */
    bool alert_en;                  /*!< Enable alert pin */
    int alert_pin;                  /*!< Alert pin number (-1 if unused) */
    void (*alert_cb)(void *arg);    /*!< Alert callback */
} power_measure_ina236_config_t;

/**
 * @brief Create a new INA236 power measurement device
 *
 * @param config Common power measure configuration
 * @param ina236_config INA236 specific configuration
 * @param ret_handle Pointer to store the created device handle
 * @return
 *     - ESP_OK: Successfully created the device
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 *     - ESP_ERR_INVALID_STATE: Device already initialized
 */
esp_err_t power_measure_new_ina236_device(const power_measure_config_t *config,
                                          const power_measure_ina236_config_t *ina236_config,
                                          power_measure_handle_t *ret_handle);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_MEASURE_INA236__ */
