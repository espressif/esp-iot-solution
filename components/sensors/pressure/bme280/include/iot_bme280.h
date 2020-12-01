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
#ifndef _IOT_BME280_H_
#define _IOT_BME280_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/i2c.h"
#include "iot_i2c_bus.h"

#define BME280_I2C_ADDRESS_DEFAULT   (0x76)     /*The device's I2C address is either 0x76 or 0x77.*/
#define BME280_DEFAULT_CHIPID        (0x60)

#define WRITE_BIT      I2C_MASTER_WRITE         /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ          /*!< I2C master read */
#define ACK_CHECK_EN   0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0                      /*!< I2C ack value */
#define NACK_VAL       0x1                      /*!< I2C nack value */

#define BME280_REGISTER_DIG_T1              0x88
#define BME280_REGISTER_DIG_T2              0x8A
#define BME280_REGISTER_DIG_T3              0x8C

#define BME280_REGISTER_DIG_P1              0x8E
#define BME280_REGISTER_DIG_P2              0x90
#define BME280_REGISTER_DIG_P3              0x92
#define BME280_REGISTER_DIG_P4              0x94
#define BME280_REGISTER_DIG_P5              0x96
#define BME280_REGISTER_DIG_P6              0x98
#define BME280_REGISTER_DIG_P7              0x9A
#define BME280_REGISTER_DIG_P8              0x9C
#define BME280_REGISTER_DIG_P9              0x9E

#define BME280_REGISTER_DIG_H1              0xA1
#define BME280_REGISTER_DIG_H2              0xE1
#define BME280_REGISTER_DIG_H3              0xE3
#define BME280_REGISTER_DIG_H4              0xE4
#define BME280_REGISTER_DIG_H5              0xE5
#define BME280_REGISTER_DIG_H6              0xE7

#define BME280_REGISTER_CHIPID              0xD0
#define BME280_REGISTER_VERSION             0xD1
#define BME280_REGISTER_SOFTRESET           0xE0

#define BME280_REGISTER_CAL26               0xE1  // R calibration stored in 0xE1-0xF0

#define BME280_REGISTER_CONTROLHUMID        0xF2
#define BME280_REGISTER_STATUS              0XF3
#define BME280_REGISTER_CONTROL             0xF4
#define BME280_REGISTER_CONFIG              0xF5
#define BME280_REGISTER_PRESSUREDATA        0xF7
#define BME280_REGISTER_TEMPDATA            0xFA
#define BME280_REGISTER_HUMIDDATA           0xFD

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;

    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
} bme280_data_t;

typedef enum {
    BME280_SAMPLING_NONE = 0b000,
    BME280_SAMPLING_X1 = 0b001,
    BME280_SAMPLING_X2 = 0b010,
    BME280_SAMPLING_X4 = 0b011,
    BME280_SAMPLING_X8 = 0b100,
    BME280_SAMPLING_X16 = 0b101
} bme280_sensor_sampling;

typedef enum {
    BME280_MODE_SLEEP = 0b00,
    BME280_MODE_FORCED = 0b01,
    BME280_MODE_NORMAL = 0b11
} bme280_sensor_mode;

typedef enum {
    BME280_FILTER_OFF = 0b000,
    BME280_FILTER_X2 = 0b001,
    BME280_FILTER_X4 = 0b010,
    BME280_FILTER_X8 = 0b011,
    BME280_FILTER_X16 = 0b100
} bme280_sensor_filter;

// standby durations in ms
typedef enum {
    BME280_STANDBY_MS_0_5 = 0b000,
    BME280_STANDBY_MS_10 = 0b110,
    BME280_STANDBY_MS_20 = 0b111,
    BME280_STANDBY_MS_62_5 = 0b001,
    BME280_STANDBY_MS_125 = 0b010,
    BME280_STANDBY_MS_250 = 0b011,
    BME280_STANDBY_MS_500 = 0b100,
    BME280_STANDBY_MS_1000 = 0b101
} bme280_standby_duration;

// The config register
typedef struct config {
    // inactive duration (standby time) in normal mode
    // 000 = 0.5 ms
    // 001 = 62.5 ms
    // 010 = 125 ms
    // 011 = 250 ms
    // 100 = 500 ms
    // 101 = 1000 ms
    // 110 = 10 ms
    // 111 = 20 ms
    unsigned int t_sb :3;

    // filter settings
    // 000 = filter off
    // 001 = 2x filter
    // 010 = 4x filter
    // 011 = 8x filter
    // 100 and above = 16x filter
    unsigned int filter :3;

    // unused - don't set
    unsigned int none :1;
    unsigned int spi3w_en :1;
} bme280_config_t;

// The ctrl_meas register
typedef struct ctrl_meas {
    // temperature oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_t :3;

    // pressure oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_p :3;

    // device mode
    // 00       = sleep
    // 01 or 10 = forced
    // 11       = normal
    unsigned int mode :2;
} bme280_ctrl_meas_t;

// The ctrl_hum register
typedef struct ctrl_hum {
    // unused - don't set
    unsigned int none :5;

    // pressure oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_h :3;
} bme280_ctrl_hum_t;

typedef void* bme280_handle_t; /*handle of bme280*/

/**
 * @brief   Create bme280 handle_t
 *
 * @param   dev object handle of I2C
 * @param   decice address
 *
 * @return
 *     - bme280_handle_t
 */
bme280_handle_t iot_bme280_create(i2c_bus_handle_t bus, uint16_t dev_addr);

/**
 * @brief   delete bme280 handle_t
 *
 * @param   dev object handle of bme280
 * @param   whether delete i2c bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_bme280_delete(bme280_handle_t dev, bool del_bus);

/**
 * @brief   Write a data on addr
 *
 * @param   dev object handle of bme280
 * @param   The address of the data to be written
 * @param   the data to be written
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_write_byte(bme280_handle_t dev, uint8_t addr,
        uint8_t data);

/**
 * @brief   Read a data on addr
 *
 * @param   dev object handle of bme280
 * @param   The address of the data to be read
 * @param   Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_read_byte(bme280_handle_t dev, uint8_t addr,
        uint8_t *data);

/**
 * @brief   Write some data start addr
 *
 * @param   dev object handle of bme280
 * @param   The address of the data to be written
 * @param   The size of the data to be written
 * @param   Pointer to write data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_write(bme280_handle_t dev, uint8_t start_addr,
        uint8_t write_num, uint8_t *data_buf);

/**
 * @brief   Read some data start addr
 *
 * @param   dev object handle of bme280
 * @param   The address of the data to be read
 * @param   The size of the data to be read
 * @param   Pointer to read data
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_read(bme280_handle_t dev, uint8_t start_addr,
        uint8_t read_num, uint8_t *data_buf);

/**
 * @brief   Get the value of BME280_REGISTER_CONFIG register
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - unsigned int: the value of BME280_REGISTER_CONFIG register
 */
unsigned int iot_bme280_getconfig(bme280_handle_t dev);

/**
 * @brief   Get the value of BME280_REGISTER_CONTROL measure register
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - unsigned int the value of BME280_REGISTER_CONTROL register
 */
unsigned int iot_bme280_getctrl_meas(bme280_handle_t dev);

/**
 * @brief   Get the value of BME280_REGISTER_CONTROLHUMID measure register
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - unsigned int the value of BME280_REGISTER_CONTROLHUMID register
 */
unsigned int iot_bme280_getctrl_hum(bme280_handle_t dev);

/**
 * @brief return true if chip is busy reading cal data
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - true chip is busy
 *    - false chip is idle or wrong
 */
bool iot_bme280_is_reading_calibration(bme280_handle_t dev);

/**
 * @brief Reads the factory-set coefficients
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_read_coefficients(bme280_handle_t dev);

/**
 * @brief  setup sensor with given parameters / settings
 *
 * @param   dev object handle of bme280
 * @param   Sensor working mode
 * @param   the sample of temperature measure
 * @param   the sample of pressure measure
 * @param   the sample of humidity measure
 * @param   Sensor filter multiples
 * @param   standby duration of sensor
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_set_sampling(bme280_handle_t dev, bme280_sensor_mode mode,
        bme280_sensor_sampling tempsampling,
        bme280_sensor_sampling presssampling,
        bme280_sensor_sampling humsampling, bme280_sensor_filter filter,
        bme280_standby_duration duration);

/**
 * @brief init bme280 device
 *
 * @param dev object handle of bme280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_init(bme280_handle_t dev);

/**
 * @brief  Take a new measurement (only possible in forced mode)
 * If we are in forced mode, the BME sensor goes back to sleep after each
 * measurement and we need to set it to forced mode once at this point, so
 * it will take the next measurement and then return to sleep again.
 * In normal mode simply does new measurements periodically.
 *
 * @param   dev object handle of bme280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t iot_bme280_take_forced_measurement(bme280_handle_t dev);

/**
 * @brief  Returns the temperature from the sensor
 *
 * @param  dev object handle of bme280
 *
 * @return
 *    - temperature value
 */
float iot_bme280_read_temperature(bme280_handle_t dev);

/**
 * @brief  Returns the temperature from the sensor
 *
 * @param  dev object handle of bme280
 *
 * @return
 *    - pressure value
 */
float iot_bme280_read_pressure(bme280_handle_t dev);

/**
 * @brief  Returns the humidity from the sensor
 *
 * @param dev object handle of bme280
 *
 * @return
 *    - humidity value
 */
float iot_bme280_read_humidity(bme280_handle_t dev);

/**
 * @brief Calculates the altitude (in meters) from the specified atmospheric
 *  pressure (in hPa), and sea-level pressure (in hPa).
 *
 * @param  dev object handle of bme280
 * @param  seaLevel: Sea-level pressure in hPa
 *
 * @return
 *    - altitude value
 */
float iot_bme280_read_altitude(bme280_handle_t dev, float seaLevel);

/**
 * Calculates the pressure at sea level (in hPa) from the specified altitude
 * (in meters), and atmospheric pressure (in hPa).
 *
 * @param  dev object handle of bme280
 * @param  altitude      Altitude in meters
 * @param  atmospheric   Atmospheric pressure in hPa
 *
 * @return
 *    - pressure value
 */
float iot_bme280_calculates_pressure(bme280_handle_t dev, float altitude,
        float atmospheric);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
/**
 * class of bme280 dev
 * simple usage:
 * Cbme280 *bme280 = new Cbme280(&i2c_bus);
 * bme280.read();
 * ......
 * delete(bme280);
 */
class CBme280
{
private:
    bme280_handle_t m_dev_handle;
    CI2CBus *bus;

    /**
     * prevent copy constructing
     */
    CBme280(const CBme280&);
    CBme280& operator =(const CBme280&);

public:
    /**
     * @brief   constructbme280bme280
     *
     * @param   p_i2c_bus pointer to CI2CBus object
     * @param   addr of device address
     */
    CBme280(CI2CBus *p_i2c_bus, uint8_t addr = BME280_I2C_ADDRESS_DEFAULT);

    ~CBme280();

    /**
     * @brief init bme280 device
     *
     * @param dev object handle of bme280
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t init(void);

    /**
     * @brief Reads the factory-set coefficients
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t read_coefficients(void);

    /**
     * @brief  setup sensor with given parameters / settings
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t set_sampling(bme280_sensor_mode mode = BME280_MODE_NORMAL,
            bme280_sensor_sampling tempsampling = BME280_SAMPLING_X16,
            bme280_sensor_sampling presssampling = BME280_SAMPLING_X16,
            bme280_sensor_sampling humsampling = BME280_SAMPLING_X16,
            bme280_sensor_filter filter = BME280_FILTER_OFF,
            bme280_standby_duration duration = BME280_STANDBY_MS_0_5);

    /**
     * @brief  Take a new measurement (only possible in forced mode)
     * If we are in forced mode, the BME sensor goes back to sleep after each
     * measurement and we need to set it to forced mode once at this point, so
     * it will take the next measurement and then return to sleep again.
     * In normal mode simply does new measurements periodically.
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - ESP_OK Success
     *    - ESP_FAIL Fail
     */
    esp_err_t take_forced_measurement(void);

    /**
     * @brief  Returns the temperature from the sensor
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - temperature value
     */
    float temperature();

    /**
     * @brief  Returns the temperature from the sensor
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - pressure value
     */
    float pressure();

    /**
     * @brief  Returns the humidity from the sensor
     *
     * @param   dev object handle of bme280
     *
     * @return
     *    - humidity value
     */
    float humidity();
    /**
     * @brief Calculates the altitude (in meters) from the specified atmospheric
     *  pressure (in hPa), and sea-level pressure (in hPa).
     *
     * @param  dev object handle of bme280
     * @param  seaLevel      Sea-level pressure in hPa
     *
     * @return
     *    - altitude value
     */
    float altitude(float seaLevel);

    /**
     * Calculates the pressure at sea level (in hPa) from the specified altitude
     * (in meters), and atmospheric pressure (in hPa).
     *
     * @param  dev object handle of bme280
     * @param  altitude      Altitude in meters
     * @param  atmospheric   Atmospheric pressure in hPa
     *
     * @return
     *    - pressure value
     */
    float calculates_pressure(float altitude, float atmospheric);
};
#endif

#endif
