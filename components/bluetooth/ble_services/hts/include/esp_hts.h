/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/* BLE HTS EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_HTS_EVENTS);
/** @endcond **/

/* 16 Bit Health Thermometer Service UUID */
#define BLE_HTS_UUID16                                  0x1809

/* 16 Bit Health Thermometer Service Characteristic UUIDs */
#define BLE_HTS_CHR_UUID16_TEMPERATURE_MEASUREMENT      0x2A1C
#define BLE_HTS_CHR_UUID16_TEMPERATURE_TYPE             0x2A1D
#define BLE_HTS_CHR_UUID16_INTERMEDIATE_TEMPERATURE     0x2A1E
#define BLE_HTS_CHR_UUID16_MEASUREMENT_INTERVAL         0x2A21

/**
 * @brief   Temperature Measurement and Intermediate Temperature Characteristic
 */
typedef struct {
    struct {
        uint8_t temperature_unit: 1;                        /*!< Temperature units flag */
        uint8_t time_stamp: 1;                              /*!< Time stamp flag */
        uint8_t temperature_type: 1;                        /*!< Temperature type flag */
        uint8_t reserved: 5;                                /*!< Reserved for future use */
    } flags;                                                /*!< Flags of temperature */

    union {
        uint32_t celsius;                                   /*!< Celsius unit */
        uint32_t fahrenheit;                                /*!< Fahrenheit unit */
    } temperature;                                          /*!< Temperature value */

    struct {
        uint16_t year;                                      /*!< Year as defined by the Gregorian calendar, Valid range 1582 to 9999 */
        uint8_t  month;                                     /*!< Month of the year as defined by the Gregorian calendar, Valid range 1 (January) to 12 (December) */
        uint8_t  day;                                       /*!< Day of the month as defined by the Gregorian calendar, Valid range 1 to 31 */
        uint8_t  hours;                                     /*!< Number of hours past midnight, Valid range 0 to 23 */
        uint8_t  minutes;                                   /*!< Number of minutes since the start of the hour. Valid range 0 to 59 */
        uint8_t  seconds;                                   /*!< Number of seconds since the start of the minute. Valid range 0 to 59 */
    } __attribute__((packed)) timestamp;                    /*!< The date and time */
    uint8_t  location;                                      /*!< The location of a temperature measurement */
} __attribute__((packed)) esp_ble_hts_temp_t;

/* Temperature Units Flag */
#define BLE_HTS_CHR_TEMPERATURE_UNITS_CELSIUS       0
#define BLE_HTS_CHR_TEMPERATURE_UNITS_FAHRENHEIT    1

#define BLE_HTS_CHR_TEMPERATURE_FLAGS_NOT                                0
#define BLE_HTS_CHR_TEMPERATURE_FLAGS_SET                                1

/* Temperature Type Characteristic */
#define BLE_HTS_CHR_TEMPERATURE_TYPE_RFU        0
#define BLE_HTS_CHR_TEMPERATURE_TYPE_ARMPIT     1
#define BLE_HTS_CHR_TEMPERATURE_TYPE_BODY       2
#define BLE_HTS_CHR_TEMPERATURE_TYPE_EAR        3
#define BLE_HTS_CHR_TEMPERATURE_TYPE_FINGER     4
#define BLE_HTS_CHR_TEMPERATURE_TYPE_GAST_TRACT 5
#define BLE_HTS_CHR_TEMPERATURE_TYPE_MOUTH      6
#define BLE_HTS_CHR_TEMPERATURE_TYPE_RECTUM     7
#define BLE_HTS_CHR_TEMPERATURE_TYPE_TOE        8
#define BLE_HTS_CHR_TEMPERATURE_TYPE_TYMPANUM   9
#define BLE_HTS_CHR_TEMPERATURE_TYPE_MAX        10

/**
 * @brief Get the current temperature type value of the device.
 *
 * @param[in]  temp_type The pointer to store the current temperature type value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong temperature type
 */
esp_err_t esp_ble_hts_get_temp_type(uint8_t *temp_type);

/**
 * @brief Set the current temperature type value of the device.
 *
 * @param[in]  temp_type The current temperature type value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong temperature type
 */
esp_err_t esp_ble_hts_set_temp_type(uint8_t temp_type);

/**
 * @brief Get the value of the temperature measurement of the device.
 *
 * @param[in]  temp_val The pointer to store the value of the temperature measurement
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong temperature measurement
 */
esp_err_t esp_ble_hts_get_measurement_temp(esp_ble_hts_temp_t *temp_val);

/**
 * @brief Set the value of the temperature measurement of the device.
 *
 * @param[in]  temp_val The pointer to store the value of the temperature measurement
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong temperature measurement
 */
esp_err_t esp_ble_hts_set_measurement_temp(esp_ble_hts_temp_t *temp_val);

/**
 * @brief Get the value of the intermediate temperature of the device.
 *
 * @param[in]  temp_val The pointer to store the value of the intermediate temperature
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong intermediate temperature
 */
esp_err_t esp_ble_hts_get_intermediate_temp(esp_ble_hts_temp_t *temp_val);

/**
 * @brief Set the value of the intermediate temperature of the device.
 *
 * @param[in]  temp_val The pointer to store the value of the intermediate temperature
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong intermediate temperature
 */
esp_err_t esp_ble_hts_set_intermediate_temp(esp_ble_hts_temp_t *temp_val);

/**
 * @brief Get the measurement interval value of the device.
 *
 * @param[in]  interval_val The pointer to store the measurement interval value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong measurement interval
 */
esp_err_t esp_ble_hts_get_measurement_interval(uint16_t *interval_val);

/**
 * @brief Set the measurement interval value of the device.
 *
 * @param[in]  interval_val The measurement interval value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong measurement interval
 */
esp_err_t esp_ble_hts_set_measurement_interval(uint16_t interval_val);

/**
 * @brief Initialization Health Thermometer Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_hts_init(void);

#ifdef __cplusplus
}
#endif
