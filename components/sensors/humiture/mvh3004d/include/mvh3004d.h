/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_mvh3004d_H_
#define _IOT_mvh3004d_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_bus.h"

typedef void *mvh3004d_handle_t;
#define MVH3004D_SLAVE_ADDR    (0x44)

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 * @param dev_addr I2C device address of sensor
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
mvh3004d_handle_t mvh3004d_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor point to object handle of mvh3004d
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mvh3004d_delete(mvh3004d_handle_t *sensor);

/**
 * @brief read temperature and huminity
 * @param sensor object handle of mvh3004d
 * @tp pointer to accept temperature
 * @rh pointer to accept huminity
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mvh3004d_get_data(mvh3004d_handle_t sensor, float *tp, float *rh);

/**
 * @brief read huminity
 * @param sensor object handle of mvh3004d
 * @rh pointer to accept huminity
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mvh3004d_get_huminity(mvh3004d_handle_t sensor, float *rh);

/**
 * @brief read temperature
 * @param sensor object handle of mvh3004d
 * @tp pointer to accept temperature
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t mvh3004d_get_temperature(mvh3004d_handle_t sensor, float *tp);

#ifdef __cplusplus
}
#endif

#endif
