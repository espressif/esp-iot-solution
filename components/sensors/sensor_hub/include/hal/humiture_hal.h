/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _HUMITURE_HAL_H_
#define _HUMITURE_HAL_H_

#ifdef CONFIG_SENSOR_INCLUDED_HUMITURE

#include "i2c_bus.h"
#include "sensor_type.h"

typedef void *sensor_humiture_handle_t; /*!< humiture sensor handle*/

/**
 * @brief Implementation Interface for Humiture Sensors
 *
 */
typedef struct {
    /**
     * @brief Initialize the humiture sensor
     *
     * @param[out] sensor_ctx Pointer to receive the driver's per-instance context
     * @param handle The bus handle
     * @param addr The I2C address of the sensor
     * @return esp_err_t Result of initialization
     */
    esp_err_t (*init)(void **sensor_ctx, bus_handle_t handle, uint8_t addr);

    /**
     * @brief Deinitialize the humiture sensor
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of deinitialization
     */
    esp_err_t (*deinit)(void *sensor_ctx);

    /**
     * @brief Test the humiture sensor
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of the test
     */
    esp_err_t (*test)(void *sensor_ctx);

    /**
     * @brief Acquire humidity data from the sensor
     *
     * @param sensor_ctx The per-instance context returned by init
     * @param[out] humidity Pointer to store the humidity data
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_humidity)(void *sensor_ctx, float *humidity);

    /**
     * @brief Acquire temperature data from the sensor
     *
     * @param sensor_ctx The per-instance context returned by init
     * @param[out] temperature Pointer to store the temperature data
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_temperature)(void *sensor_ctx, float *temperature);

    /**
     * @brief Put the humiture sensor to sleep
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of the sleep operation
     */
    esp_err_t (*sleep)(void *sensor_ctx);

    /**
     * @brief Wake up the humiture sensor
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of the wake-up operation
     */
    esp_err_t (*wakeup)(void *sensor_ctx);

    /**
     * @brief Set sensor work mode
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of setting work mode
     */
    esp_err_t (*set_mode)(void *sensor_ctx, sensor_mode_t work_mode);

    /**
     * @brief Set sensor measurement range
     *
     * @param sensor_ctx The per-instance context returned by init
     * @return esp_err_t Result of setting measurement range
     */
    esp_err_t (*set_range)(void *sensor_ctx, sensor_range_t range);
} humiture_impl_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a humiture/temperature sensor instance.
 *
 * @param bus i2c/spi bus handle the sensor attached to
 * @param sensor_name name of sensor
 * @param addr addr of sensor
 * @return sensor_humiture_handle_t return humiture sensor handle if succeed, return NULL if create failed.
 */
sensor_humiture_handle_t humiture_create(bus_handle_t bus, const char *sensor_name, uint8_t addr);

/**
 * @brief Delete and release the sensor resource.
 *
 * @param sensor point to humiture sensor handle, will set to NULL if delete succeed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t humiture_delete(sensor_humiture_handle_t *sensor);

/**
 * @brief Test if sensor is active
 *
 * @param sensor humiture sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t humiture_test(sensor_humiture_handle_t sensor);

/**
 * @brief Acquire humiture sensor relative humidity result one time.
 *
 * @param sensor humiture sensor handle to operate.
 * @param humidity result data (unit:percentage)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t humiture_acquire_humidity(sensor_humiture_handle_t sensor, float *humidity);

/**
 * @brief Acquire humiture sensor temperature result one time.
 *
 * @param sensor humiture sensor handle to operate.
 * @param sensor_data result data (unit:dCelsius)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t humiture_acquire_temperature(sensor_humiture_handle_t sensor, float *sensor_data);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor humiture sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t humiture_sleep(sensor_humiture_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor humiture sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t humiture_wakeup(sensor_humiture_handle_t sensor);

/**
 * @brief acquire a group of sensor data
 *
 * @param sensor humiture sensor handle to operate
 * @param data_group acquired data
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t humiture_acquire(sensor_humiture_handle_t sensor, sensor_data_group_t *data_group);

/**
 * @brief control sensor mode with control commands and args
 *
 * @param sensor humiture sensor handle to operate
 * @param cmd control commands detailed in sensor_command_t
 * @param args control commands args
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
 */
esp_err_t humiture_control(sensor_humiture_handle_t sensor, sensor_command_t cmd, void *args);

#ifdef __cplusplus
}
#endif

#endif

#endif
