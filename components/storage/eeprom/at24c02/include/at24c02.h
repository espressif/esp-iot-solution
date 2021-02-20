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
#ifndef _AT24C02_H_
#define _AT24C02_H_
#include "driver/i2c.h"
#include "i2c_bus.h"

#define AT24C02_I2C_ADDRESS_DEFAULT   (0x50)    //1 0 1 0 A2 A1 A0 R/W

typedef void *at24c02_handle_t;                 /*handle of at24c02*/

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @brief Create AT24C02 handle_t
 *
 * @param bus object handle of I2C
 * @param dev_addr device address
 *
 * @return
 *     - at24c02_handle_t
 */
at24c02_handle_t at24c02_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief delete AT24C02 handle_t
 *
 * @param device point to object handle of AT24C02
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t at24c02_delete(at24c02_handle_t *device);

/**
 * @brief   Write a byte on addr
 *
 * @param device object handle of at24c02
 * @param addr address of the data to be written
 * @param data data to be written
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t at24c02_write_byte(at24c02_handle_t device, uint8_t addr, uint8_t data);

/**
 * @brief Read a byte on addr
 *
 * @param device object handle of at24c02
 * @param addr address of the data to be read
 * @param data point to the buf will save read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t at24c02_read_byte(at24c02_handle_t device, uint8_t addr, uint8_t *data);

/**
 * @brief Write multiple bytes from data_buf to start addr
 *
 * @param device object handle of at24c02
 * @param start_addr address of the data to be written
 * @param write_num size of the data to be written
 * @param data_buf pointer to the buf of data will be written
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t at24c02_write(at24c02_handle_t device, uint8_t start_addr, uint8_t write_num, uint8_t *data_buf);

/**
 * @brief Read multiple bytes from start addr to data_buf
 *
 * @param device object handle of at24c02
 * @param start_addr address of the data to be read
 * @param read_num size of the data to be read
 * @param data_buf pointer to the buf will save read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t at24c02_read(at24c02_handle_t device, uint8_t start_addr, uint8_t read_num, uint8_t *data_buf);

/**
 * @brief Write one bit of a byte to at24c02 with specified address
 *
 * @param device object handle of at24c02
 * @param addr The internal reg/mem address to write to.
 * @param bit_num The bit number 0 - 7 to write
 * @param data The bit to write.
 * @return esp_err_t
 */
esp_err_t at24c02_write_bit(at24c02_handle_t device, uint8_t addr, uint8_t bit_num, uint8_t data);

/**
 * @brief Write specified bits of a byte to at24c02 with specified address
 *
 * @param device object handle of at24c02
 * @param addr The internal reg/mem address to write to.
 * @param bit_start The bit to start from, 0 - 7, MSB at 0
 * @param length The number of bits to write, 1 - 8
 * @param data The bits to write.
 * @return esp_err_t
 */
esp_err_t at24c02_write_bits(at24c02_handle_t device, uint8_t addr, uint8_t bit_start, uint8_t length, uint8_t data);

/**
 * @brief Read one bit of a byte from at24c02 with specified address
 *
 * @param device object handle of at24c02
 * @param addr The internal reg/mem address to read from.
 * @param bit_num The bit number 0 - 7 to read
 * @param data Pointer to a buffer to save the read data to.
 * @return esp_err_t
 */
esp_err_t at24c02_read_bit(at24c02_handle_t device, uint8_t addr, uint8_t bit_num, uint8_t *data);

/**
 * @brief Read specified bits of a byte from at24c02 with specified address
 *
 * @param device object handle of at24c02
 * @param addr The internal reg/mem address to read from.
 * @param bit_start The bit to start from, 0 - 7, MSB at 0
 * @param length The number of bits to read, 1 - 8
 * @param data Pointer to a buffer to save the read data to
 * @return esp_err_t
 */
esp_err_t at24c02_read_bits(at24c02_handle_t device, uint8_t addr, uint8_t bit_start, uint8_t length, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif
