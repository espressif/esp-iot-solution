// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _IMU_H_
#define _IMU_H_

#include "i2c_bus.h"
#include "esp_err.h"
#include "iot_sensor_type.h"

typedef void *bus_handle_t;
typedef void *sensor_imu_handle_t;

typedef enum {
    MPU6050_ID = 0x01,
    LIS2DH12_ID,
    IMU_MAX_ID,
} imu_id_t;

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @brief Create a Inertial Measurement Unit sensor instance.
 * Same series' sensor or sensor with same address can only be created once.
 *
 * @param bus i2c bus handle the sensor attached to
 * @param imu_id id declared in imu_id_t
 * @return sensor_imu_handle_t return imu sensor handle if succeed, NULL is failed.
 */
sensor_imu_handle_t imu_create(bus_handle_t bus, int imu_id);

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
 * @param sensor_data result data (unit:g)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_acquire_acce(sensor_imu_handle_t sensor, sensor_data_t *sensor_data);

/**
 * @brief Acquire imu sensor gyroscope result one time.
 *
 * @param sensor imu sensor handle to operate
 * @param sensor_data result data (unit:dps)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_acquire_gyro(sensor_imu_handle_t sensor, sensor_data_t *sensor_data);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor imu sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_sleep(sensor_imu_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor imu sensor handle to operate
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t imu_wakeup(sensor_imu_handle_t sensor);

esp_err_t imu_acquire(sensor_imu_handle_t sensor, sensor_data_group_t *data_group);

esp_err_t imu_control(sensor_imu_handle_t sensor, sensor_command_t cmd, void *args);

#ifdef __cplusplus
extern "C"
}
#endif


#endif
