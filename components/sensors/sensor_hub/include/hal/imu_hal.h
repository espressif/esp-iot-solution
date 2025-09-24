/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IMU_HAL_H_
#define _IMU_HAL_H_

#ifdef CONFIG_SENSOR_INCLUDED_IMU

#include "i2c_bus.h"
#include "esp_err.h"
#include "sensor_type.h"

typedef void *sensor_imu_handle_t; /*!< imu sensor handle*/

/**
 * @brief Implementation Interface for IMU Sensors
 *
 */
typedef struct {
    /**
     * @brief Initialize the IMU sensor
     *
     * @param handle The bus handle
     * @param addr The I2C address of the sensor
     * @return esp_err_t Result of initialization
     */
    esp_err_t (*init)(bus_handle_t, uint8_t addr);

    /**
     * @brief Deinitialize the IMU sensor
     *
     * @return esp_err_t Result of deinitialization
     */
    esp_err_t (*deinit)(void);

    /**
     * @brief Test the IMU sensor
     *
     * @return esp_err_t Result of the test
     */
    esp_err_t (*test)(void);

    /**
     * @brief Acquire accelerometer data
     *
     * @param[out] acce_x Pointer to store the X-axis accelerometer data
     * @param[out] acce_y Pointer to store the Y-axis accelerometer data
     * @param[out] acce_z Pointer to store the Z-axis accelerometer data
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_acce)(float *acce_x, float *acce_y, float *acce_z);

    /**
     * @brief Acquire gyroscope data
     *
     * @param[out] gyro_x Pointer to store the X-axis gyroscope data
     * @param[out] gyro_y Pointer to store the Y-axis gyroscope data
     * @param[out] gyro_z Pointer to store the Z-axis gyroscope data
     * @return esp_err_t Result of acquiring data
     */
    esp_err_t (*acquire_gyro)(float *gyro_x, float *gyro_y, float *gyro_z);

    /**
     * @brief Put the IMU sensor to sleep
     *
     * @return esp_err_t Result of the sleep operation
     */
    esp_err_t (*sleep)(void);

    /**
     * @brief Wake up the IMU sensor
     *
     * @return esp_err_t Result of the wake-up operation
     */
    esp_err_t (*wakeup)(void);

    /**
     * @brief Set sensor work mode
     *
     * @return esp_err_t Result of setting work mode
     */
    esp_err_t (*set_mode)(sensor_mode_t work_mode);

    /**
     * @brief Set sensor measurement range
     *
     * @return esp_err_t Result of setting measurement range
     */
    esp_err_t (*set_range)(sensor_range_t range);
} imu_impl_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a Inertial Measurement Unit sensor instance.
 *
 * @param bus i2c/spi bus handle the sensor attached to
 * @param sensor_name name of sensor
 * @param addr addr of sensor
 * @return sensor_imu_handle_t return imu sensor handle if succeed, NULL is failed.
 */
sensor_imu_handle_t imu_create(bus_handle_t bus, const char *sensor_name, uint8_t addr);

/**
 * @brief Delete and release the sensor resource.
 *
 * @param sensor point to imu sensor handle, will set to NULL if delete succeed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_delete(sensor_imu_handle_t *sensor);

/**
 * @brief Test if sensor is active.
 *
 * @param sensor imu sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_test(sensor_imu_handle_t sensor);

/**
 * @brief Acquire imu sensor accelerometer result one time.
 *
 * @param sensor imu sensor handle to operate
 * @param acce result data (unit:g)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t imu_acquire_acce(sensor_imu_handle_t sensor, axis3_t* acce);

/**
 * @brief Acquire imu sensor gyroscope result one time.
 *
 * @param sensor imu sensor handle to operate
 * @param gyro result data (unit:dps)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t imu_acquire_gyro(sensor_imu_handle_t sensor, axis3_t* gyro);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor imu sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t imu_sleep(sensor_imu_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor imu sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t imu_wakeup(sensor_imu_handle_t sensor);

/**
 * @brief acquire a group of sensor data
 *
 * @param sensor imu sensor handle to operate
 * @param data_group acquired data
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t imu_acquire(sensor_imu_handle_t sensor, sensor_data_group_t *data_group);

/**
 * @brief control sensor mode with control commands and args
 *
 * @param sensor imu sensor handle to operate
 * @param cmd control commands detailed in sensor_command_t
 * @param args control commands args
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
 */
esp_err_t imu_control(sensor_imu_handle_t sensor, sensor_command_t cmd, void *args);

#ifdef __cplusplus
}
#endif

#endif

#endif
