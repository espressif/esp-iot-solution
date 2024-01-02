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

/* 16 Bit Heart Rate Service UUID */
#define BLE_HRS_UUID16                                              0x180D

/* 16 Bit Heart Rate Service Characteristic UUIDs */
#define BLE_HRS_CHR_UUID16_MEASUREMENT                              0x2A37
#define BLE_HRS_CHR_UUID16_BODY_SENSOR_LOC                          0x2A38
#define BLE_HRS_CHR_UUID16_HEART_RATE_CNTL_POINT                    0x2A39

/* The maximum number of RR-Interval Values */
#define BLE_HRS_CHR_MERSUREMENT_RR_INTERVAL_MAX_NUM                     7

/**
 * @brief   Heart Rate Measurement Characteristic
 */
typedef struct {
    struct {
        uint8_t format: 1;                                              /*!< Heart rate value format flag */
        uint8_t detected: 1;                                            /*!< Sensor contact detected flag */
        uint8_t supported: 1;                                           /*!< Sensor contact cupported flag */
        uint8_t energy: 1;                                              /*!< Energy expended present flag */
        uint8_t interval: 1;                                            /*!< RR-Interval present flag */
        uint8_t reserved: 3;                                            /*!< Reserved for future use flag */
    } flags;                                                            /*!< Flags of heart rate measurement */

    union {
        uint8_t  u8;                                                    /*!< 8 bit resolution */
        uint16_t u16;                                                   /*!< 16 bit resolution */
    } heartrate;                                                        /*!< Heart rate measurement value */

    uint16_t energy_val;                                                /*!< Expended energy value */
    uint16_t interval_buf[BLE_HRS_CHR_MERSUREMENT_RR_INTERVAL_MAX_NUM]; /*!< The RR-Interval value represents the time between two R-Wave detections */
} __attribute__((packed)) esp_ble_hrs_hrm_t;

/* Body Sensor Location Characteristic */
#define BLE_HRS_CHR_BODY_SENSOR_LOC_OTHER                               0
#define BLE_HRS_CHR_BODY_SENSOR_LOC_CHEST                               1
#define BLE_HRS_CHR_BODY_SENSOR_LOC_WRIST                               2
#define BLE_HRS_CHR_BODY_SENSOR_LOC_FINGER                              3
#define BLE_HRS_CHR_BODY_SENSOR_LOC_HAND                                4
#define BLE_HRS_CHR_BODY_SENSOR_LOC_EAR_LOBE                            5
#define BLE_HRS_CHR_BODY_SENSOR_LOC_FOOT                                6
#define BLE_HRS_CHR_BODY_SENSOR_LOC_RFU                                 7
#define BLE_HRS_CHR_BODY_SENSOR_LOC_MAX                                 8

/**
 * @brief Get the sensor location value of the device.
 *
 * @param[in]  location The pointer to store the sensor location value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong location
 */
esp_err_t esp_ble_hrs_get_location(uint8_t *location);

/**
 * @brief Set the sensor location value of the device.
 *
 * @param[in]  location The sensor location value
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong location
 */
esp_err_t esp_ble_hrs_set_location(uint8_t location);

/**
 * @brief Get the value of the heart rate measurement of the device.
 *
 * @param[in]  hrm The pointer to store the value of the heart rate measurement
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong heart rate measurement
 */
esp_err_t esp_ble_hrs_get_hrm(esp_ble_hrs_hrm_t *hrm);

/**
 * @brief Set the value of the heart rate measurement of the device.
 *
 * @param[in]  hrm The pointer to store the value of the heart rate measurement
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong heart rate measurement
 */
esp_err_t esp_ble_hrs_set_hrm(esp_ble_hrs_hrm_t *hrm);

/**
 * @brief Initialization Heart Rate Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_hrs_init(void);

#ifdef __cplusplus
}
#endif
