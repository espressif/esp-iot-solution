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
#ifndef _IOT_MAG3110_H_
#define _IOT_MAG3110_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"



#define MAG3110_I2C_ADDRESS         0x0E    /*!< slave address for mag3110 sensor */

/* MAG3110 register */
#define MAG3110_DR_STATUS			0x00	/*Default value is 0x00 */
#define MAG3110_OUT_X_MSB			0x01
#define MAG3110_OUT_X_LSB			0x02
#define MAG3110_OUT_Y_MSB			0x03
#define MAG3110_OUT_Y_LSB			0x04
#define MAG3110_OUT_Z_MSB			0x05
#define MAG3110_OUT_Z_LSB			0x06
#define MAG3110_WHO_AM_I			0x07
#define MAG3110_SYSMOD				0x08
#define MAG3110_OFF_X_MSB			0x09
#define MAG3110_OFF_X_LSB			0x0A
#define MAG3110_OFF_Y_MSB			0x0B
#define MAG3110_OFF_Y_LSB			0x0C
#define MAG3110_OFF_Z_MSB			0x0D
#define MAG3110_OFF_Z_LSB			0x0E
#define MAG3110_DIE_TEMP			0x0F
#define MAG3110_CTRL_REG1			0x10
#define MAG3110_CTRL_REG2			0x11

typedef struct {
    int16_t magno_x;
    int16_t magno_y;
    int16_t magno_z;
} mag3110_raw_values_t;

typedef enum {
    stand_by_mode = 0x00,   			  /*!< Accelerometer full scale range is +/- 2g */
    active_raw_mode = 0x01,   				  /*!< Accelerometer full scale range is +/- 4g */
} mag3110_mode_sel_t;


typedef void* mag3110_handle_t;

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
mag3110_handle_t iot_mag3110_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief Read value from register of mag3110
 *
 * @param sensor object handle of mag3110
 * @param reg_addr register address
 * @param data register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_read_byte(mag3110_handle_t sensor, uint8_t reg, uint8_t *data);

/**
 * @brief Read value from multiple register of mag3110
 *
 * @param sensor object handle of mag3110
 * @param reg_addr start address of register
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_read(mag3110_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Write value to one register of mag3110
 *
 * @param sensor object handle of mag3110
 * @param reg_addr register address
 * @param data register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_write_byte(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t data);

/**
 * @brief Write value to multiple register of mag3110
 *
 * @param sensor object handle of mag3110
 * @param reg_addr start address of register
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_write(mag3110_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Wake up mag3110
 *
 * @param sensor object handle of mag3110
 * @ 0 for standby mode, 1 for active mode
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_mode_set(mag3110_handle_t sensor,mag3110_mode_sel_t mode_sel);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor object handle of mag3110
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_delete(mag3110_handle_t sensor, bool del_bus);

/**
 * @brief Get device identification of mag3110
 *
 * @param sensor object handle of mag3110
 * @param deviceid a pointer of device ID
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_deviceid(mag3110_handle_t sensor, uint8_t* deviceid);


/**
 * @brief Read raw magnetometer measurements
 *
 * @param sensor object handle of mag3110
 * @param raw magnetometer measurements
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_magneticfield(mag3110_handle_t sensor, mag3110_raw_values_t* magneticfield);


#ifdef __cplusplus
}
#endif

#endif

