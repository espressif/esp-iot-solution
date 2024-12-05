/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SHT3x_H_
#define _SHT3x_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "i2c_bus.h"
#include "esp_log.h"
#include "math.h"

typedef enum {

    SOFT_RESET_CMD = 0x30A2,    /*!< Command to soft reset*/
    READOUT_FOR_PERIODIC_MODE = 0xE000,     /*!< Command to read Periodic*/
    READ_SERIAL_NUMBER = 0x3780,        /*!< Command to read senser number*/
    SHT3x_STOP_PERIODIC = 0x3093,       /*!< Command to break or stop periodic mode*/
    SHT3x_ART_CMD = 0x2B32,             /*!< Command to accelerated response time*/

    /*  Single Shot Data Acquisition Mode*/
    SHT3x_SINGLE_HIGH_ENABLED    = 0x2C06,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in high repeatability and Clock Stretching enabled*/
    SHT3x_SINGLE_MEDIUM_ENABLED  = 0x2C0D,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in medium repeatability and Clock Stretching enabled*/
    SHT3x_SINGLE_LOW_ENABLED     = 0x2C10,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in low repeatability and Clock Stretching enabled*/
    SHT3x_SINGLE_HIGH_DISABLED   = 0x2400,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in high repeatability and Clock Stretching disabled*/
    SHT3x_SINGLE_MEDIUM_DISABLED = 0x240B,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in medium repeatability and Clock Stretching disabled*/
    SHT3x_SINGLE_LOW_DISABLED    = 0x2416,      /*!< Command to set measure mode as Single Shot Data Acquisition mode in low repeatability and Clock Stretching disabled*/

    /*  Periodic Data Acquisition mode*/
    SHT3x_PER_0_5_HIGH   = 0x2032,      /*!< Command to set measure mode as Periodic Data Acquisition mode in high repeatability and 0.5 mps*/
    SHT3x_PER_0_5_MEDIUM = 0x2024,      /*!< Command to set measure mode as Periodic Data Acquisition mode in medium repeatability and 0.5 mps*/
    SHT3x_PER_0_5_LOW    = 0x202F,      /*!< Command to set measure mode as Periodic Data Acquisition mode in low repeatability and 0.5 mps*/
    SHT3x_PER_1_HIGH     = 0x2130,      /*!< Command to set measure mode as Periodic Data Acquisition mode in high repeatability and 1 mps*/
    SHT3x_PER_1_MEDIUM   = 0x2126,      /*!< Command to set measure mode as Periodic Data Acquisition mode in medium repeatability and 1 mps*/
    SHT3x_PER_1_LOW      = 0x212D,      /*!< Command to set measure mode as Periodic Data Acquisition mode in low repeatability and 1 mps*/
    SHT3x_PER_2_HIGH     = 0x2236,      /*!< Command to set measure mode as Periodic Data Acquisition mode in high repeatability and 2 mps*/
    SHT3x_PER_2_MEDIUM   = 0x2220,      /*!< Command to set measure mode as Periodic Data Acquisition mode in medium repeatability and 2 mps*/
    SHT3x_PER_2_LOW      = 0x222B,      /*!< Command to set measure mode as Periodic Data Acquisition mode in low repeatability and 2 mps*/
    SHT3x_PER_4_HIGH     = 0x2334,      /*!< Command to set measure mode as Periodic Data Acquisition mode in high repeatability and 4 mps*/
    SHT3x_PER_4_MEDIUM   = 0x2322,      /*!< Command to set measure mode as Periodic Data Acquisition mode in medium repeatability and 4 mps*/
    SHT3x_PER_4_LOW      = 0x2329,      /*!< Command to set measure mode as Periodic Data Acquisition mode in low repeatability and 4 mps*/
    SHT3x_PER_10_HIGH    = 0x2737,      /*!< Command to set measure mode as Periodic Data Acquisition mode in high repeatability and 10 mps*/
    SHT3x_PER_10_MEDIUM  = 0x2721,      /*!< Command to set measure mode as Periodic Data Acquisition mode in medium repeatability and 10 mps*/
    SHT3x_PER_10_LOW     = 0x272A,      /*!< Command to set measure mode as Periodic Data Acquisition mode in low repeatability and 10 mps*/

    /*  cmd for sht3x heater condition*/
    SHT3x_HEATER_ENABLE = 0x306D,   /*!< Command to enable the heater*/
    SHT3x_HEATER_DISABLED = 0x3066,   /*!< Command to disable the heater*/
} sht3x_cmd_measure_t;

typedef enum {
    SHT3x_ADDR_PIN_SELECT_VSS = 0x44, /*!< set address PIN select VSS  */
    SHT3x_ADDR_PIN_SELECT_VDD = 0x45, /*!< set address PIN select VDD  */
} sht3x_set_address_t;

typedef void *sht3x_handle_t;

/**
 * @brief Create sht3x handle_t
 *
 * @param bus sensorice object handle of sht3x
 * @param dev_addr sensorice address
 *
 * @return
 *     - sht3x handle_t
 */
sht3x_handle_t sht3x_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief Delete sht3x handle_t
 *
 * @param sensor point to sensorice object handle of sht3x
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_delete(sht3x_handle_t *sensor);

/**
 * @brief Get temperature and humidity
 *
 * @param sensor object handle of shd3x
 * @param Tem_val temperature data buffer
 * @param Hum_val humidity data buffer
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_get_humiture(sht3x_handle_t sensor, float *Tem_val, float *Hum_val);

/**
 * @brief Get temperature and humidity just once
 *
 * @param sensor object handle of shd3x
 * @param Tem_val temperature data
 * @param Hum_val humidity data
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_get_single_shot(sht3x_handle_t sensor, float *Tem_val, float *Hum_val);

/**
 * @brief Soft reset for sht3x
 *
 * @param sensor object handle of sht3x
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_soft_reset(sht3x_handle_t sensor);

/**
 * @brief stop or break or stop periodic mode
 *
 * @param sensor object handle of sht3x
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_stop_periodic(sht3x_handle_t sensor);

/**
 * @brief accelerated response time
 *
 * @param sensor object handle of sht3x
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_art(sht3x_handle_t sensor);

/**
 * @brief set measure mode of sht3x
 *
 * @param sensor object handle of shd3x
 * @param sht3x_cmd_measure_t the instruction to set measurement mode
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t sht3x_set_measure_mode(sht3x_handle_t sensor, sht3x_cmd_measure_t sht3x_measure_mode);

/**
 * @brief change the condition of sht3x heater
 *
 * @param sensor object handle of shd3x
 * @param sht3x_cmd_measure_t the instruction to turn on/off heater of sht3x
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 * @note
 *      the default condition of heater is disabled
 */
esp_err_t sht3x_heater(sht3x_handle_t sensor, sht3x_cmd_measure_t sht3x_heater_condition);

#ifdef __cplusplus
}
#endif

#endif
