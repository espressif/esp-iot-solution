/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* 16 Bit Weight Scale Service UUID */
#define BLE_WSS_UUID16                                                          0x181D

/* 16 Bit Weight Scale Characteristics UUID */
#define BLE_WSS_CHR_UUID16_WEIGHT_FEATURE                                       0x2A9E
#define BLE_WSS_CHR_UUID16_WEIGHT_MEASUREMENT                                   0x2A9D

/* Weight Scale Feature masks */
#define BLE_WSS_TIMESTAMP_MASK                                                 (1 << 0)
#define BLE_WSS_MULTI_USER_MASK                                                (1 << 1)
#define BLE_WSS_BMI_MASK                                                       (1 << 2)

/* Weight resolution offset*/
#define BLE_WSS_WEIGHT_RESOLUTION_OFSET                                        (3)

#define BLE_WSS_WEIGHT_RESOLUTION_NONE                                         (0x0)
#define BLE_WSS_WEIGHT_RESOLUTION_0P5_KG                                       (0x1)
#define BLE_WSS_WEIGHT_RESOLUTION_0P2_KG                                       (0x2)
#define BLE_WSS_WEIGHT_RESOLUTION_0P1_KG                                       (0x3)
#define BLE_WSS_WEIGHT_RESOLUTION_0P05_KG                                      (0x4)
#define BLE_WSS_WEIGHT_RESOLUTION_0P02_KG                                      (0x5)
#define BLE_WSS_WEIGHT_RESOLUTION_0P01_KG                                      (0x6)
#define BLE_WSS_WEIGHT_RESOLUTION_0P005_KG                                     (0x7)

/* Height resolution offset*/
#define BLE_WSS_HEIGHT_RESOLUTION_OFSET                                        (6)

#define BLE_WSS_HEIGHT_RESOLUTION_NONE                                         (0x0)
#define BLE_WSS_HEIGHT_RESOLUTION_0P01_M                                       (0x1)
#define BLE_WSS_HEIGHT_RESOLUTION_0P005_M                                      (0x2)
#define BLE_WSS_HEIGHT_RESOLUTION_0P001_M                                      (0x3)

/* Weight Scale Measurement Flag */
#define BLE_WSS_MEASUREMENT_UINTS_FLAG                                         (1 << 0)
#define BLE_WSS_TIME_STAMP_FLAG                                                (1 << 1)
#define BLE_WSS_USER_ID_FLAG                                                   (1 << 2)
#define BLE_WSS_BMI_FLAG                                                       (1 << 3)

/**
 * @brief   Weight Scale Feature
 */
typedef struct {
    uint32_t timestamp: 1;                                  /*!< 0: Don't Support, 1: Support */
    uint32_t user_id: 1;                                    /*!< 0: Don't Support, 1: Support */
    uint32_t bmi: 1;                                        /*!< 0: Don't Support, 1: Support */
    uint32_t weight: 1;                                     /*!< 0: Don't Support, 1: Support */
    uint32_t w_resolution: 3;                               /*!< If weight support, this filed should present */
    uint32_t height: 1;                                     /*!< 0: Don't Support, 1: Support */
    uint32_t h_resolution: 2;                               /*!< If height support, this filed should present */
} esp_ble_wss_feature_t;                                    /*!< The structure of the weight scale feature field */

/**
 * @brief   weight Measurement
 */
typedef struct {
    struct {
        uint32_t measurement_unit: 1;                       /*!< 0: Kg & meter, 1: reference to  weight and height resolution */
        uint32_t time_present: 1;                           /*!< 0: Don't contain time information, 1: time stamp present */
        uint32_t user_present: 1;                           /*!< 0: Don't contain user index, 1: contain user index */
        uint32_t bmi_height_present: 1;                     /*!< 0: Don't contain BMI and height, 1: contain BMI and height */
    } flag;                                                 /*!< Flag */

    uint16_t weight;                                        /*!< weight */

    struct {
        uint16_t year;                                      /*!< Year as defined by the Gregorian calendar, Valid range 1582 to 9999 */
        uint8_t  month;                                     /*!< Month of the year as defined by the Gregorian calendar, Valid range 1 (January) to 12 (December) */
        uint8_t  day;                                       /*!< Day of the month as defined by the Gregorian calendar, Valid range 1 to 31 */
        uint8_t  hours;                                     /*!< Number of hours past midnight, Valid range 0 to 23 */
        uint8_t  minutes;                                   /*!< Number of minutes since the start of the hour. Valid range 0 to 59 */
        uint8_t  seconds;                                   /*!< Number of seconds since the start of the minute. Valid range 0 to 59 */
    } __attribute__((packed)) timestamp;                    /*!< The date and time */

    uint8_t user_id;                                        /*!< User index */
    uint8_t bmi;                                            /*!< BMI */
    uint16_t height;                                        /*!< Height */
    uint8_t weight_resolution;                              /*!< Weight resolution */
    uint8_t height_resolution;                              /*!< Height resolution */
} __attribute__((packed)) esp_ble_wss_measurement_t;

/**
 * @brief Read the weight measurement characteristic value.
 *
 * @param[in]  out_val The pointer to store the weight measurement value.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_wss_get_measurement(esp_ble_wss_measurement_t *out_val);

/**
 * @brief Set the weight measurement characteristic value.
 *
 * @param[in]  in_val The pointer to store the weight measurement.
 *
 * @param[in]  need_send send the weight measurement information to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_wss_set_measurement(esp_ble_wss_measurement_t *in_val, bool need_send);

/**
 * @brief Initialization Weight Scale Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_wss_init(void);

#ifdef __cplusplus
}
#endif
