/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Maximum Attribute Value Length */
#define BLE_BCS_MAX_VAL_LEN                                                     100

/* 16 Bit Body Composition Service UUID */
#define BLE_BCS_UUID16                                                          0x181B

/* 16 Bit Body Composition Characteristic UUIDs */
#define BLE_BCS_CHR_UUID16_FEATURE                                              0x2A9B
#define BLE_BCS_CHR_UUID16_MEASUREMENT                                          0x2A9C

/* Body Composition Feature Bit Masks */
#define BLE_BCS_FEAT_TIME_STAMP                    (0x1 << 0)   //bit0 Time Stamp（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_MULTI_USER                    (0x1 << 1)   //bit1 User ID（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_BASAL_METABOLISM              (0x1 << 2)   //bit2 Basal Metabolism（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_MUSCLE_PERCENTAGE             (0x1 << 3)   //bit3 Muscle Percentage（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_MUSCLE_MASS                   (0x1 << 4)   //bit4 Muscle Mass（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_FAT_FREE_MASS                 (0x1 << 5)   //bit5 Fat Free Mass（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_SOFT_LEAN_MASS                (0x1 << 6)   //bit6 Soft Lean Mass（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_BODY_WATER_MASS               (0x1 << 7)   //bit7 Body Water Mass（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_IMPEDENCE                     (0x1 << 8)   //bit8 Impedance（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_WEIGHT                        (0x1 << 9)   //bit9 Weight（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_HEIGHT                        (0x1 << 10)  //bit10 Height（0-Don't support, 1-Support)
#define BLE_BCS_FEAT_MASS_MEASUREMENT_RESOLUTION   (0x1 << 11)  //bit11-bit14 Weight Resolution (000-undefine, 001-0.5kg or 1lb, 002-0.2kg or 0.5lb, 003-0.1kg or 0.2lb, 004-0.05kg or 0.1lb, 005-0.02kg or 0.05lb, 006-0.01kg or 0.02lb, 007-0.005kg or 0.01lb);
#define BLE_BCS_FEAT_HEIGHT_RESOLUTION             (0x1 << 15)  //bit15-bit17 Height Resolution (000-undefine, 001-0.01m or 1ft, 002-0.005m or 0.5ft, 003-0.001m or 0.1ft);

/* Body Composition Measurement Flags Bit Masks */
#define BLE_BCS_FLAG_MEASUREMENT_UNITS             (0x1 << 0)   //bit0 Measurement Units
#define BLE_BCS_FLAG_TIME_STAMP                    (0x1 << 1)   //bit1 Time Stamp
#define BLE_BCS_FLAG_MULTI_USER                    (0x1 << 2)   //bit2 User ID
#define BLE_BCS_FLAG_BASAL_METABOLISM              (0x1 << 3)   //bit3 Basal Metabolism
#define BLE_BCS_FLAG_MUSCLE_PERCENTAGE             (0x1 << 4)   //bit4 Muscle Percentage
#define BLE_BCS_FLAG_MUSCLE_MASS                   (0x1 << 5)   //bit5 Muscle Mass
#define BLE_BCS_FLAG_FAT_FREE_MASS                 (0x1 << 6)   //bit6 Fat Free Mass
#define BLE_BCS_FLAG_SOFT_LEAN_MASS                (0x1 << 7)   //bit7 Soft Lean Mass
#define BLE_BCS_FLAG_BODY_WATER_MASS               (0x1 << 8)   //bit8 Body Water Mass
#define BLE_BCS_FLAG_IMPEDENCE                     (0x1 << 9)   //bit9 Impedance
#define BLE_BCS_FLAG_WEIGHT                        (0x1 << 10)  //bit10 Weight
#define BLE_BCS_FLAG_HEIGHT                        (0x1 << 11)  //bit11 Height
#define BLE_BCS_FLAG_MULTIPLE_PACKET               (0x1 << 12)  //bit12 Multiple Packet

/**
 * @brief   Body Composition Measurement Characteristic
 */
typedef struct {
    uint32_t bcs_flag;                                      /*!< Body Composition flag field */

    struct {
        uint16_t year;                                      /*!< Year as defined by the Gregorian calendar, Valid range 1582 to 9999 */
        uint8_t  month;                                     /*!< Month of the year as defined by the Gregorian calendar, Valid range 1 (January) to 12 (December) */
        uint8_t  day;                                       /*!< Day of the month as defined by the Gregorian calendar, Valid range 1 to 31 */
        uint8_t  hours;                                     /*!< Number of hours past midnight, Valid range 0 to 23 */
        uint8_t  minutes;                                   /*!< Number of minutes since the start of the hour. Valid range 0 to 59 */
        uint8_t  seconds;                                   /*!< Number of seconds since the start of the minute. Valid range 0 to 59 */
    } __attribute__((packed)) timestamp;                    /*!< The date and time */

    uint8_t user_id;                                        /*!< User ID field */
    uint16_t basal_metabolism;                              /*!< Basal Metabolism field */
    uint16_t muscle_percentage;                             /*!< Muscle Percentage field */
    uint16_t muscle_mass;                                   /*!< Muscle Mass field */
    uint16_t fat_free_mass;                                 /*!< Fat Free Mass field */
    uint16_t soft_lean_mass;                                /*!< Soft Lean Mass field */
    uint16_t body_water_mass;                               /*!< Body Water Mass field */
    uint8_t impedance;                                      /*!< Impedance field */
    uint16_t weight;                                        /*!< Weight field */
    uint16_t height;                                        /*!< Height field */
} __attribute__((packed)) esp_bcs_val_t;

/**
 * @brief Set the Body Componsition value.
 *
 * @param[in]  in_val The pointer to store the Body Componsition Information.
 *
 * @param[in]  need_send If set to true, the Body Componsition Info will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_bcs_set_measurement(esp_bcs_val_t *in_val, bool need_send);

/**
 * @brief Initialization Body Componsion Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_bcs_init(void);

#ifdef __cplusplus
}
#endif
