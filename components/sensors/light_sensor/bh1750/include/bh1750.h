// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _BH1750_H_
#define _BH1750_H_
#include "esp_err.h"
#include "i2c_bus.h"

typedef enum {
    BH1750_CONTINUE_1LX_RES       = 0x10,   /*!< Command to set measure mode as Continuously H-Resolution mode*/
    BH1750_CONTINUE_HALFLX_RES    = 0x11,   /*!< Command to set measure mode as Continuously H-Resolution mode2*/
    BH1750_CONTINUE_4LX_RES       = 0x13,   /*!< Command to set measure mode as Continuously L-Resolution mode*/
    BH1750_ONETIME_1LX_RES        = 0x20,   /*!< Command to set measure mode as One Time H-Resolution mode*/
    BH1750_ONETIME_HALFLX_RES     = 0x21,   /*!< Command to set measure mode as One Time H-Resolution mode2*/
    BH1750_ONETIME_4LX_RES        = 0x23,   /*!< Command to set measure mode as One Time L-Resolution mode*/
} bh1750_cmd_measure_t;

#define BH1750_I2C_ADDRESS_DEFAULT   (0x23)
typedef void *bh1750_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

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
bh1750_handle_t bh1750_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor Point to object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_delete(bh1750_handle_t *sensor);

/**
 * @brief Set bh1750 as power down mode (low current)
 *
 * @param sensor object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_power_down(bh1750_handle_t sensor);

/**
 * @brief Set bh1750 as power on mode
 *
 * @param sensor object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_power_on(bh1750_handle_t sensor);

/**
 * @brief Reset data register of bh1750
 *
 * @param sensor object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_reset_data_register(bh1750_handle_t sensor);

/**
 * @brief Get light intensity from bh1750
 *
 * @param sensor object handle of bh1750
 * @param cmd_measure the instruction to set measurement mode
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 * @note
 *        You should call this funtion to set measurement mode before call bh1750_get_data() to acquire data.
 *        If you set to onetime mode, you just can get one measurement data.
 *        If you set to continuously mode, you can call bh1750_get_data() to acquire data repeatedly.
 */
esp_err_t bh1750_set_measure_mode(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure);

/**
 * @brief Get light intensity from bh1750
 *
 * @param sensor object handle of bh1750
 * @param data light intensity value got from bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 * @note
 *        You should get data after measurement time over, so take care of measurement time in different mode.
 */
esp_err_t bh1750_get_data(bh1750_handle_t sensor, float *data);

/**
 * @brief Get light intensity from bh1750
 *
 * @param sensor object handle of bh1750
 * @param cmd_measure the instruction to set measurement mode
 * @param data light intensity value got from bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_get_light_intensity(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure, float *data);

/**
 * @brief Change measurement time
 *
 * @param sensor object handle of bh1750
 * @param measure_time measurement time
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bh1750_change_measure_time(bh1750_handle_t sensor, uint8_t measure_time);

/**implements of light sensor hal interface**/
#ifdef CONFIG_SENSOR_LIGHT_INCLUDED_BH1750
/**
 * @brief initialize bh1750 with default configurations
 * 
 * @param i2c_bus i2c bus handle the sensor will attached to
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_bh1750_init(i2c_bus_handle_t handle);

/**
 * @brief de-initialize bh1750
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_bh1750_deinit(void);

/**
 * @brief test if bh1750 is active
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_bh1750_test(void);

/**
 * @brief acquire bh1750 light illuminance result one time.
 * 
 * @param l result data (unit:lux)
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_bh1750_acquire_light(float *l);

#endif

#ifdef __cplusplus
}
#endif

#endif
