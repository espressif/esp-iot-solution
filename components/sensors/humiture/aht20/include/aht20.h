/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * @File: aht20.h
 *
 * @brief: AHT20 driver function declarations and typedefinitons
 *
 * @Date: May 2, 2025
 *
 * @Author: Rohan Jeet <jeetrohan92@gmail.com>
 *
 */
#ifndef AHT20_H
#define AHT20_H

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

#include "esp_bit_defs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"

#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AHT20_ADDRESS_LOW     0X38  /*!< AHT20 I2C ADDRESS LOW */
#define AHT20_ADDRESS_HIGH    0X39  /*!< AHT20 I2C ADDRESS HIGH */
#define AHT20_INIT_REG        0XBE  /*!< initialize the AHT20 */
#define AHT20_MEASURE_CYC     0xAC  /*!< trigger measurement in cycle mode */

/**
 *@brief AHT20 result
 */
typedef struct {
    float_t humidity;   /*!< humidity reading. */
    float_t temperature;    /*!< temperature reading. */
} aht20_reading_t;

/**
 *@brief AHT20 raw result
 */
typedef struct {
    uint32_t humidity;  /*!< raw humidity reading. */
    uint32_t temperature;   /*!< raw temperature reading. */
} aht20_raw_reading_t;

/**
 * @brief AHT20 device object
 */
typedef struct {
    i2c_bus_device_handle_t i2c_dev;   /*!< i2c device handle. */
    aht20_reading_t humiture;             /*!< AHT20 reading. */
} aht20_dev_config_t;

/**
 * @brief AHT20 device handle
 */
typedef aht20_dev_config_t *aht20_handle_t;

/**
* @brief soft reset AHT20
*
* @param[in] aht20_handle AHT20 device handle
*
* @return
*      - ESP_OK: successful reset
*      - other error codes : failed to reset
*
*/
esp_err_t aht20_reset(aht20_handle_t aht20_handle);

/**
* @brief get AHT20 sensor raw readings
*-
* @param[in] aht20_handle AHT20 device handle
*
* @param[out] raw_read raw reading
*
* @return
*      - ESP_OK: successful read
*      - other error codes : failed to read
*
*/
esp_err_t aht20_read_raw(aht20_handle_t aht20_handle, aht20_raw_reading_t *raw_read);

/**
* @brief get AHT20 sensor readings
*-
* @param[in] aht20_handle AHT20 device handle
*
* @return
*      - ESP_OK: successful read
*      - other error codes : failed to read
*
*/
esp_err_t aht20_read_humiture(aht20_handle_t aht20_handle);

/**
* @brief get AHT20 humidity readings
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[out] humidity AHT20 humidity reading
*
* @return
*      - ESP_OK: successful read
*      - other error codes : failed to read
*
*/
esp_err_t aht20_read_humidity(aht20_handle_t aht20_handle, float_t *humidity);

/**
* @brief get AHT20 temperature readings
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[out] temperature AHT20 temperature reading
*
* @return
*      - ESP_OK: successful read
*      - other error codes : failed to read
*
*/
esp_err_t aht20_read_temperature(aht20_handle_t aht20_handle, float_t *temperature);

/**
* @brief check AHT20 calibration status
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[out] calibration calibrated if value is true
*
* @return
*      - ESP_OK: successfully read AHT20 calibration status
*      - other error codes : failure in reading AHT20 calibration status
*
*/
esp_err_t aht20_calibration_status(aht20_handle_t aht20_handle, bool *calibration);

/**
* @brief check AHT20 measurement status
*
* @param[in] aht20_handle AHT20 device handle
*
* @param[out]busy busy in measurement if value is true
*
* @return
*      - ESP_OK: successfully read AHT20 busy status
*      - other error codes : failure in reading AHT20 busy status
*
*/
esp_err_t aht20_busy_status(aht20_handle_t aht20_handle, bool *busy);

/**
* @brief Initialize the AHT20
*
* @param[in] aht20_handle AHT20 device handle
*
* @return
*      - ESP_OK: successful initiailiztion
*      - other error codes : failed to initialize
*
*/
esp_err_t aht20_init(aht20_handle_t aht20_handle);

/**
* @brief create an AHT20 device handle
*
* @param[in] bus_handle I2C master bus handle, with which this device will be associated
*
* @param[in] scl_speed_hz I2C communication speed used by this device
*
* @param[in] aht20_address I2C address of this device
*
* @return
*      - NULL: Failed to create AHT20 device handle
*      - other error codes : from the underlying i2c driver, check log
*
*/
aht20_handle_t aht20_create(i2c_bus_handle_t bus_handle, uint8_t aht20_address);

/**
* @brief free the resources associated with AHT20 device
*
* @param[in] aht20_handler address of AHT20 device handle
*
* @return
*      - ESP_ERR_INVALID_ARG: Invalid argument
*      - ESP_OK : successful
*/
esp_err_t aht20_remove(aht20_handle_t *aht20_handler);

#ifdef __cplusplus
}
#endif    // __cplusplus

#endif    // __AHT20_H
