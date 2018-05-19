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
#ifndef _IOT_I2C_BUS_H_
#define _IOT_I2C_BUS_H_
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C"
{
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
i2c_bus_handle_t iot_i2c_bus_create(i2c_port_t port, i2c_config_t* conf);

/**
 * @brief Delete and release the I2C bus object
 *
 * @param bus I2C bus handle
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_i2c_bus_delete(i2c_bus_handle_t bus);

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
esp_err_t iot_i2c_bus_cmd_begin(i2c_bus_handle_t bus, i2c_cmd_handle_t cmd,
portBASE_TYPE ticks_to_wait);
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
    CI2CBus& operator =(const CI2CBus&);
public:
    /**
     * @brief Constructor for CI2CBus class
     * @param i2c_port I2C hardware port
     * @param scl_io gpio index for slc pin
     * @param sda_io gpio index for sda pin
     * @param clk_hz I2C clock frequency
     * @param i2c_mode mode for I2C bus
     *
     */
    CI2CBus(i2c_port_t i2c_port, gpio_num_t scl_io, gpio_num_t sda_io,
            int clk_hz = 100000, i2c_mode_t i2c_mode = I2C_MODE_MASTER);

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

