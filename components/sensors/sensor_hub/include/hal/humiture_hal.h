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

#ifndef _HUMITURE_HAL_H_
#define _HUMITURE_HAL_H_

#include "i2c_bus.h"
#include "sensor_type.h"

typedef void *sensor_humiture_handle_t; /*!< humiture sensor handle*/

/**
 * @brief humiture sensor id, used for humiture_create
 * 
 */
typedef enum {
    SHT3X_ID = 0x01, /*!< sht3x humiture sensor id*/
    HTS221_ID, /*!< hts221 humiture sensor id*/
    HUMITURE_MAX_ID, /*!< max humiture sensor id*/
} humiture_id_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a humiture/temperature/humidity sensor instance.
 * Same series' sensor or sensor with same address can only be created once.
 *
 * @param bus i2c bus handle the sensor attached to
 * @param id id declared in humiture_id_t
 * @return sensor_humiture_handle_t return humiture sensor handle if succeed, return NULL if create failed.
 */
sensor_humiture_handle_t humiture_create(bus_handle_t bus, int id);

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
extern "C"
}
#endif


#endif
