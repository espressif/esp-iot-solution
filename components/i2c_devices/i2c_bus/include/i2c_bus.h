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
#ifndef _IOT_I2C_BUS_H_
#define _IOT_I2C_BUS_H_
#include "driver/i2c.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef void* i2c_bus_handle_t;

/**
 * @brief Create and init I2C bus and return a I2C bus handle
 *
 * @param port I2C port number
 * @param conf Pointer to I2C parameters
 *
 * @return
 *     - NULL Fail
 *     - Others Success
 */
i2c_bus_handle_t i2c_bus_create(i2c_port_t port, i2c_config_t* conf);

/**
 * @brief Delete and release the I2C bus object
 *
 * @param bus I2C bus handle
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t i2c_bus_delete(i2c_bus_handle_t bus);

/**
 * @brief I2C start sending buffered commands
 *
 * @param bus I2C bus handle
 * @param cmd I2C cmd handle
 * @param ticks_to_wait Maximum blocking time
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t i2c_bus_cmd_begin(i2c_bus_handle_t bus, i2c_cmd_handle_t cmd, portBASE_TYPE ticks_to_wait);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
/**
 * class of I2c bus
 */
class CI2CBus
{
private:
    i2c_bus_handle_t m_i2c_bus_handle;

    /**
     * prevent copy constructing
     */
    CI2CBus(const CI2CBus&);
    CI2CBus& operator = (const CI2CBus&);
public:
    /**
     * @brief Constructor for CI2CBus class
     * @param i2c_port I2C hardware port
     * @param scl_io gpio index for slc pin
     * @param sda_io gpio index for sda pin
     * @param i2c_mode mode for I2C bus
     * @param clk_hz I2C clock frequency
     *
     */
    CI2CBus(i2c_port_t i2c_port, gpio_num_t scl_io, gpio_num_t sda_io, i2c_mode_t i2c_mode = I2C_MODE_MASTER, int clk_hz = 100000);

    /**
     * @brief Destructor function of CI2CBus class
     */
    ~CI2CBus();

    /**
     * @brief Send command and data to I2C bus
     * @param cmd pointor to command link
     * @ticks_to_wait max block time
     * @return
     *     - ESP_OK Success
     *     - ESP_ERR_INVALID_ARG Parameter error
     *     - ESP_FAIL Sending command error, slave doesn't ACK the transfer.
     *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
     *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
     */
    esp_err_t send(i2c_cmd_handle_t cmd, portBASE_TYPE ticks_to_wait);

    /**
     * @brief Get bus handle
     * @return bus handle
     */
    i2c_bus_handle_t get_bus_handle();
};
#endif


#endif

