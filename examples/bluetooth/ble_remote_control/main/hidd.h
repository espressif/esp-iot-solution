/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h> // memcpy

#include "esp_err.h"
#include "esp_log.h"

#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"

#define BD_ADDR_2_STR(x) "Remote BD Addr %08x%04x", \
    (x[0] << 24) + (x[1] << 16) + (x[2] << 8) + x[3], (x[4] << 8) + x[5]

#define HID_LATENCY 8   // 8ms report rate at minimum

/**
 * Reference: Bluetooth HID over GATT Profile Specification (HOGP)
 * HID Profile includes the following services and mandatory characteristics:
 * - HID Service                0x1812
 *      - HID Information Characteristic    0x2A4A
 *      - Report Map Characteristic         0x2A4B
 *      - HID Control Point Characteristic  0x2A4C
 *      - Report Characteristic             0x2A4D
 * - Battery Service            0x180F
 *      - Battery Level Characteristic      0x2A19
 * - Device Information Service 0x180A
 *      - Manufacturer Name String Characteristic   0x2A29
 *      - PnP ID Characteristic                     0x2A50
 * - Scan Parameters Service (optional, not included in this example)
 *
 * The UUIDs are defined in esp_gatt_defs.h
 * Refer to the individual service specs for more details at Bluetooth.org
*/

// Declared as variables to obtain pointers for attribute table registration
// UUIDs of Services and Characteristics
static const uint16_t UUID_PRI_SVC = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECLARE = ESP_GATT_UUID_CHAR_DECLARE;

static const uint16_t UUID_BAT_SVC = ESP_GATT_UUID_BATTERY_SERVICE_SVC;
static const uint16_t UUID_BAT_LVL_VAL = ESP_GATT_UUID_BATTERY_LEVEL;

static const uint16_t UUID_DEV_INFO_SVC = ESP_GATT_UUID_DEVICE_INFO_SVC;
static const uint16_t UUID_MANU_NAME_VAL = ESP_GATT_UUID_MANU_NAME;
static const uint16_t UUID_PNP_ID_VAL = ESP_GATT_UUID_PNP_ID;

static const uint16_t UUID_HID_SVC = ESP_GATT_UUID_HID_SVC;
static const uint16_t UUID_HID_INFO_VAL = ESP_GATT_UUID_HID_INFORMATION;
static const uint16_t UUID_HID_REPORT_MAP_VAL = ESP_GATT_UUID_HID_REPORT_MAP;
static const uint16_t UUID_HID_CTNL_PT_VAL = ESP_GATT_UUID_HID_CONTROL_POINT;
static const uint16_t UUID_HID_REPORT_VAL = ESP_GATT_UUID_HID_REPORT;
static const uint16_t UUID_HID_CLIENT_CONFIG_DESCR = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint16_t UUID_HID_EXT_REPORT_REF_DESCR = ESP_GATT_UUID_EXT_RPT_REF_DESCR;
static const uint16_t UUID_HID_REPORT_REF_DESCR = ESP_GATT_UUID_RPT_REF_DESCR;

// Characteristic Properties
static const uint8_t CHAR_PROP_READ = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t CHAR_PROP_READ_NOTIFY = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t CHAR_PROP_READ_WRITE = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

// Instance ID of Services to differentiate during GATTS events
enum {
    INST_ID_HID,
    INST_ID_BAT_SVC,
    INST_ID_DEV_INFO,
};

enum { // Battery Service
    IDX_BAT_SVC,

    // Battery Level Characteristic, Value & Client Characteristic Configuration Descriptor
    IDX_BAT_LVL_CHAR,
    IDX_BAT_LVL_VAL,
    IDX_BAT_LVL_DESCR_CCCD,

    LEN_BAT_SVC, // Total number of attributes in Battery Service
};

enum { // Device Information Service
    IDX_DEV_INFO_SVC,

    // Manufacturer Name Characteristic & Value
    IDX_DEV_INFO_MANUFACTURER_NAME_CHAR,
    IDX_DEV_INFO_MANUFACTURER_NAME_VAL,

    // PnP ID Characteristic & Value
    IDX_DEV_INFO_PNP_ID_CHAR,
    IDX_DEV_INFO_PNP_ID_VAL,

    LEN_DEV_INFO_SVC, // Total number of attributes in Device Information Service
};

enum { // HID Service
    IDX_HID_SVC,

    // HID Info Characteristic & Value
    IDX_HID_INFO_CHAR,
    IDX_HID_INFO_VAL,

    // Report Map Characteristic, Value & External Reference Descriptor
    IDX_HID_REPORT_MAP_CHAR,
    IDX_HID_REPORT_MAP_VAL,
    IDX_HID_REPORT_MAP_DESCR_EXT_REF,

    // HID Control Point Characteristic & Value
    IDX_HID_CTNL_PT_CHAR,
    IDX_HID_CTNL_PT_VAL,

    // (Input) Report Characteristic, Value, Descriptor
    IDX_HID_REPORT_CHAR,
    IDX_HID_REPORT_VAL,
    IDX_HID_REPORT_DESCR_CCCD, // Client Characteristic Configuration Descriptor
    IDX_HID_REPORT_DESCR_RPT_REF, // Report Reference Descriptor

    LEN_HID_SVC, // Total number of attributes in HID Service
};

/**
 * @brief Struct to store handles of attributes of services
 * @details Handles are needed to send notifications to clients
 * @details Simplified implementation to store attribute handles for the 3 services needed for HID Profile
*/
typedef struct {
    uint16_t handles_bat_svc[LEN_BAT_SVC];
    uint16_t handles_dev_info_svc[LEN_DEV_INFO_SVC];
    uint16_t handles_hid_svc[LEN_HID_SVC];
} handle_table_t;

/**
 * @brief Struct to store client information of connected devices
 * @details The information needed to send notifications to clients
*/
typedef struct {
    esp_gatt_if_t gatt_if;
    uint16_t connection_id;
    uint16_t attr_handle;
    uint16_t bat_attr_handle;
} client_info_t;

/**
 * @brief Callback function for HID Profile events (from GATTS callback)
 * @details Handles events related to HID Profile
 *
 * @param event Event type
 * @param gatts_if GATT Server Interface
 * @param param Event parameters
*/
void hid_event_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

/**
 * @brief Set the hid report values to be sent
 *
 * @note Values to be set depends on the HID Report Descriptor, specific to the device specification
 * @note Only the bottom 4 bits of buttons and hat_switch are used
*/
void set_hid_report_values(uint8_t joystick_x, uint8_t joystick_y, uint8_t buttons, uint8_t hat_switch, uint8_t throttle);

/**
 * @brief Send the HID Report to the client (notification)
*/
esp_err_t send_user_input(void);

/**
 * @brief Set the hid battery level value to be sent
 *
 * @note Values to be set depends on the device specification
 * @note The value is capped at 100
*/
esp_err_t set_hid_battery_level(uint8_t value);
