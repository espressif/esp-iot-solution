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
#ifndef _IOT_AT24C02_H_
#define _IOT_AT24C02_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"

#define AT24C02_I2C_ADDRESS_DEFAULT   (0x50)    //1 0  1  0   A2  A1  A0  R/W

#define WRITE_BIT      I2C_MASTER_WRITE         /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ          /*!< I2C master read */
#define ACK_CHECK_EN   0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0                      /*!< I2C ack value */
#define NACK_VAL       0x1                      /*!< I2C nack value */

typedef void* at24c02_handle_t;                 /*handle of at24c02*/

/**
 * @brief   Create AT24C02 handle_t
 *
 * @param   dev object handle of I2C
 * @param   decice address
 *
 * @return
 *     - at24c02_handle_t
 */
at24c02_handle_t iot_at24c02_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief   delete AT24C02 handle_t
 *
 * @param   dev object handle of AT24C02
 * @param   whether delete i2c bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_at24c02_delete(at24c02_handle_t dev, bool del_bus);

/**
 * @brief   Write a data on addr
 *
 * @param   dev object handle of at24c02
 * @param   The address of the data to be written
 * @param   the data to be written
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_at24c02_write_byte(at24c02_handle_t dev, uint8_t addr,
        uint8_t data);

/**
 * @brief   Read a data on addr
 *
 * @param   dev object handle of at24c02
 * @param   The address of the data to be read
 * @param   Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_at24c02_read_byte(at24c02_handle_t dev, uint8_t addr,
        uint8_t *data);

/**
 * @brief   Write some data start addr
 *
 * @param   dev object handle of at24c02
 * @param   The address of the data to be written
 * @param   The size of the data to be written
 * @param   Pointer to write data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_at24c02_write(at24c02_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf);

/**
 * @brief   Read some data start addr
 *
 * @param   dev object handle of at24c02
 * @param   The address of the data to be read
 * @param   The size of the data to be read
 * @param   Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_at24c02_read(at24c02_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of AT24C02 dev
 * simple usage:
 * CAT24C02 *AT24C02 = new CAT24C02(&i2c_bus);
 * AT24C02.read();
 * ......
 * delete(AT24C02);
 */
class CAT24C02
{
private:
    at24c02_handle_t m_dev_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CAT24C02(const CAT24C02&);
    CAT24C02& operator =(const CAT24C02&);
public:
    /**
     * @brief   constructor of CAT24C02
     *
     * @param   p_i2c_bus pointer to CI2CBus object
     * @param   addr of device address
     */
    CAT24C02(CI2CBus *p_i2c_bus, uint8_t addr = AT24C02_I2C_ADDRESS_DEFAULT);

    ~CAT24C02();
    /**
     * @brief   Write a data on addr
     *
     * @param   The address of the data to be written
     * @param   the data to be written
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t write_byte(uint8_t addr, uint8_t data);

    /**
     * @brief   Read a data on addr
     *
     * @param   The address of the data to be read
     * @param   Pointer to read data
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t read_byte(uint8_t addr, uint8_t *data);


    /**
     * @brief   Write some data start addr
     *
     * @param   The address of the data to be written
     * @param   The size of the data to be written
     * @param   Pointer to write data
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t write(uint8_t start_addr, uint8_t write_num,
            uint8_t *data_buf);

    /**
     * @brief   Read some data start addr
     *
     * @param   The address of the data to be read
     * @param   The size of the data to be read
     * @param   Pointer to read data
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t read(uint8_t start_addr, uint8_t read_num,
            uint8_t *data_buf);
};
#endif

#endif

