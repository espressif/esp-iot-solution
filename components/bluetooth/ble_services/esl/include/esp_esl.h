/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
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
/* BLE ESL EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_ESL_EVENTS);
/** @endcond **/

/* 16 Bit Electronic Shelf Label Service UUID */
#define BLE_ESL_UUID16                                                      0x1857

/* 16 Bit ESL Characteristic UUIDs */
#define BLE_ESL_CHR_UUID16_ADDRESS                                          0x2BF6
#define BLE_ESL_CHR_UUID16_AP_SYNC_KEY                                      0x2BF7
#define BLE_ESL_CHR_UUID16_RESP_KEY                                         0x2BF8
#define BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME                                 0x2BF9
#define BLE_ESL_CHR_UUID16_DISPLAY_INFO                                     0x2BFA
#define BLE_ESL_CHR_UUID16_IMAGE_INFO                                       0x2BFB
#define BLE_ESL_CHR_UUID16_SENSOR_INFO                                      0x2BFC
#define BLE_ESL_CHR_UUID16_LED_INFO                                         0x2BFD
#define BLE_ESL_CHR_UUID16_ECP                                              0x2BFE

/* ESL Size and Address Constants */
#define BLE_ESL_SESSION_KEY_SIZE                                            16
#define BLE_ESL_IV_SIZE                                                     8
#define BLE_ESL_KEY_MATERIAL_SIZE                                           24
#define BLE_ESL_DISPLAY_INFO_RECORD_SIZE                                    5
#define BLE_ESL_ECP_MAX_SIZE                                                17
#define BLE_ESL_PAYLOAD_MAX_SIZE                                            48    /*!< Max ESL Payload size in Synchronized / PAwR state */
#define BLE_ESL_BROADCAST_ADDRESS                                           0xFF
#define BLE_ESL_OBJECT_ID_BASE                                              0x000000000100ULL /*!< Object ID = base + Image_Index */

/* ESL Sensor Information Size field */
#define BLE_ESL_SENSOR_SIZE_SHORT                                           0x00  /*!< 16-bit Property ID */
#define BLE_ESL_SENSOR_SIZE_LONG                                            0x01  /*!< 32-bit vendor Sensor_Type */
#define BLE_ESL_SENSOR_SHORT_LEN                                            3
#define BLE_ESL_SENSOR_LONG_LEN                                             5

/* ESL LED Information type field in bits 7-6 */
#define BLE_ESL_LED_TYPE_SRGB                                               0x00
#define BLE_ESL_LED_TYPE_MONOCHROME                                         0x01
#define BLE_ESL_LED_TYPE(octet)                                             (((octet) >> 6) & 0x03)

/* ESL Control Point Command Op Codes */
#define BLE_ESL_CMD_PING                                                    0x00
#define BLE_ESL_CMD_UNASSOCIATE                                             0x01
#define BLE_ESL_CMD_SERVICE_RESET                                           0x02
#define BLE_ESL_CMD_FACTORY_RESET                                           0x03
#define BLE_ESL_CMD_UPDATE_COMPLETE                                         0x04
#define BLE_ESL_CMD_READ_SENSOR                                             0x10
#define BLE_ESL_CMD_REFRESH_DISPLAY                                         0x11
#define BLE_ESL_CMD_DISPLAY_IMAGE                                           0x20
#define BLE_ESL_CMD_DISPLAY_TIMED_IMAGE                                     0x60
#define BLE_ESL_CMD_LED_CONTROL                                             0xB0
#define BLE_ESL_CMD_LED_TIMED_CONTROL                                       0xF0

/* ESL Control Point Response Op Codes */
#define BLE_ESL_RESP_ERROR                                                  0x00
#define BLE_ESL_RESP_LED_STATE                                              0x01
#define BLE_ESL_RESP_SENSOR_VALUE                                           0x0E  /*!< Tag nibble; Length in MSN encodes Sensor_Data size */
#define BLE_ESL_RESP_VENDOR                                                 0x0F  /*!< Vendor-specific response Tag nibble */
#define BLE_ESL_RESP_BASIC_STATE                                            0x10
#define BLE_ESL_RESP_DISPLAY_STATE                                          0x11

/* ESL Control Point procedure timeout, seconds */
#define BLE_ESL_ECP_PROCEDURE_TIMEOUT_S                                     30

/* ESL Error Codes associated with Error Response */
#define BLE_ESL_ERR_UNSPECIFIED                                             0x01
#define BLE_ESL_ERR_INVALID_OPCODE                                          0x02
#define BLE_ESL_ERR_INVALID_STATE                                           0x03
#define BLE_ESL_ERR_INVALID_IMAGE_INDEX                                     0x04
#define BLE_ESL_ERR_IMAGE_NOT_AVAILABLE                                     0x05
#define BLE_ESL_ERR_INVALID_PARAMS                                          0x06
#define BLE_ESL_ERR_CAPACITY_LIMIT                                          0x07
#define BLE_ESL_ERR_INSUFFICIENT_BATTERY                                    0x08
#define BLE_ESL_ERR_INSUFFICIENT_RESOURCES                                  0x09
#define BLE_ESL_ERR_RETRY                                                   0x0A
#define BLE_ESL_ERR_QUEUE_FULL                                              0x0B
#define BLE_ESL_ERR_IMPLAUSIBLE_ABS_TIME                                    0x0C

/* Basic State Bitmap Bits */
#define BLE_ESL_BASIC_STATE_SERVICE_NEEDED                                  (1 << 0)
#define BLE_ESL_BASIC_STATE_SYNCHRONIZED                                    (1 << 1)
#define BLE_ESL_BASIC_STATE_ACTIVE_LED                                      (1 << 2)
#define BLE_ESL_BASIC_STATE_PENDING_LED_UPDATE                              (1 << 3)
#define BLE_ESL_BASIC_STATE_PENDING_DISP_UPDATE                             (1 << 4)

/* ESL Control Point TLV Helpers */
#define BLE_ESL_TLV_TAG(opcode)                                             ((opcode) & 0x0F)
#define BLE_ESL_TLV_LENGTH(opcode)                                          (((opcode) >> 4) & 0x0F)
#define BLE_ESL_TLV_PARAMS_LEN(opcode)                                      (BLE_ESL_TLV_LENGTH(opcode) + 1)
#define BLE_ESL_TLV_TOTAL_LEN(opcode)                                       (BLE_ESL_TLV_LENGTH(opcode) + 2)

/**
 * @brief   ESL Address Characteristic
 */
typedef struct {
    uint8_t esl_id;                                                             /*!< ESL ID, 0x00 to 0xFE, 0xFF is broadcast */
    uint8_t group_id_rfu;                                                       /*!< Group_ID in bits [6:0], RFU in bit 7 */
} __attribute__((packed)) esp_ble_esl_address_t;

/**
 * @brief   Key Material Characteristic
 */
typedef struct {
    uint8_t session_key[BLE_ESL_SESSION_KEY_SIZE];                              /*!< 128-bit AES session key */
    uint8_t iv[BLE_ESL_IV_SIZE];                                                /*!< 64-bit initialization vector */
} __attribute__((packed)) esp_ble_esl_key_material_t;

/**
 * @brief   ESL Current Absolute Time Characteristic
 */
typedef uint32_t esp_ble_esl_abs_time_t;

/**
 * @brief   Display Information Record
 */
typedef struct {
    uint16_t width;                                                             /*!< Display width in pixels */
    uint16_t height;                                                            /*!< Display height in pixels */
    uint8_t  display_type;                                                      /*!< Display type enumeration */
} __attribute__((packed)) esp_ble_esl_display_info_t;

/**
 * @brief   Image Information Characteristic
 */
typedef struct {
    uint8_t max_image_index;                                                    /*!< Highest supported Image_Index */
} __attribute__((packed)) esp_ble_esl_image_info_t;

/* ESL Address Helpers */
#define BLE_ESL_ADDR_GROUP_ID(addr)                                         ((addr).group_id_rfu & 0x7F)
#define BLE_ESL_ADDR_SET_GROUP_ID(addr, gid) \
    do { (addr).group_id_rfu = (uint8_t)((gid) & 0x7F); } while (0)

/**
 * @brief Get the ESL Address characteristic value.
 *
 * @param[out]  out_val The pointer to store the ESL Address
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_get_address(esp_ble_esl_address_t *out_val);

/**
 * @brief Set the ESL Address characteristic value.
 *
 * @param[in]  in_val The pointer to store the ESL Address
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_set_address(const esp_ble_esl_address_t *in_val);

/**
 * @brief Get the AP Sync Key Material characteristic value.
 *
 * @param[out]  out_val The pointer to store the key material
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_get_ap_sync_key(esp_ble_esl_key_material_t *out_val);

/**
 * @brief Set the AP Sync Key Material characteristic value.
 *
 * @param[in]  in_val The pointer to store the key material
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_set_ap_sync_key(const esp_ble_esl_key_material_t *in_val);

/**
 * @brief Get the ESL Response Key Material characteristic value.
 *
 * @param[out]  out_val The pointer to store the key material
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_get_resp_key(esp_ble_esl_key_material_t *out_val);

/**
 * @brief Set the ESL Response Key Material characteristic value.
 *
 * @param[in]  in_val The pointer to store the key material
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_set_resp_key(const esp_ble_esl_key_material_t *in_val);

/**
 * @brief Get the ESL Current Absolute Time characteristic value.
 *
 * @param[out]  out_val The pointer to store the absolute time
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_get_current_abs_time(esp_ble_esl_abs_time_t *out_val);

/**
 * @brief Set the ESL Current Absolute Time characteristic value.
 *
 * @param[in]  in_val The pointer to store the absolute time
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_set_current_abs_time(const esp_ble_esl_abs_time_t *in_val);

/**
 * @brief Set the Display Information characteristic value.
 *
 * Call this before esp_ble_esl_init() when CONFIG_BLE_ESL_DISPLAY_INFO is enabled.
 *
 * @param[in]  displays The pointer to store the display records
 * @param[in]  count    The number of displays
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_set_display_info(const esp_ble_esl_display_info_t *displays, uint8_t count);

/**
 * @brief Get the Display Information characteristic value.
 *
 * @param[out] displays  The pointer to store the display records
 * @param[in]  max_count The capacity of displays
 * @param[out] out_count The pointer to store the number of displays copied
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_get_display_info(esp_ble_esl_display_info_t *displays, uint8_t max_count,
                                       uint8_t *out_count);

/**
 * @brief Set the Image Information characteristic value.
 *
 * @param[in]  info The pointer to store the image information
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_set_image_info(const esp_ble_esl_image_info_t *info);

/**
 * @brief Get the Image Information characteristic value.
 *
 * @param[out]  info The pointer to store the image information
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_get_image_info(esp_ble_esl_image_info_t *info);

/**
 * @brief Set the Sensor Information characteristic value.
 *
 * @param[in]  data The pointer to store the sensor information
 * @param[in]  len  The length of sensor information
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_set_sensor_info(const uint8_t *data, uint16_t len);

/**
 * @brief Get the Sensor Information characteristic value.
 *
 * @param[out] data     The pointer to store the sensor information
 * @param[in]  max_len  The capacity of data
 * @param[out] out_len  The pointer to store the length copied
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_get_sensor_info(uint8_t *data, uint16_t max_len, uint16_t *out_len);

/**
 * @brief Set the LED Information characteristic value.
 *
 * @param[in]  leds  The pointer to store the LED information
 * @param[in]  count The number of LEDs
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_set_led_info(const uint8_t *leds, uint8_t count);

/**
 * @brief Get the LED Information characteristic value.
 *
 * @param[out] leds      The pointer to store the LED information
 * @param[in]  max_count The capacity of leds
 * @param[out] out_count The pointer to store the number of LEDs copied
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 *  - ESP_ERR_NOT_SUPPORTED on disabled characteristic
 */
esp_err_t esp_ble_esl_get_led_info(uint8_t *leds, uint8_t max_count, uint8_t *out_count);

/**
 * @brief Notify an ESL Control Point response to the remote client.
 *
 * @p len must be in [2, BLE_ESL_ECP_MAX_SIZE] and equal to
 * BLE_ESL_TLV_TOTAL_LEN(data[0]) for a single response TLV.
 *
 * @param[in]  data The pointer to store the response TLV
 * @param[in]  len  The length of response TLV
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_esl_notify_ecp_response(const uint8_t *data, uint16_t len);

/**
 * @brief Initialization Electronic Shelf Label Service
 *
 * Call esp_ble_esl_set_display_info() first when CONFIG_BLE_ESL_DISPLAY_INFO is enabled.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_STATE on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_esl_init(void);

/**
 * @brief Deinitialization Electronic Shelf Label Service
 *
 * @return
 *  - ESP_OK on successful
 *  - Error code from esp_ble_conn_remove_svc() on failure
 */
esp_err_t esp_ble_esl_deinit(void);

#ifdef __cplusplus
}
#endif
