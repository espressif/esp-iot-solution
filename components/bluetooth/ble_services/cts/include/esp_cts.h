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

/** @cond **/
/* BLE CTS EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_CTS_EVENTS);
/** @endcond **/

/* 16 Bit Current Time Service UUID */
#define BLE_CTS_UUID16                                                          0x1805

/* 16 Bit Current Time Characteristics UUID */
#define BLE_CTS_CHR_UUID16_CURRENT_TIME                                         0x2A2B
#define BLE_CTS_CHR_UUID16_LOCAL_TIME                                           0x2A0F
#define BLE_CTS_CHR_UUID16_REFERENCE_TIME                                       0x2A14

/* CTS Adjust Reason Masks */
#define BLE_CTS_MANUAL_TIME_UPDATE_MASK                                        (1 << 0)
#define BLE_CTS_EXTERNAL_REFERENCE_TIME_UPDATE_MASK                            (1 << 1)
#define BLE_CTS_CHANGE_OF_TIME_ZONE_MASK                                       (1 << 2)
#define BLE_CTS_CHANGE_OF_DST_MASK                                             (1 << 3)

/**
 * @brief   Current Time
 */
typedef struct {
    struct {
        uint16_t year;                                      /*!< Year as defined by the Gregorian calendar, Valid range 1582 to 9999 */
        uint8_t  month;                                     /*!< Month of the year as defined by the Gregorian calendar, Valid range 1 (January) to 12 (December) */
        uint8_t  day;                                       /*!< Day of the month as defined by the Gregorian calendar, Valid range 1 to 31 */
        uint8_t  hours;                                     /*!< Number of hours past midnight, Valid range 0 to 23 */
        uint8_t  minutes;                                   /*!< Number of minutes since the start of the hour. Valid range 0 to 59 */
        uint8_t  seconds;                                   /*!< Number of seconds since the start of the minute. Valid range 0 to 59 */
    } __attribute__((packed)) timestamp;                    /*!< The date and time */

    uint8_t day_of_week;                                    /*!< Day_of_week. valid range : 1(Monday) - 7(Sunday), 0 means day of week not known */
    uint8_t fractions_256;                                  /*!< Fractions_256. the number of 1 / 256 fractions of second, valid range : 0 - 255 */
    uint8_t adjust_reason;                                  /*!< This field represents reason(s) for adjusting time */
} __attribute__((packed)) esp_ble_cts_cur_time_t;

/**
 * @brief   Local Time Information
 */
typedef struct {
    /* This field represents the offset from UTC in number of 15-minute increments.
     * Valid range from -48 to +56.
     * A value of -128 means that the time zone offset is not known.
     * All other values are Reserved for Future Use.
     * The offset defined in this characteristic is constant regardless of whether daylight savings is in effect.
     */
    int8_t timezone;                                        /*!< Time zone */
    /** dst_offset.
     *  allowed values : 0, 2, 4, 8, 255
     */
    uint8_t dst_offset;                                     /*!< Dst offset */
} __attribute__((packed)) esp_ble_cts_local_time_t;

/**
 * @brief   reference time information
 */
typedef struct {
    /** time_source.
     *  valid range : 0 - 253
     *  255 means not known
     */
    uint8_t time_source;                                    /*!< Time source */
    /*
     * This field represents accuracy (drift) of time information in steps of 1/8 of a second (125ms) compared to a reference time source.
     * Valid range from 0 to 253 (0s to 31.625s).
     * A value of 254 means drift is larger than 31.625s.
     * A value of 255 means drift is unknown.
     */
    uint8_t time_accuracy;                                  /*!< Time accuracy */
    /** days_since_update.
     *  valid range : 0 - 254
     *  A value of 255 is used when the time span is greater than or equal to 255 days
     */
    uint8_t days_since_update;                              /*!< Days since update */
    /** hours_since_update.
     *  valid range : 0 - 23
     *  A value of 255 is used when the time span is greater than or equal to 255 days
     */
    uint8_t hours_since_update;                             /*!< Hours since update */
} __attribute__((packed)) esp_ble_cts_ref_time_t;

/**
 * @brief Read the value of Current Time characteristic.
 *
 * @param[in]  out_val The pointer to store the Current Time Increment.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_cts_get_current_time(esp_ble_cts_cur_time_t *out_val);

/**
 * @brief Set the Current Time characteristic value.
 *
 * @param[in]  in_val The pointer to store the Current Time.
 *
 * @param[in]  need_send send the current time to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_cts_set_current_time(esp_ble_cts_cur_time_t *in_val, bool need_send);

/**
 * @brief Read the value of Local Time characteristic.
 *
 * @param[in]  out_val The pointer to store the Local Time Increment.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_cts_get_local_time(esp_ble_cts_local_time_t *out_val);

/**
 * @brief Set the Local Time characteristic value.
 *
 * @param[in]  in_val The pointer to store the Local Time.
 *
 * @return
 *  - ESP_OK on successful
 */
esp_err_t esp_ble_cts_set_local_time(esp_ble_cts_local_time_t *in_val);

/**
 * @brief Read the value of Reference Time characteristic.
 *
 * @param[in]  out_val The pointer to store the Reference Time Increment.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_cts_get_reference_time(esp_ble_cts_ref_time_t *out_val);

/**
 * @brief Set the Reference Time characteristic value.
 *
 * @param[in]  in_val The pointer to store the Reference Time.
 *
 * @return
 *  - ESP_OK on successful
 */
esp_err_t esp_ble_cts_set_reference_time(esp_ble_cts_ref_time_t *in_val);

/**
 * @brief Initialization Current Time Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_cts_init(void);

#ifdef __cplusplus
}
#endif
