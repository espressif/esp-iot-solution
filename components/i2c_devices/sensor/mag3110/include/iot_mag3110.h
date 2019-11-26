// Written by Serena Yeoh (Nov 2019)
// for the ESP32 Azure IoT Kit by Espressif Systems (Shanghai) PTE LTD
//
// References:
//  Datasheet
//  https://www.nxp.com/docs/en/data-sheet/MAG3110.pdf
//
//  Sample implementation of drivers for the ESP IoT Solutions
//  https://github.com/espressif/esp-iot-solution/tree/master/components/i2c_devices/sensor
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
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

#define WRITE_BIT       I2C_MASTER_WRITE    /*!< I2C master write */
#define READ_BIT        I2C_MASTER_READ     /*!< I2C master read */
#define ACK_CHECK_EN    0x1                 /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS   0x0                 /*!< I2C master will not check ack from slave */
#define ACK_VAL         0x0                 /*!< I2C ack value */
#define NACK_VAL        0x1                 /*!< I2C nack value */

#define MAG3110_CHIP_ID         0xC4
#define MAG3110_I2C_ADDRESS    (0x0E)   /* Address of MAG3110 on the Azure IoT Kit */  

/* Registers based on datasheet */
#define MAG3110_DR_STATUS_REG	0x00	
#define MAG3110_OUT_X_MSB_REG	0x01
#define MAG3110_OUT_X_LSB_REG	0x02
#define MAG3110_OUT_Y_MSB_REG	0x03
#define MAG3110_OUT_Y_LSB_REG	0x04
#define MAG3110_OUT_Z_MSB_REG	0x05
#define MAG3110_OUT_Z_LSB_REG	0x06
#define MAG3110_WHO_AM_I_REG	0x07    // Chip ID
#define MAG3110_SYSMOD_REG	0x08
#define MAG3110_OFF_X_MSB_REG	0x09
#define MAG3110_OFF_X_LSB_REG	0x0A
#define MAG3110_OFF_Y_MSB_REG	0x0B
#define MAG3110_OFF_Y_LSB_REG	0x0C
#define MAG3110_OFF_Z_MSB_REG	0x0D
#define MAG3110_OFF_Z_LSB_REG	0x0E
#define MAG3110_DIE_TEMP_REG	0x0F
#define MAG3110_CTRL_REG1	0x10
#define MAG3110_CTRL_REG2	0x11

/* Data ready statuses */
#define MAG3110_DR_STATUS_ZYXOW     (1 << 7) /* X, Y, Z Data Overwrite */
#define MAG3110_DR_STATUS_ZOW       (1 << 6) /* Z-axis Data Overwrite */
#define MAG3110_DR_STATUS_YOW       (1 << 5) /* Y-axis Data Overwrite */
#define MAG3110_DR_STATUS_XOW       (1 << 4) /* X-axis Data Overwrite */
#define MAG3110_DR_STATUS_ZYXDR     (1 << 3) /* X, Y, Z new Data Ready */
#define MAG3110_DR_STATUS_ZDR       (1 << 2) /* Z-axis new Data Available */
#define MAG3110_DR_STATUS_YDR       (1 << 1) /* Y-axis new Data Available */
#define MAG3110_DR_STATUS_XDR       (1 << 0) /* X-axis new Data Available */

#define MAG3110_CTRL_REG1_FR    	  BIT2  // Fast Read - not implemented 
#define MAG3110_CTRL_REG1_TM    	  BIT1  // Trigger Measurement - not implemented 
#define MAG3110_CTRL_REG1_AC   		  BIT0

#define MAG3110_CTRL_REG2_AUTO_MRST_EN    BIT7
#define MAG3110_CTRL_REG2_RAW             BIT5
#define MAG3110_CTRL_REG2_MAG_RST         BIT4  // Magnetic Sensor Reset - not implemented 

#define MAG3110_DR_OSR_DEFAULT    MAG3110_DR_OSR_8000_16; 

/* Datasheet Notes:
 * The register contains the die temperature in °C expressed as an 8-bit 2's 
 * complement number. The sensitivity of the temperature sensor is factory trimmed 
 * to 1°C/LSB. The temperature sensor offset is not factory trimmed and must be 
 * calibrated by the user software if higher absolute accuracy is required. 
 * Note: The register allows for temperature measurements from -128°C to 127°C 
 * but the output range is limited to -40°C to 125°C
 */
#define MAG3110_DIE_TEMP_OFFSET	12    

typedef struct {
    int16_t X;
    int16_t Y;
    int16_t Z;
} mag3110_values_t;

typedef enum {
    MAG3110_MODE_STANDBY    = 0x00,   			  
    MAG3110_MODE_ACTIVE     = 0x01  				  
} mag3110_operating_modes;

/* Over-Sampling Ratio and Data Rate from datasheet */
typedef enum {
    MAG3110_DR_OSR_8000_16  = 0,	/* Output Rate 80.00 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_4000_32  = 1,	/* Output Rate 40.00 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_2000_64  = 2,	/* Output Rate 20.00 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_1000_128 = 3,	/* Output Rate 10.00 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_4000_16  = 4,	/* Output Rate 40.00 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_2000_32  = 5,	/* Output Rate 20.00 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_1000_64  = 6,	/* Output Rate 10.00 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0500_128 = 7,	/* Output Rate  5.00 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_2000_16  = 8,	/* Output Rate 20.00 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_1000_32  = 9,	/* Output Rate 10.00 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0500_64  = 10,	/* Output Rate  5.00 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0250_128 = 11,	/* Output Rate  2.50 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_1000_16  = 12,	/* Output Rate 10.00 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_0500_32  = 13,	/* Output Rate  5.00 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0250_64  = 14,	/* Output Rate  2.50 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0125_128 = 15,	/* Output Rate  1.25 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_0500_16  = 16,	/* Output Rate  5.00 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_0250_32  = 17,	/* Output Rate  2.50 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0125_64  = 18,	/* Output Rate  1.25 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0063_128 = 19,	/* Output Rate  0.63 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_0250_16  = 20,	/* Output Rate  2.50 Hz, Over Sample Ratio 16  */
    MAG3110_DR_OSR_0125_32  = 21,	/* Output Rate  1.25 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0063_64  = 22,	/* Output Rate  0.63 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0031_128 = 23,	/* Output Rate  0.31 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_0125_16  = 24,	/* Output Rate  1.25 Hz, Over Sample Ratio 16  */ 
    MAG3110_DR_OSR_0063_32  = 25,	/* Output Rate  0.63 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0031_64  = 26,	/* Output Rate  0.31 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0016_128 = 27,	/* Output Rate  0.16 Hz, Over Sample Ratio 128 */
    MAG3110_DR_OSR_0063_16  = 28,	/* Output Rate  0.63 Hz, Over Sample Ratio 16  */ 
    MAG3110_DR_OSR_0031_32  = 29,	/* Output Rate  0.31 Hz, Over Sample Ratio 32  */
    MAG3110_DR_OSR_0016_64  = 30,	/* Output Rate  0.16 Hz, Over Sample Ratio 64  */
    MAG3110_DR_OSR_0008_128 = 31	/* Output Rate  0.08 Hz, Over Sample Ratio 128 */
} mag3110_oversampling_data_rate;

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
 * @brief Delete and release a sensor object
 *
 * @param sensor Object handle of mag3110
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_delete(mag3110_handle_t sensor, bool del_bus);

/**
 * @brief Read value from register of mag3110
 *
 * @param sensor Object handle of mag3110
 * @param reg  Register address
 * @param data Pointer to register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_read_byte(mag3110_handle_t sensor, uint8_t reg, uint8_t *data);

/**
 * @brief   Read values from one or multiple consecutive registers 
 *
 * @param   sensor Object handle of mag3110
 * @param   reg The address of the data to be read
 * @param   reg_num The number of registers to continue reading
 * @param   data_buf Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_read(mag3110_handle_t sensor, uint8_t reg, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Write value to one register of mag3110
 *
 * @param sensor Object handle of mag3110
 * @param reg_addr Register address
 * @param data Register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_write_byte(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t data);

/**
 * @brief Write value to one or multiple registers of mag3110
 *
 * @param sensor Object handle of mag3110
 * @param reg_start_addr Start address of register
 * @param reg_num The size of the data to be written
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_write(mag3110_handle_t sensor, uint8_t reg_addr, uint8_t reg_num, uint8_t* data_buf);

/**
 * @brief Get device identification of mag3110
 *
 * @param sensor Object handle of mag3110
 * @param chipid A pointer to chip Id
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_chip_id(mag3110_handle_t sensor, uint8_t* chipid);

/**
 * @brief Get the die temperature of mag3110. Please configure your own offset.
 *
 * @param sensor Object handle of mag3110
 * @param temperature A pointer to the die temperature
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_die_temperature(mag3110_handle_t sensor, int8_t* temperature);

/**
 * @brief Set the operating mode of the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param mode A value from the mag3110_operating_modes enum
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_set_mode(mag3110_handle_t sensor, mag3110_operating_modes mode);


/**
 * @brief Indicates whether new data is ready for mag3110
 *
 * @param sensor Object handle of mag3110
 * 
 * @return
 *     - true new data is ready
 *     - false data not ready
 */
bool iot_mag3110_is_data_ready(mag3110_handle_t sensor);

/**
 * @brief Initializes the mag3110
 *
 * @param sensor Object handle of mag3110
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_init(mag3110_handle_t sensor);


/**
 * @brief Gets the user offsets stored in the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param values_t A Pointer to a struct containing the X, Y, Z values
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_user_offsets(mag3110_handle_t sensor, mag3110_values_t* values_t);

/**
 * @brief Sets the user offsets into the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param x - The X value.
 * @param y - The Y value.
 * @param z - The Z value.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_set_user_offsets(mag3110_handle_t sensor, int16_t x, int16_t y, int16_t z);

/**
 * @brief Sets the Over-Sampling Ratio and Data Rate of the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param dr_osr A value from the mag3110_oversampling_data_rate (between 0 - 31)
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_set_DR_OSR(mag3110_handle_t sensor, uint8_t dr_osr);

/**
 * @brief Enables/Disables the raw mode of the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param enabled The desired state for RAW
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_enable_raw_mode(mag3110_handle_t sensor, bool enabled);

/**
 * @brief Enables/Disables the automatic magnetic sensor reset mode of the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param enabled The desired state for AUTO_MRST_EN
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_enable_auto_reset(mag3110_handle_t sensor, bool enabled);

/**
 * @brief Gets the magneticfield data from the mag3110
 *
 * @param sensor Object handle of mag3110
 * @param enabled A Pointer to a struct containing the X, Y, Z values.
 * 
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_mag3110_get_data(mag3110_handle_t sensor, mag3110_values_t* values_t);


#ifdef __cplusplus
}
#endif

#endif
