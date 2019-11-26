// Ported over by Serena Yeoh (Nov 2019)
// for the ESP32 Azure IoT Kit by Espressif Systems (Shanghai) PTE LTD
//
// References:
//  Datasheet
//  http://www.fmti.com.tw/en/product_c_p_1
//
//  Original driver from Formosa (manufacturer)
//  https://github.com/formosa-measurement-technology-inc/FMTI_fbm320_driver
//
//  Sample implementation of drivers for the ESP IoT Solutions
//  https://github.com/espressif/esp-iot-solution/tree/master/components/i2c_devices/sensor
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
#ifndef _IOT_FBM320_H_
#define _IOT_FBM320_H_

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

#define FBM320_CHIP_ID          0x42
#define FBM320_I2C_ADDRESS     (0x6D)  /* Address of FBM320 on the Azure IoT Kit */
#define FBM320_TEMP_RESOLUTION  0.01

#define FBM320_TAKE_MEAS_REG	0xF4
#define FBM320_READ_MEAS_REG_U	0xF6
#define FBM320_READ_MEAS_REG_L	0xF7
#define FBM320_READ_MEAS_REG_XL	0xF8
#define FBM320_SOFTRESET_REG    0xE0
#define FBM320_CHIP_ID_REG	0x6B
#define FBM320_VERSION_REG	0xA5

#define FBM320_MEAS_TEMP                0x2E /* 2.5ms wait for measurement */
#define FBM320_MEAS_PRESS_OVERSAMP_0	0x34 /* 2.5ms wait for measurement */
#define FBM320_MEAS_PRESS_OVERSAMP_1	0x74 /* 3.7ms wait for measurement */
#define FBM320_MEAS_PRESS_OVERSAMP_2	0xB4 /* 6ms wait for measurement */
#define FBM320_MEAS_PRESS_OVERSAMP_3	0xF4 /* 10.7ms wait for measurement */
#define FBM320_SOFTRESET_CMD            0xB6

#define FBM320_OVERSAMPLING_RATE_DEFAULT  FBM320_OSR_8192

/* Calibration registers */
#define FBM320_CALIBRATION_DATA_START0	 0xAA /* Calibraton data address {0xF1, 0xD0, 0xBB:0xAA} */
#define FBM320_CALIBRATION_DATA_START1   0xD0
#define FBM320_CALIBRATION_DATA_START2   0xF1
#define FBM320_CALIBRATION_DATA_LENGTH	 20   /* bytes */

/* The wait times are longer than prescribed due to the slight inaccuracies in the 
 * CONFIG_FREERTOS_HZ and portTICK_RATE_MS.  */
#define FBM320_CONVERSION_usTIME_OSR1024   8    /*  2.5ms */  
#define FBM320_CONVERSION_usTIME_OSR2048   10   /*  3.7ms */  
#define FBM320_CONVERSION_usTIME_OSR4096   15   /*  6.0ms */  
#define FBM320_CONVERSION_usTIME_OSR8192   20   /* 10.7ms */ 
// #define FBM320_CONVERSION_usTIME_OSR16384  20.5  // Not ported over from manufacturer code.

/* Calibration data */
typedef struct {
	int32_t C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12, C13;	
} fbm320_calibration_data_t;

typedef enum  {
	FBM320_OSR_1024 = 0x0,
	FBM320_OSR_2048 = 0x1,
	FBM320_OSR_4096 = 0x2,
	FBM320_OSR_8192 = 0x3
	//FBM320_OSR_16384 = 0x4   // Not ported over from manufacturer code.
} fbm320_oversampling_rate;

typedef struct {
    float temperature;  /* Celcious */
	int32_t pressure;   /* Pascal (Pa) */
    int32_t altitude;   /* milimeters (mm)*/
} fbm320_values_t;

typedef void* fbm320_handle_t;

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
fbm320_handle_t iot_fbm320_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief Delete and release a sensor object
 *
 * @param sensor Object handle of fbm320
 * @param del_bus Whether to delete the I2C bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_delete(fbm320_handle_t sensor, bool del_bus);

/**
 * @brief Read value from register of fbm320
 *
 * @param sensor Object handle of fbm320
 * @param reg Register address
 * @param data Register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_read_byte(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t *data);

/**
 * @brief   Read values from one or multiple consecutive registers 
 *
 * @param   sensor Object handle of fbm320
 * @param   reg The address of the data to be read
 * @param   reg_num The number of registers to continue reading
 * @param   data_buf Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_read(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t reg_num, uint8_t* data_buf);

/**
 * @brief Write value to one register of fbm320
 *
 * @param sensor Object handle of fbm320
 * @param reg_addr Register address
 * @param data Register value
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_write_byte(fbm320_handle_t sensor, uint8_t reg_addr, uint8_t data);

/**
 * @brief Write value to multiple register of fbm320
 *
 * @param sensor Object handle of fbm320
 * @param reg_start_addr Start address of register
 * @param reg_num The size of the data to be written
 * @param data_buf Pointer of register values buffer
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_write(fbm320_handle_t sensor, uint8_t reg_start_addr, uint8_t reg_num, uint8_t *data_buf);

/**
 * @brief Get chip id of fbm320
 *
 * @param sensor Object handle of fbm320
 * @param data Pointer to Chip Id
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_get_chip_id(fbm320_handle_t sensor, uint8_t* chipid);

/**
 * @brief Get version id of fbm320
 *
 * @param sensor Object handle of fbm320
 * @param data Pointer to Version Id
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_get_version_id(fbm320_handle_t sensor, uint8_t* data);

/**
 * @brief Get uncompensated temperature value from fbm320
 *
 * @param sensor Object handle of fbm320
 * @param data Pointer to read data
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_get_raw_temperature(fbm320_handle_t sensor, uint32_t* data);

/**
 * @brief Get uncompensated pressure value from fbm320
 *
 * @param sensor Object handle of fbm320
 * @param data Pointer to read data
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_get_raw_pressure(fbm320_handle_t sensor, uint32_t* data);

/**
 * @brief Initializes the fbm320
 *
 * @param sensor Object handle of fbm320
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_init(fbm320_handle_t sensor);

/**
 * @brief Read the calibration saved in OTP memory from fbm320
 *
 * @param sensor Object handle of fbm320
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_read_calibration(fbm320_handle_t sensor);

/**
 * @brief Set the oversampling rate for fbm320
 *
 * @param sensor Object handle of fbm320
 * @param rate The oversampling rate.
 *
 * @return
 *     - void
 */
void iot_fbm320_set_oversampling_rate(fbm320_handle_t sensor, fbm320_oversampling_rate rate);

/**
 * @brief Calculate the real temperature and pressure values.
 *
 * @param sensor Object handle of fbm320
 * @param data_t Pointer to the calculated values
 *
 * @return
 *     - void
 */
void iot_fbm320_calculate(fbm320_handle_t sensor, fbm320_values_t* data_t);

/**
 * @brief Convert pressure to altitude. The standard sea-level pressure is 1013.25 hPa.
 *
 * @param sensor Object handle of fbm320
 * @param pressure The pressure value in 0.125 hPa
 *
 * @return
 *     - Altitude value in milimeter (mm)
 */
int32_t iot_fbm320_pressure_to_altitude(int32_t pressure);

/**
 * @brief Gets the updated sensor data.
 *
 * @param sensor Object handle of fbm320
 * @param data_t Pointer to stored values.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_fbm320_get_data(fbm320_handle_t sensor, fbm320_values_t* data_t);

#ifdef __cplusplus
}
#endif

#endif