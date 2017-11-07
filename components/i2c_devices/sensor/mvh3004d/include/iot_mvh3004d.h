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

#ifndef _IOT_mvh3004d_H_
#define _IOT_mvh3004d_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"

typedef void* mvh3004d_handle_t;
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
mvh3004d_handle_t iot_mvh3004d_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor object handle of mvh3004d
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mvh3004d_delete(mvh3004d_handle_t sensor, bool del_bus);

/**
 * @brief read temperature and huminity
 * @param sensor object handle of mvh3004d
 * @tp pointer to accept temperature
 * @rh pointer to accept huminity
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mvh3004d_get_data(mvh3004d_handle_t sensor, float* tp, float* rh);

/**
 * @brief read huminity
 * @param sensor object handle of mvh3004d
 * @rh pointer to accept huminity
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mvh3004d_get_huminity(mvh3004d_handle_t sensor, float* rh);

/**
 * @brief read temperature
 * @param sensor object handle of mvh3004d
 * @tp pointer to accept temperature
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mvh3004d_get_temperature(mvh3004d_handle_t sensor, float* tp);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of mvh3004d temperature and humidity sensor
 */
class CMvh3004d
{
private:
    mvh3004d_handle_t m_sensor_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CMvh3004d(const CMvh3004d&);
    CMvh3004d& operator = (const CMvh3004d&);
public:
    /**
     * @brief Constructor for CMvh3004d class
     * @param p_i2c_bus pointer to CI2CBus object
     * @param addr slave device address
     */
    CMvh3004d(CI2CBus *p_i2c_bus, uint8_t addr = MVH3004D_SLAVE_ADDR);

    /**
     * @brief Destructor function for CMvh3004d class
     */
    ~CMvh3004d();

    /**
     * @brief read temperature
     * @return temperature value
     */
    float read_temperature();

    /**
     * @brief read humidity
     * @return humidity value
     */
    float read_humidity();
};
#endif

#endif

