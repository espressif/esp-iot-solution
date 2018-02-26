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
#ifndef _IOT_BH1750_H_
#define _IOT_BH1750_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"
typedef enum{
    BH1750_CONTINUE_1LX_RES       =0x10,    /*!< Command to set measure mode as Continuously H-Resolution mode*/
    BH1750_CONTINUE_HALFLX_RES    =0x11,    /*!< Command to set measure mode as Continuously H-Resolution mode2*/
    BH1750_CONTINUE_4LX_RES       =0x13,    /*!< Command to set measure mode as Continuously L-Resolution mode*/
    BH1750_ONETIME_1LX_RES        =0x20,    /*!< Command to set measure mode as One Time H-Resolution mode*/
    BH1750_ONETIME_HALFLX_RES     =0x21,    /*!< Command to set measure mode as One Time H-Resolution mode2*/
    BH1750_ONETIME_4LX_RES        =0x23,    /*!< Command to set measure mode as One Time L-Resolution mode*/
}bh1750_cmd_measure_t;

#define BH1750_I2C_ADDRESS_DEFAULT   (0x23)
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
esp_err_t iot_bh1750_power_down(bh1750_handle_t sensor);

/**
 * @brief Set bh1750 as power on mode
 *
 * @param sensor object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_bh1750_power_on(bh1750_handle_t sensor);

/**
 * @brief Reset data register of bh1750
 *
 * @param sensor object handle of bh1750
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_bh1750_reset_data_register(bh1750_handle_t sensor);

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
 *        You should call this funtion to set measurement mode before call iot_bh1750_get_data() to acquire data.
 *        If you set to onetime mode, you just can get one measurement data.
 *        If you set to continuously mode, you can call iot_bh1750_get_data() to acquire data repeatedly.
 */
esp_err_t iot_bh1750_set_measure_mode(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure);

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
esp_err_t iot_bh1750_get_data(bh1750_handle_t sensor, float* data);

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
esp_err_t iot_bh1750_get_light_intensity(bh1750_handle_t sensor, bh1750_cmd_measure_t cmd_measure, float* data);

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
esp_err_t iot_bh1750_change_measure_time(bh1750_handle_t sensor, uint8_t measure_time);

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
bh1750_handle_t iot_bh1750_create(i2c_bus_handle_t bus, uint16_t dev_addr);

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
esp_err_t iot_bh1750_delete(bh1750_handle_t sensor, bool del_bus);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of BH1750 light sensor
 * simple usage:
 * CBh1750 *bh1750 = new CBh1750(&i2c_bus);
 * bh1750.read();
 * ......
 * delete(bh1750);
 */
class CBh1750
{
private:
    bh1750_handle_t m_sensor_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CBh1750(const CBh1750&);
    CBh1750& operator = (const CBh1750&);
public:
    /**
     * @brief constructor of CBh1750
     *
     * @param p_i2c_bus pointer to CI2CBus object
     * @param addr slave device address
     */
    CBh1750(CI2CBus *p_i2c_bus, uint8_t addr = BH1750_I2C_ADDRESS_DEFAULT);

    ~CBh1750();
    /**
     * @brief read brightness
     * @return brightness value
     */
    float read();

    /**
     * @brief turn on the sensor
     * @return
     *     - ESP_OK Success
     *     - ESP_ERR_INVALID_ARG Parameter error
     *     - ESP_FAIL Sending command error, slave doesn't ACK the transfer.
     *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
     *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
     */
    esp_err_t on();

    /**
     * @brief turn off the sensor
     * @return
     *     - ESP_OK Success
     *     - ESP_ERR_INVALID_ARG Parameter error
     *     - ESP_FAIL Sending command error, slave doesn't ACK the transfer.
     *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
     *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.     */
    esp_err_t off();

    /**
     * @brief set test mode for sensor
     * @return
     *     - ESP_OK Success
     *     - ESP_ERR_INVALID_ARG Parameter error
     *     - ESP_FAIL Sending command error, slave doesn't ACK the transfer.
     *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
     *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
     */
    esp_err_t set_mode(bh1750_cmd_measure_t cmd_measure);
};
#endif

#endif

