/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INA236_I2C_ADDRESS_DEFAULT   (0x41)

/**
 * @brief INA236 handle
 *
 */
typedef struct ina236_dev_t *ina236_handle_t;

/**
 * @brief INA236 alert callback function
 *
 */
typedef void (*int236_alert_cb_t)(void *arg);

/**
 * @brief INA236 configuration structure
 *
 */
typedef struct {
    i2c_bus_handle_t bus;         /*!< I2C bus object */
    bool alert_en;                /*!< Enable alert callback */
    uint8_t dev_addr;             /*!< I2C device address */
    uint8_t alert_pin;            /*!< Alert pin number */
    int236_alert_cb_t alert_cb;   /*!< Alert callback function */
} driver_ina236_config_t;

/**
 * @brief Create and init object and return a handle
 *
 * @param handle Pointer to handle
 * @param config Pointer to configuration
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t ina236_create(ina236_handle_t *handle, driver_ina236_config_t *config);

/**
 * @brief Deinit object and free memory
 *
 * @param handle ina236 handle Handle
 *
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t ina236_delete(ina236_handle_t handle);

/**
 * @brief Get the Voltage on the bus
 *
 * @param handle object handle of ina236
 * @param volt Voltage value in volts
 * @return
 *    - ESP_OK Success
 *    - Others Fail
 */
esp_err_t ina236_get_voltage(ina236_handle_t handle, float *volt);

/**
 * @brief Get the Current on the bus
 *
 *
 * @param handle object handle of ina236
 * @param curr Current value in A
 * @return
 *     - ESP_OK Success
 *     - Others Fail
 */
esp_err_t ina236_get_current(ina236_handle_t handle, float *curr);

/**
 * @brief  Clear the mask of the alert pin
 *
 * @param handle object handle of ina236
 * @return
 *      - ESP_OK Success
 *      - Others Fail
 */
esp_err_t ina236_clear_mask(ina236_handle_t handle);

#ifdef __cplusplus
}
#endif
