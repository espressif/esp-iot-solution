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
/* BLE UDS EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_UDS_EVENTS);
/** @endcond **/

/* UDS Characteristic Value Length */
#define BLE_UDS_ATT_VAL_LEN                                                     20    /*!< Attribute value length */
#define BLE_UDS_USER_CTRL_POINT_PARAM_LEN                                       18    /*!< User control point param length */

/* 16 Bit USER DATA Service UUID */
#define BLE_UDS_UUID16                                                          0x181C

/* 16 Bit User Data Characteristics UUIDs */
#define BLE_UDS_CHR_UUID16_DATABASH_CHG                                         0x2A99
#define BLE_UDS_CHR_UUID16_USER_INDEX                                           0x2A9A
#define BLE_UDS_CHR_UUID16_USER_CTRL                                            0x2A9F
#define BLE_UDS_CHR_UUID16_REG_USER                                             0x2B37

/* User Control Point characteristic OP-Code */
#define BLE_UDS_USER_CTRL_REG_NEW_OP                                            0x01
#define BLE_UDS_USER_CTRL_CONSENT_OP                                            0x02
#define BLE_UDS_USER_CTRL_DEL_DATA_OP                                           0x03
#define BLE_UDS_USER_CTRL_LIST_ALL_OP                                           0x04
#define BLE_UDS_USER_CTRL_DEL_USER_OP                                           0x05
#define BLE_UDS_USER_CTRL_RSP_OP                                                0x20

/**
 * @brief   Database Change Increment
 */
typedef struct {
    uint8_t len;                                                         /*!< Database buffer length */
    uint8_t db_buf[BLE_UDS_ATT_VAL_LEN];                                 /*!< Database buffer */
} esp_ble_uds_db_t;

/**
 * @brief   Register User Characteristic
 */
typedef struct {
    struct {
        uint8_t first_segment: 1;                                        /*!< 0: False, 1: True */
        uint8_t last_segment: 1;                                         /*!< 0: False, 1: True */
        uint8_t rolling_segment_number: 6;                               /*!< When the Rolling segment number is equal to 63, the value is reset to 0 the next time it is incremented */
    } header;                                                            /*!< The structure of the Segmentation Header field */

    struct {
        uint8_t user_name_present: 1;                                    /*!< 0: False, 1: True */
        uint8_t user_name_truncated: 1;                                  /*!< 0: False, 1: True */
    } flag;                                                              /*!< Defines the value of the flags field that a server shall use when the register user characteristic is indicate */

    uint8_t user_index;                                                  /*!< User ID Index*/
    uint8_t user_name[BLE_UDS_ATT_VAL_LEN];                              /*!< User name*/
} __attribute__((packed)) esp_ble_uds_reg_user_t;

/**
 * @brief   User Control Point
 */
typedef struct {
    uint8_t op_code;                                                     /*!< User control point opcode */
    uint8_t param_len;                                                   /*!< Parameter buffer length */
    uint8_t parameter[BLE_UDS_USER_CTRL_POINT_PARAM_LEN];                /*!< Parameter depend on opcode */
} esp_ble_uds_user_ctrl_t;

/**
 * @brief Read the value of Database Change Increment characteristic.
 *
 * @param[in]  out_val The pointer to store the Database Change Increment.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_uds_get_db_incre(esp_ble_uds_db_t *out_val);

/**
 * @brief Set the Database Change Increment characteristic value.
 *
 * @param[in]  in_val The pointer to store the Database Change Increment Information.
 *
 * @param[in]  need_send If set to true, the User Data Info will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_uds_set_db_incre(esp_ble_uds_db_t *in_val, bool need_send);

/**
 * @brief Read the value of User Control characteristic.
 *
 * @param[in]  out_val The pointer to store the User Control value.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_uds_get_user_ctrl(esp_ble_uds_user_ctrl_t *out_val);

/**
 * @brief Set the User Control Point characteristic value.
 *
 * @param[in]  in_val The pointer to store the User Control Value.
 *
 * @param[in]  need_send If set to true, the User Control Value will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_uds_set_user_ctrl(esp_ble_uds_user_ctrl_t *in_val, bool need_send);

/**
 * @brief Read the value of Register User characteristic.
 *
 * @param[in]  out_val The pointer to store the Register User value.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_uds_get_reg_user(esp_ble_uds_reg_user_t *out_val);

/**
 * @brief Set the Register User characteristic value.
 *
 * @param[in]  in_val The pointer to store the Register User Value.
 *
 * @param[in]  need_send If set to true, the Register User Value will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_uds_set_reg_user(esp_ble_uds_reg_user_t *in_val, bool need_send);

/**
 * @brief Initialization User Data Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_uds_init(void);

#ifdef __cplusplus
}
#endif
