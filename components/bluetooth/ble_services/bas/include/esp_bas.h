/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* 16 Bit Battery Service UUID */
#define BLE_BAS_UUID16                                                          0x180F

/* 16 Bit Battery Service Characteristic UUIDs */
#define BLE_BAS_CHR_UUID16_LEVEL                                                0x2A19
#define BLE_BAS_CHR_UUID16_LEVEL_STATUS                                         0x2BED
#define BLE_BAS_CHR_UUID16_ESTIMATED_SERVICE_DATE                               0x2BEF
#define BLE_BAS_CHR_UUID16_CRITICAL_STATUS                                      0x2BE9
#define BLE_BAS_CHR_UUID16_ENERGY_STATUS                                        0x2BF0
#define BLE_BAS_CHR_UUID16_TIME_STATUS                                          0x2BEE
#define BLE_BAS_CHR_UUID16_HEALTH_STATUS                                        0x2BEA
#define BLE_BAS_CHR_UUID16_HEALTH_INFO                                          0x2BEB
#define BLE_BAS_CHR_UUID16_BATTERY_INFO                                         0x2BEC

/**
 * @brief   This structure represents a 24bits data type.
 */
typedef struct {
    uint32_t u24: 24;                                                               /*!< A 24bits data */
} __attribute__((packed)) uint24_t;

/* Battery Service Level Status Flags Bit Masks */
#define BAS_CHR_LEVEL_STATUS_FLAGS_BM_NONE                                          0x00
#define BAS_CHR_LEVEL_STATUS_FLAGS_BM_IDENTIFY                                      0x01
#define BAS_CHR_LEVEL_STATUS_FLAGS_BM_BATTERY_LEVEL                                 0x02
#define BAS_CHR_LEVEL_STATUS_FLAGS_BM_ADDITIONAL_STATUS                             0x04
#define BAS_CHR_LEVEL_STATUS_FLAGS_BM_RFU                                           0xF8

/* Battery Service Level Status Power State Bit Masks */

/* Battery Present in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_NOT                                0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_SET                                1

/* Wired External Power Source Connected in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRED_EXTERNAL_POWER_SOURCE_NOTCONNECT     0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRED_EXTERNAL_POWER_SOURCE_CONNECTED      1
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRED_EXTERNAL_POWER_SOURCE_UNKNOWN        2
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRED_EXTERNAL_POWER_SOURCE_RFU            3

/* Wireless External Power Source Connected in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRELESS_EXTERNAL_POWER_SOURCE_NOTCONNECT  0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRELESS_EXTERNAL_POWER_SOURCE_CONNECTED   1
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRELESS_EXTERNAL_POWER_SOURCE_UNKNOWN     2
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_WIRELESS_EXTERNAL_POWER_SOURCE_RFU         3

/* Battery Charge State in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_STATE_UNKNOWN               0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_STATE_CHARGING              1
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_STATE_DISCHARGING_ACTIVE    2
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_STATE_DISCHARGING_INACTIVE  3

/* Battery Charge Level in Power State Filed */;
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_LEVEL_UNKNOWN               0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_LEVEL_GOOD                  1
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_LEVEL_LOW                   2
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_BATTERY_CHARGE_LEVEL_CRITICAL              3

/* Charging Type in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_UNKNOWN                        0
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_CURRENT                        1
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_VOLTAGE                        2
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_TRICKLE                        3
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_FLOAT                          4
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_TYPE_RFU                            5

/* Charging Fault Reason in Power State Filed */
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_FAULT_NONE                          0x00
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_FAULT_BATTERY                       0x01
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_FAULT_EXTERNAL_POWER_SOURCE         0x02
#define BAS_CHR_LEVEL_STATUS_POWER_STATE_CHARGE_FAULT_OTHER                         0x04

/* Battery Service Level Status Additional Status Bit Masks */
#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_SERVICE_FALSE                        0
#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_SERVICE_TRUE                         1
#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_SERVICE_UNKNOWN                      2
#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_SERVICE_RFU                          3

#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_BATTERY_FAULT_UNKNOWN                0
#define BAS_CHR_LEVEL_STATUS_ASSITIONAL_STATUS_BATTERY_FAULT_TRUE                   1

/* Battery Service Battery Critical Status Flags Bits Masks */
#define BAS_CHR_CRITICAL_STATUS_FLAGS_BM_NONE                                       0x00
#define BAS_CHR_CRITICAL_STATUS_FLAGS_BM_CRITICAL_POWER_STATE                       0x01
#define BAS_CHR_CRITICAL_STATUS_FLAGS_BM_IMMEDIATE_SERVICE                          0x02

/* Battery Service Battery Energy Status Flags Bits Masks */
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_NONE                                         0x00
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_EXTERNAL_SOURCE_POWER                        0x01
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_VOLTAGE                                      0x02
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_AVAILALBE_ENERGY                             0x04
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_AVAILALBE_BATTERY_CAPACITY                   0x08
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_CHARGE_RATE                                  0x10
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_AVAILALBE_ENERGY_LAST_CHARGE                 0x20
#define BAS_CHR_ENERGY_STATUS_FLAGS_BM_RFU                                          0xC0

/* Battery Service Battery Time Status Flags Bits Masks */
#define BAS_CHR_TIME_STATUS_FLAGS_BM_NONE                                           0x00
#define BAS_CHR_TIME_STATUS_FLAGS_BM_DISCHARGED_STANDBY                             0x01
#define BAS_CHR_TIME_STATUS_FLAGS_BM_RECHARGE                                       0x02
#define BAS_CHR_TIME_STATUS_FLAGS_BM_RFU                                            0xF4

/* Battery Service Battery Health Status Flags Bits Masks */
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_NONE                                         0x00
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_BATTERY_HEALTH_SUMMARY                       0x01
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_RCYCLE_COUNT                                 0x02
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_CURRENT_TEMPERATURE                          0x04
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_DEEP_DISCHARGE_COUNT                         0x08
#define BAS_CHR_HEALTH_STATUS_FLAGS_BM_RFU                                          0xF0

/* Battery Service Battery Health Information Flags Bits Masks */
#define BAS_CHR_HEALTH_INFO_FLAGS_BM_NONE                                           0x00
#define BAS_CHR_HEALTH_INFO_FLAGS_BM_CYCLE_COUNT_DESIGNED_LIFETIME                  0x01
#define BAS_CHR_HEALTH_INFO_FLAGS_BM_DESIGNED_OPERATING_TEMPERATURE                 0x02
#define BAS_CHR_HEALTH_INFO_FLAGS_BM_RFU                                            0xFC

/* Battery Service Battery Information Flags Bits Masks */
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_NONE                                          0x0000
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_MANUFACTURE_DATE                              0x0001
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_EXPIRATION_DATE                               0x0002
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_DESIGNED_CAPACITY                             0x0004
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_LOW_ENERGY                                    0x0008
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_CRITICAL_ENERGY                               0x0010
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_CHEMISTRY                                     0x0020
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_NOMINAL_VOLTAGE                               0x0040
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_AGGREGATION_GROUP                             0x0080
#define BAS_CHR_BATTERY_INFO_FLAGS_BM_RFU                                           0xFF00

/* Battery Service Battery Information Feature Bits Masks */
#define BAS_CHR_BATTERY_INFO_FEATURE_BM_NONE                                        0x00
#define BAS_CHR_BATTERY_INFO_FEATURE_BM_REPLACE                                     0x01
#define BAS_CHR_BATTERY_INFO_FEATURE_BM_RECHARGE                                    0x02
#define BAS_CHR_BATTERY_INFO_FEATURE_BM_RFU                                         0xF4

/**
 * @brief   Battery Level Status Characteristic
 */
typedef struct {
    struct {
        uint8_t en_identifier: 1;                                                   /*!< Identifier Present */
        uint8_t en_battery_level: 1;                                                /*!< Battery Level Present */
        uint8_t en_additional_status: 1;                                            /*!< Additional Status Present */
        uint8_t flags_reserved: 5;                                                  /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Level Status */

    struct {
        uint16_t battery: 1;                                                        /*!< Battery Present */
        uint16_t wired_external_power_source: 2;                                    /*!< Wired External Power Source Connected: */
        uint16_t wireless_external_power_source: 2;                                 /*!< Wireless External Power Source Connected: */
        uint16_t battery_charge_state: 2;                                           /*!< Battery Charge State: */
        uint16_t battery_charge_level: 2;                                           /*!< Battery Charge Level: */
        uint16_t battery_charge_type: 3;                                            /*!< Battery Charging Type */
        uint16_t charging_fault_reason: 3;                                          /*!< Charging Fault Reason */
        uint16_t power_state_reserved: 1;                                           /*!< Reserve Feature Used */
    } power_state;                                                                  /*!< Power State of Battery Level Status */

    uint16_t identifier;                                                            /*!< Used as an identifier for a service instance. */
    uint8_t  battery_level;                                                         /*!< Refer to the Battery Level */

    struct {
        uint8_t service_required: 2;                                                /*!< Service Required */
        uint8_t battery_fault: 1;                                                   /*!< Battery Fault: */
        uint8_t reserved: 5;                                                        /*!< Reserve Feature Used */
    } additional_status;                                                            /*!< Contains additional status information such as whether or not service is required */
} __attribute__((packed)) esp_ble_bas_level_status_t;

/**
 * @brief   Battery Critical Status Characteristic
 */
typedef struct {
    struct {
        uint8_t critical_power_state: 1;                                            /*!< Critical Power State */
        uint8_t immediate_service: 1;                                               /*!< Immediate Service Required */
        uint8_t reserved: 6;                                                        /*!< Reserve Feature Used */
    } status;                                                                       /*!< Battery Critical Status */
} __attribute__((packed)) esp_ble_bas_critical_status_t;

/**
 * @brief   Battery Energy Status Characteristic
 */
typedef struct {
    struct {
        uint8_t en_external_source_power: 1;                                        /*!< External Source Power Present */
        uint8_t en_voltage: 1;                                                      /*!< Present Voltage Present */
        uint8_t en_available_energy: 1;                                             /*!< Available Energy Present */
        uint8_t en_available_battery_capacity: 1;                                   /*!< Available Battery Capacity Present */
        uint8_t en_charge_rate: 1;                                                  /*!< Charge Rate Present */
        uint8_t en_available_energy_last_charge: 1;                                 /*!< Available Energy at Last Charge Present */
        uint8_t reserved: 2;                                                        /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Energy Status */

    uint16_t external_source_power;                                                 /*!< The total power being consumed from an external power source */
    uint16_t voltage;                                                               /*!< The present terminal voltage of the battery in volts. */
    uint16_t available_energy;                                                      /*!< The available energy of the battery in kilowatt-hours in its current charge state */
    uint16_t available_battery_capacity;                                            /*!< The capacity of the battery in kilowatt-hours at full charge in its current condition */
    uint16_t charge_rate;                                                           /*!< The energy flowing into the battery in watts */
    uint16_t available_energy_last_charge;                                          /*!< The available energy of the battery in kilowatt-hours in its last charge state */
} __attribute__((packed)) esp_ble_bas_energy_status_t;

/**
 * @brief   Battery Time Status Characteristic
 */
typedef struct {
    struct {
        uint8_t en_discharged_standby: 1;                                           /*!< Time until Discharged on Standby Present */
        uint8_t en_recharged: 1;                                                    /*!< Time until Recharged Present */
        uint8_t reserved: 6;                                                        /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Time Status */

    uint24_t   discharged;                                                          /*!< Estimated time in minutes until discharged */

    uint24_t   discharged_standby;                                                  /*!< Estimated time in minutes until discharged assuming for the remaining time the device is in standby. */
    uint24_t   recharged;                                                           /*!< Estimated time in minutes until recharged */
} __attribute__((packed)) esp_ble_bas_time_status_t;

/**
 * @brief   Battery Health Status Characteristic
 */
typedef struct {
    struct {
        uint8_t en_battery_health_summary: 1;                                       /*!< Battery Health Summary Present */
        uint8_t en_cycle_count: 1;                                                  /*!< Cycle Count Present */
        uint8_t en_current_temperature: 1;                                          /*!< Current Temperature Present */
        uint8_t en_deep_discharge_count: 1;                                         /*!< Deep Discharge Count Present */
        uint8_t reserved: 4;                                                        /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Health Status */

    uint8_t battery_health_summary;                                                 /*!< Represents aggregation of the overall health of the battery */
    uint16_t cycle_count;                                                           /*!< Represents the count value of charge cycles */
    int8_t  current_temperature;                                                    /*!< Represents the current temperature of the battery */
    uint16_t deep_discharge_count;                                                  /*!< Represents the number of times the battery was completely discharged */
} __attribute__((packed)) esp_ble_bas_health_status_t;

/**
 * @brief   Battery Health Information Characteristic
 */
typedef struct {
    struct {
        uint8_t en_cycle_count_designed_lifetime: 1;                                /*!< Cycle Count Designed Lifetime Present */
        uint8_t min_max_designed_operating_temperature: 1;                          /*!< Min and Max Designed Operating Temperature Present */
        uint8_t reserved: 5;                                                        /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Health Information */

    uint16_t cycle_count_designed_lifetime;                                         /*!< Represents the designed number of charge cycles supported by the device */
    int8_t   min_designed_operating_temperature;                                    /*!< Represents the minimum designed operating temperature of the battery */
    int8_t   max_designed_operating_temperature;                                    /*!< Represents the maximum designed operating temperature of the battery */
} __attribute__((packed)) esp_ble_bas_health_info_t;

/**
 * @brief   Battery Information Characteristic
 */
typedef struct {
    struct {
        uint16_t en_manufacture_date: 1;                                            /*!< Battery Manufacture Date Present */
        uint16_t en_expiration_date: 1;                                             /*!< Battery Expiration Date Present */
        uint16_t en_designed_capacity: 1;                                           /*!< Battery Designed Capacity Present */
        uint16_t en_low_energy: 1;                                                  /*!< Battery Low Energy Present */
        uint16_t en_critical_energy: 1;                                             /*!< Battery Critical Energy Present */
        uint16_t en_chemistry: 1;                                                   /*!< Battery Chemistry Present */
        uint16_t en_nominalvoltage: 1;                                              /*!< Nominal Voltage Present */
        uint16_t en_aggregation_group: 1;                                           /*!< Battery Aggregation Group Present */
        uint16_t flags_reserved: 8;                                                 /*!< Reserve Feature Used */
    } flags;                                                                        /*!< Flags of Battery Information */

    struct {
        uint8_t replace_able: 1;                                                    /*!< Battery Replaceable */
        uint8_t recharge_able: 1;                                                   /*!< Battery Rechargeable */
        uint8_t reserved: 7;                                                        /*!< Reserve Feature Used */
    } features;                                                                     /*!< Features of Battery Information */

    uint24_t manufacture_date;                                                      /*!< Battery date of manufacture specified as days */
    uint24_t expiration_date;                                                       /*!< Battery expiration date specified as days */
    uint16_t designed_capacity;                                                     /*!< The capacity of the battery in kilowatt-hours at full charge in original (new) condition. */
    uint16_t low_energy;                                                            /*!< The battery energy value in kilowatt-hours when the battery is low */
    uint16_t critical_energy;                                                       /*!< The battery energy value in kilowatt-hours when the battery is critical. */
    uint8_t  chemistry;                                                             /*!< The value of battery chemistry */
    uint16_t nominalvoltage;                                                        /*!< Nominal voltage of the battery in units of volts */
    uint8_t  aggregation_group;                                                     /*!< Indicates the Battery Aggregation Group to which this instance of the battery service is associated */
} __attribute__((packed)) esp_ble_bas_battery_info_t;

/**
 * @brief   Battery Service
 */
typedef struct {
    uint8_t                             battery_level;                              /*!< The charge level of a battery */
    esp_ble_bas_level_status_t          level_status;                               /*!< The power state of a battery */
    uint24_t                            estimated_service_date;                     /*!< The estimated date when replacement or servicing is required. */
    esp_ble_bas_critical_status_t       critical_status;                            /*!< The device will possibly not function as expected due to low energy or service required */
    esp_ble_bas_energy_status_t         energy_status;                              /*!< Details about the energy status of the battery */
    esp_ble_bas_time_status_t           time_status;                                /*!< Time estimates for discharging and charging */
    esp_ble_bas_health_status_t         health_status;                              /*!< Several aspects of battery health */
    esp_ble_bas_health_info_t           health_info;                                /*!< The health of a battery */
    esp_ble_bas_battery_info_t          battery_info;                               /*!< The physical characteristics of a battery in the context of the battery’s connection in a device */
} __attribute__((packed)) esp_ble_bas_data_t;

/**
 * @brief Get the current battery level of the device.
 *
 * @param[in]  level The pointer to store the current battery level
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery level
 */
esp_err_t esp_ble_bas_get_battery_level(uint8_t *level);

/**
 * @brief Set the current battery level of the device.
 *
 * @param[in]  level The pointer to store the current battery level
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery level
 */
esp_err_t esp_ble_bas_set_battery_level(uint8_t *level);

/**
 * @brief Get the summary information related to the battery status of the device.
 *
 * @param[in]  status The pointer to store the the summary information related to the battery status
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery status
 */
esp_err_t esp_ble_bas_get_level_status(esp_ble_bas_level_status_t *status);

/**
 * @brief Set the summary information related to the battery status of the device.
 *
 * @param[in]  status The pointer to store the the summary information related to the battery status
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery status
 */
esp_err_t esp_ble_bas_set_level_status(esp_ble_bas_level_status_t *status);

/**
 * @brief Get the current estimated date when the battery is likely to require service or replacement.
 *
 * @param[in]  estimated_date The pointer to store the current estimated date
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery estimated date
 */
esp_err_t esp_ble_bas_get_estimated_date(uint32_t *estimated_date);

/**
 * @brief Set the current estimated date when the battery is likely to require service or replacement.
 *
 * @param[in]  estimated_date The pointer to store the current estimated date
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery estimated date
 */
esp_err_t esp_ble_bas_set_estimated_date(uint32_t *estimated_date);

/**
 * @brief Get the status bits related to potential battery malfunction.
 *
 * @param[in]  status The pointer to store the status bits related to potential battery malfunction
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery malfunction
 */
esp_err_t esp_ble_bas_get_critical_status(esp_ble_bas_critical_status_t *status);

/**
 * @brief Set the status bits related to potential battery malfunction.
 *
 * @param[in]  status The pointer to store the status bits related to potential battery malfunction
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery malfunction
 */
esp_err_t esp_ble_bas_set_critical_status(esp_ble_bas_critical_status_t *status);

/**
 * @brief Get the current energy status details of the battery.
 *
 * @param[in]  status The pointer to store the status bits related to potential battery energy
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery energy
 */
esp_err_t esp_ble_bas_get_energy_status(esp_ble_bas_energy_status_t *status);

/**
 * @brief Set the current energy status details of the battery.
 *
 * @param[in]  status The pointer to store the status bits related to potential battery energy
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery energy
 */
esp_err_t esp_ble_bas_set_energy_status(esp_ble_bas_energy_status_t *status);

/**
 * @brief Get the current estimates on times for discharging and charging.
 *
 * @param[in]  status The pointer to store the current estimates on times
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery estimates
 */
esp_err_t esp_ble_bas_get_time_status(esp_ble_bas_time_status_t *status);

/**
 * @brief Set the current estimates on times for discharging and charging.
 *
 * @param[in]  status The pointer to store the current estimates on times
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery estimates
 */
esp_err_t esp_ble_bas_set_time_status(esp_ble_bas_time_status_t *status);

/**
 * @brief Get the aspects of battery health.
 *
 * @param[in]  status The pointer to store the aspects of battery health
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery health
 */
esp_err_t esp_ble_bas_get_health_status(esp_ble_bas_health_status_t *status);

/**
 * @brief Set the aspects of battery health.
 *
 * @param[in]  status The pointer to store the aspects of battery health
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery health
 */
esp_err_t esp_ble_bas_set_health_status(esp_ble_bas_health_status_t *status);

/**
 * @brief Get the static aspects of battery health.
 *
 * @param[in]  info The pointer to store the static aspects of battery health
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery health information
 */
esp_err_t esp_ble_bas_get_health_info(esp_ble_bas_health_info_t *info);

/**
 * @brief Set the static aspects of battery health.
 *
 * @param[in]  info The pointer to store the static aspects of battery health
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery health information
 */
esp_err_t esp_ble_bas_set_health_info(esp_ble_bas_health_info_t *info);

/**
 * @brief Get the physical characteristics of the battery.
 *
 * @param[in]  info The pointer to store the physical characteristics of the battery
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery information
 */
esp_err_t esp_ble_bas_get_battery_info(esp_ble_bas_battery_info_t *info);

/**
 * @brief Set the physical characteristics of the battery.
 *
 * @param[in]  info The pointer to store the physical characteristics of the battery
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong battery information
 */
esp_err_t esp_ble_bas_set_battery_info(esp_ble_bas_battery_info_t *info);

/**
 * @brief Initialization Battery Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_bas_init(void);

#ifdef __cplusplus
}
#endif
