/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// About detailed definitions of the struct in See https://bthome.io/format/

/**
 * @brief bthome handle type
 */
typedef struct bthome_t *bthome_handle_t;

/**
 * @brief BTHome sensor ID
 */
typedef enum {
    BTHOME_SENSOR_ID_PACKET             = 0x00,
    BTHOME_SENSOR_ID_BATTERY            = 0x01,
    BTHOME_SENSOR_ID_TEMPERATURE_PRECISE = 0x02,
    BTHOME_SENSOR_ID_HUMIDITY_PRECISE   = 0x03,
    BTHOME_SENSOR_ID_PRESSURE           = 0x04,
    BTHOME_SENSOR_ID_ILLUMINANCE        = 0x05,
    BTHOME_SENSOR_ID_MASS               = 0x06,
    BTHOME_SENSOR_ID_MASSLB             = 0x07,
    BTHOME_SENSOR_ID_DEWPOINT           = 0x08,
    BTHOME_SENSOR_ID_COUNT              = 0x09,
    BTHOME_SENSOR_ID_ENERGY             = 0x0A,
    BTHOME_SENSOR_ID_POWER              = 0x0B,
    BTHOME_SENSOR_ID_VOLTAGE            = 0x0C,
    BTHOME_SENSOR_ID_PM25               = 0x0D,
    BTHOME_SENSOR_ID_PM10               = 0x0E,
    BTHOME_SENSOR_ID_CO2                = 0x12,
    BTHOME_SENSOR_ID_TVOC               = 0x13,
    BTHOME_SENSOR_ID_MOISTURE_PRECISE   = 0x14,
    BTHOME_SENSOR_ID_HUMIDITY           = 0x2E,
    BTHOME_SENSOR_ID_MOISTURE           = 0x2F,
    BTHOME_SENSOR_ID_COUNT2             = 0x3D,
    BTHOME_SENSOR_ID_COUNT4             = 0x3E,
    BTHOME_SENSOR_ID_ROTATION           = 0x3F,
    BTHOME_SENSOR_ID_DISTANCE           = 0x40,
    BTHOME_SENSOR_ID_DISTANCEM          = 0x41,
    BTHOME_SENSOR_ID_DURATION           = 0x42,
    BTHOME_SENSOR_ID_CURRENT            = 0x43,
    BTHOME_SENSOR_ID_SPD                = 0x44,
    BTHOME_SENSOR_ID_TEMPERATURE        = 0x45,
    BTHOME_SENSOR_ID_UV                 = 0x46,
    BTHOME_SENSOR_ID_VOLUME1            = 0x47,
    BTHOME_SENSOR_ID_VOLUME2            = 0x48,
    BTHOME_SENSOR_ID_VOLUMEFR           = 0x49,
    BTHOME_SENSOR_ID_VOLTAGE1           = 0x4A,
    BTHOME_SENSOR_ID_GAS                = 0x4B,
    BTHOME_SENSOR_ID_GAS4               = 0x4C,
    BTHOME_SENSOR_ID_ENERGY4            = 0x4D,
    BTHOME_SENSOR_ID_VOLUME             = 0x4E,
    BTHOME_SENSOR_ID_WATER              = 0x4F,
    BTHOME_SENSOR_ID_TIMESTAMP          = 0x50,
    BTHOME_SENSOR_ID_ACCELERATION       = 0x51,
    BTHOME_SENSOR_ID_GYROSCOPE          = 0x52,
    BTHOME_SENSOR_ID_TEXT               = 0x53,
    BTHOME_SENSOR_ID_RAW                = 0x54,
    BTHOME_SENSOR_ID_VOLUME_STORAGE     = 0x55,
} __attribute__((packed)) bthome_sensor_id_t;

/**
 * @brief BTHome binary sensor ID
 */
typedef enum {
    BTHOME_BIN_SENSOR_ID_GENERIC             = 0x0F,
    BTHOME_BIN_SENSOR_ID_POWER               = 0x10,
    BTHOME_BIN_SENSOR_ID_OPENING             = 0x11,
    BTHOME_BIN_SENSOR_ID_BATTERY             = 0x15,
    BTHOME_BIN_SENSOR_ID_BATTERY_CHARGING    = 0x16,
    BTHOME_BIN_SENSOR_ID_CARBON_MONOXIDE     = 0x17,
    BTHOME_BIN_SENSOR_ID_COLD                = 0x18,
    BTHOME_BIN_SENSOR_ID_CONNECTIVITY        = 0x19,
    BTHOME_BIN_SENSOR_ID_DOOR                = 0x1A,
    BTHOME_BIN_SENSOR_ID_GARAGE_DOOR         = 0x1B,
    BTHOME_BIN_SENSOR_ID_GAS                 = 0x1C,
    BTHOME_BIN_SENSOR_ID_HEAT                = 0x1D,
    BTHOME_BIN_SENSOR_ID_LIGHT               = 0x1E,
    BTHOME_BIN_SENSOR_ID_LOCK                = 0x1F,
    BTHOME_BIN_SENSOR_ID_MOISTURE            = 0x20,
    BTHOME_BIN_SENSOR_ID_MOTION              = 0x21,
    BTHOME_BIN_SENSOR_ID_MOVING              = 0x22,
    BTHOME_BIN_SENSOR_ID_OCCUPANCY           = 0x23,
    BTHOME_BIN_SENSOR_ID_PLUG                = 0x24,
    BTHOME_BIN_SENSOR_ID_PRESENCE            = 0x25,
    BTHOME_BIN_SENSOR_ID_PROBLEM             = 0x26,
    BTHOME_BIN_SENSOR_ID_RUNNING             = 0x27,
    BTHOME_BIN_SENSOR_ID_SAFETY              = 0x28,
    BTHOME_BIN_SENSOR_ID_SMOKE               = 0x29,
    BTHOME_BIN_SENSOR_ID_SOUND               = 0x2A,
    BTHOME_BIN_SENSOR_ID_TAMPER              = 0x2B,
    BTHOME_BIN_SENSOR_ID_VIBRATION           = 0x2C,
    BTHOME_BIN_SENSOR_ID_WINDOW              = 0x2D,
} __attribute__((packed))bthome_bin_sensor_id_t;

/**
 * @brief BTHome event ID
 */
typedef enum {
    BTHOME_EVENT_ID_BUTTON    = 0x3A,
    BTHOME_EVENT_ID_DIMMER    = 0x3C,
} __attribute__((packed)) bthome_event_id_t;

#define BTHOME_REPORTS_MAX 10

/**
 * @brief BTHome device info
 */
typedef union {
    struct __attribute__((packed)) {
        uint8_t encryption_flag        : 1;  /**< bit 0: Encryption flag */
        uint8_t reserved1              : 1;  /**< bit 1: Reserved for future use */
        uint8_t trigger_based_flag     : 1;  /**< bit 2: Trigger based device flag */
        uint8_t reserved2              : 2;  /**< bit 3-4: Reserved for future use */
        uint8_t bthome_version         : 3;  /**< bit 5-7: BTHome Version */
    } bit;  /**< Bit field representation of device info */
    uint8_t all;  /**< Byte representation of device info */
} bthome_device_info_t;

/**
 * @brief BTHome report structure
 */
typedef struct {
    uint8_t id;    /**< Object ID: One of the BTHome Sensor ID, Binary Sensor ID or Event ID */
    uint8_t len;   /**< Length of the data */
    uint8_t *data; /**< Pointer to the data */
} bthome_report_t;

/**
 * @brief BTHome reports structure
 */
typedef struct {
    uint8_t num_reports; /**< Number of reports in the array */
    bthome_report_t report[BTHOME_REPORTS_MAX]; /**< Array of reports */
} bthome_reports_t;

/**
 * @brief BTHome store function
 */
typedef void (*bthome_store_func_t)(bthome_handle_t handle, const char *key, const uint8_t *data, uint8_t len);

/**
 * @brief BTHome load function
 */
typedef void (*bthome_load_func_t)(bthome_handle_t handle, const char *key, uint8_t *data, uint8_t len);

/**
 * @brief BTHome callback functions
 */
typedef struct {
    bthome_store_func_t store; /**< Function pointer to store data */
    bthome_load_func_t  load;  /**< Function pointer to load data */
} bthome_callbacks_t;

/**
 * @brief Create a BTHome object
 *
 * @param handle pointer to the bthome handle
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_create(bthome_handle_t *handle);

/**
 * @brief Delete a BTHome object
 *
 * @param handle pointer to the bthome handle
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_delete(bthome_handle_t handle);

/**
 * @brief set the encryption key used for encryption and decryption
 *
 * @param handle bthome handle
 * @param key pointer to the key
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_set_encrypt_key(bthome_handle_t handle, const uint8_t *key);

/**
 * @brief set the local mac address used for encryption
 *
 * @param handle bthome handle
 * @param mac   pointer to the local ble mac address
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_set_local_mac_addr(bthome_handle_t handle, uint8_t *mac);

/**
 * @brief set the peer mac address used for decryption
 *
 * @param handle bthome handle
 * @param mac  pointer to the peer ble mac address
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_set_peer_mac_addr(bthome_handle_t handle, const uint8_t *mac);

/**
 * @brief register the callback function
 *
 * @param handle pointer to the bthome handle
 * @param callbacks pointer to the callback structure
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_register_callbacks(bthome_handle_t handle, bthome_callbacks_t *callbacks);

/**
 * @brief load the parameters from the storage
 *
 * @param handle pointer to the bthome handle
 * @return esp_err_t
 *      ESP_OK: success
 *      others: fail
 */
esp_err_t bthome_load_params(bthome_handle_t handle);

/**
 * @brief free the reports structure
 *
 * @param reports pointer to the reports need to be freed
 */
void bthome_free_reports(bthome_reports_t *reports);

/**
 * @brief Add sensor data to the payload
 *
 * @param buffer pointer to the buffer
 * @param offset current offset of the buffer
 * @param obj_id sensor ID
 * @param data pointer to the data
 * @param data_len length of the data
 * @return length of the data added to the buffer
 */
uint8_t bthome_payload_add_sensor_data(uint8_t *buffer, uint8_t offset, bthome_sensor_id_t obj_id, uint8_t *data, uint8_t data_len);

/**
 * @brief Add binary sensor data to the payload
 *
 * @param buffer pointer to the buffer
 * @param offset current offset of the buffer
 * @param obj_id binary sensor ID
 * @param data data
 * @return length of the data added to the buffer
 */
uint8_t bthome_payload_adv_add_bin_sensor_data(uint8_t *buffer, uint8_t offset, bthome_bin_sensor_id_t obj_id, uint8_t data);

/**
 * @brief Add event data to the payload
 *
 * @param buffer pointer to the buffer
 * @param offset current offset of the buffer
 * @param obj_id event ID
 * @param evt_size event size
 * @param evt event value
 * @return length of the data added to the buffer
 */
uint8_t bthome_payload_adv_add_evt_data(uint8_t *buffer, uint8_t offset, bthome_event_id_t obj_id, uint8_t *evt, uint8_t evt_size);

/**
 * @brief Parse the advertisement data
 *
 * @param handle bthome handle
 * @param adv pointer to the advertisement data
 * @param len length of the advertisement data
 * @return bthome_reports_t *
 *          pointer to the reports structure
 *          NULL if the parsing fails
 */
bthome_reports_t *bthome_parse_adv_data(bthome_handle_t handle, uint8_t *adv, uint8_t len);

/**
 * @brief Create a BTHome advertisement data
 *
 * @param handle bthome handle
 * @param buffer advertisement data buffer
 * @param name advertisement name
 * @param name_len length of the name
 * @param info bthome device info
 * @param payload pointer to the payload
 * @param payload_len length of the payload
 * @return length of the advertisement data
 */
uint8_t bthome_make_adv_data(bthome_handle_t handle, uint8_t *buffer, uint8_t *name, uint8_t name_len, bthome_device_info_t info, uint8_t *payload, uint8_t payload_len);

#ifdef __cplusplus
}
#endif
