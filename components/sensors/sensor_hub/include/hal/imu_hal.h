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

#ifndef _IMU_HAL_H_
#define _IMU_HAL_H_

#include "i2c_bus.h"
#include "esp_err.h"
#include "sensor_type.h"

typedef void *sensor_imu_handle_t; /*!< imu sensor handle*/

/**
 * @brief imu sensor id, used for imu_create
 * 
 */
typedef enum {
    MPU6050_ID = 0x01, /*!< MPU6050 imu sensor id*/
    LIS2DH12_ID, /*!< LIS2DH12 imu sensor id*/
    IMU_MAX_ID, /*!< max imu sensor id*/
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
extern "C"
}
#endif


#endif
