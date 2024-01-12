/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/* BLE HRP EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_HRP_EVENTS);
/** @endcond **/

/* 16 Bit Heart Rate Service UUID */
#define BLE_HRP_UUID16                                                  0x180D

/* 16 Bit Heart Rate Service Characteristic UUIDs */
#define BLE_HRP_CHR_UUID16_MEASUREMENT                                  0x2A37
#define BLE_HRP_CHR_UUID16_BODY_SENSOR_LOC                              0x2A38
#define BLE_HRP_CHR_UUID16_HEART_RATE_CNTL_POINT                        0x2A39

/* The maximum number of RR-Interval Values */
#define BLE_HRP_CHR_MERSUREMENT_RR_INTERVAL_MAX_NUM                     7

/**
 * @brief   Heart Rate Measurement Characteristic
 */
typedef struct {
    struct {
        uint8_t format: 1;                                              /*!< Heart rate value format flag */
        uint8_t detected: 1;                                            /*!< Sensor contact detected flag */
        uint8_t supported: 1;                                           /*!< Sensor contact supported flag */
        uint8_t energy: 1;                                              /*!< Energy expended present flag */
        uint8_t interval: 1;                                            /*!< RR-Interval present flag */
        uint8_t reserved: 3;                                            /*!< Reserved for future use flag */
    } flags;                                                            /*!< Flags of heart rate measurement */

    union {
        uint8_t  u8;                                                    /*!< 8 bit resolution */
        uint16_t u16;                                                   /*!< 16 bit resolution */
    } heartrate;                                                        /*!< Heart rate measurement value */

    uint16_t energy_val;                                                /*!< Expended energy value */
    uint16_t interval_buf[BLE_HRP_CHR_MERSUREMENT_RR_INTERVAL_MAX_NUM]; /*!< The RR-Interval value represents the time between two R-Wave detections */
} __attribute__((packed)) esp_ble_hrp_data_t;

/* Heart Rate Measurement Flags Bit Masks */
#define BLE_HRP_FLAGS_BM_NONE                                           0x00
#define BLE_HRP_FLAGS_BM_FORMAT                                         0x01
#define BLE_HRP_FLAGS_BM_SENSOR_CONTACT_DETECTED                        0x02
#define BLE_HRP_FLAGS_BM_SENSOR_CONTACT_SUPPOTRED                       0x04
#define BLE_HRP_FLAGS_BM_ENERGY                                         0x08
#define BLE_HRP_FLAGS_BM_RR_INTERVAL                                    0x10
#define BLE_HRP_FLAGS_BM_RFU                                            0x70

/* Heart Rate Measurement Format Flag */
#define BLE_HRP_CHR_MERSUREMENT_FLAGS_FORMAT_U8                         0
#define BLE_HRP_CHR_MERSUREMENT_FLAGS_FORMAT_U16                        1

#define BLE_HRP_CHR_MERSUREMENT_FLAGS_NOT                               0
#define BLE_HRP_CHR_MERSUREMENT_FLAGS_SET                               1

/* Heart Rate Control Point Command IDs */
#define BLE_HRP_CMD_RFU                                                 0
#define BLE_HRP_CMD_RESET_ENERGY_EXPENDED                               1
#define BLE_HRP_CMD_MAX                                                 2

/**
 * @brief Get the sensor location value of the device.
 *
 * @param[in]  location The pointer to store the sensor location value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery level
 */
esp_err_t esp_ble_hrp_get_location(uint8_t *location);

/**
 * @brief Get the control point value of the device.
 *
 * @param[in]  cmd_id The pointer to store the control point value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery level
 */
esp_err_t esp_ble_hrp_get_ctrl(uint8_t *cmd_id);

/**
 * @brief Set the control point value of the device.
 *
 * @param[in]  cmd_id The control point value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery level
 */
esp_err_t esp_ble_hrp_set_ctrl(uint8_t cmd_id);

/**
 * @brief Initialization Heart Rate Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_hrp_init(void);

/**
 * @brief Deinitialization Heart Rate Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_hrp_deinit(void);

#ifdef __cplusplus
}
#endif
