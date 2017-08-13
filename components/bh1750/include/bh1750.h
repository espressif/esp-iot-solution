/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#ifndef _IOT_BH1750_H_
#define _IOT_BH1750_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "i2c_bus.h"
typedef enum{
    BH1750_CONTINUE_1LX_RES       =0x10,    /*!< Command to set measure mode as Continuously H-Resolution mode*/
    BH1750_CONTINUE_HALFLX_RES    =0x11,    /*!< Command to set measure mode as Continuously H-Resolution mode2*/
    BH1750_CONTINUE_4LX_RES       =0x13,    /*!< Command to set measure mode as Continuously L-Resolution mode*/
    BH1750_ONETIME_1LX_RES        =0x20,    /*!< Command to set measure mode as One Time H-Resolution mode*/
    BH1750_ONETIME_HALFLX_RES     =0x21,    /*!< Command to set measure mode as One Time H-Resolution mode2*/
    BH1750_ONETIME_4LX_RES        =0x23,    /*!< Command to set measure mode as One Time L-Resolution mode*/
}bh1750_cmd_measure_t;

typedef void* bh1750_handle_t;

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
esp_err_t bh1750_get_data(bh1750_handle_t sensor, float* data);

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
esp_err_t bh1750_get_light_intensity(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure, float* data);

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

/**
 * @brief Create and init sensor object and return a sensor handle
 *
 * @param bus I2C bus object handle
 * @param dev_addr I2C device address of sensor
 * @param addr_10bit_en To enable 10bit address
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
bh1750_handle_t sensor_bh1750_create(i2c_bus_handle_t bus, uint16_t dev_addr, bool addr_10bit_en);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor object handle of bh1750
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sensor_bh1750_delete(bh1750_handle_t sensor, bool del_bus);

#ifdef __cplusplus
}
#endif

#endif

