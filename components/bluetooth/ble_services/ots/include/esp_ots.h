/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event_base.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/* BLE OTS EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_OTS_EVENTS);
/** @endcond **/

/* OTS Characteristic Value Length */
#define BLE_OTS_ATT_VAL_LEN                                               20    /*!< Attribute value length */

/* 16 Bit Object Transfer Service UUID */
#define BLE_OTS_UUID16                                                    0x1825

/* 16 Bit Object Transfer Characteristics UUIDs */
#define BLE_OTS_CHR_UUID16_OTS_FEATURE                                    0x2ABD
#define BLE_OTS_CHR_UUID16_OBJECT_NAME                                    0x2ABE
#define BLE_OTS_CHR_UUID16_OBJECT_TYPE                                    0x2ABF
#define BLE_OTS_CHR_UUID16_OBJECT_SIZE                                    0x2AC0
#define BLE_OTS_CHR_UUID16_OBJECT_FIRST_CREATED                           0x2AC1
#define BLE_OTS_CHR_UUID16_OBJECT_LAST_MODIFIED                           0x2AC2
#define BLE_OTS_CHR_UUID16_OBJECT_ID                                      0x2AC3
#define BLE_OTS_CHR_UUID16_OBJECT_PROP                                    0x2AC4
#define BLE_OTS_CHR_UUID16_OBJECT_ACTION_CONTROL_POINT                    0x2AC5
#define BLE_OTS_CHR_UUID16_OBJECT_LIST_CONTROL_POINT                      0x2AC6
#define BLE_OTS_CHR_UUID16_OBJECT_LIST_FILTER                             0x2AC7
#define BLE_OTS_CHR_UUID16_OBJECT_CHANGED                                 0x2AC8

/* Object Action Control Point (OACP) Op Code */
#define BLE_OTS_OACP_CREATE                                               (0x01)
#define BLE_OTS_OACP_DELETE                                               (0x02)
#define BLE_OTS_OACP_CALCULATE_CHECKSUM                                   (0x03)
#define BLE_OTS_OACP_EXECUTE                                              (0x04)
#define BLE_OTS_OACP_READ                                                 (0x05)
#define BLE_OTS_OACP_WRITE                                                (0x06)
#define BLE_OTS_OACP_ABORT                                                (0x07)
#define BLE_OTS_OACP_RESPONSE                                             (0x60)

/* The Result Codes associated with OACP Response Op Code (0x60) */
#define BLE_OTS_OACP_RSP_SUCCESS                                          (0x01)
#define BLE_OTS_OACP_RSP_NOT_SUPPORT                                      (0x02)
#define BLE_OTS_OACP_RSP_INVALID_PARAMETER                                (0x03)
#define BLE_OTS_OACP_RSP_INSUFFICIENT_RESOURCES                           (0x04)
#define BLE_OTS_OACP_RSP_INVALID_OBJECT                                   (0x05)
#define BLE_OTS_OACP_RSP_CHANNEL_UNAVAILABLE                              (0x06)
#define BLE_OTS_OACP_RSP_UNSUPPORT_TYPE                                   (0x07)
#define BLE_OTS_OACP_RSP_NOT_PERMIT                                       (0x08)
#define BLE_OTS_OACP_RSP_OBJECT_LOCK                                      (0x09)
#define BLE_OTS_OACP_RSP_OPERATION_FAIL                                   (0x0A)

/* Object List Control Point (OLCP) Op Code */
#define BLE_OTS_OLCP_FIRST                                                (0x01)
#define BLE_OTS_OLCP_LAST                                                 (0x02)
#define BLE_OTS_OLCP_PREVIOUS                                             (0x03)
#define BLE_OTS_OLCP_NEXT                                                 (0x04)
#define BLE_OTS_OLCP_GO_TO                                                (0x05)
#define BLE_OTS_OLCP_ORDER                                                (0x06)
#define BLE_OTS_OLCP_REQ_NUM_OF_OBJ                                       (0x07)
#define BLE_OTS_OLCP_CLEAR_MARK                                           (0x08)
#define BLE_OTS_OLCP_RESPONSE                                             (0x70)

/* The List Sort Order Parameter. For Op Code (0x06) */
#define BLE_OTS_OLCP_SORT_BY_NAME_ASCENDING                               (0x01)
#define BLE_OTS_OLCP_SORT_BY_TYPE_ASCENDING                               (0x02)
#define BLE_OTS_OLCP_SORT_BY_SIZE_ASCENDING                               (0x03)
#define BLE_OTS_OLCP_SORT_BY_FIRST_CREATE_TIME_ASCENDING                  (0x04)
#define BLE_OTS_OLCP_SORT_BY_LAST_MODIFY_TIME_ASCENDING                   (0x05)
#define BLE_OTS_OLCP_SORT_BY_NAME_DESCENDING                              (0x11)
#define BLE_OTS_OLCP_SORT_BY_TYPE_DESCENDING                              (0x12)
#define BLE_OTS_OLCP_SORT_BY_SIZE_DESCENDING                              (0x13)
#define BLE_OTS_OLCP_SORT_BY_FIRST_CREATE_TIME_DESCENDING                 (0x14)
#define BLE_OTS_OLCP_SORT_BY_LAST_MODIFY_TIME_DESCENDING                  (0x15)

/* The Result Codes (OLCP RSP) associated with OLCP Response Op Code (0x70) */
#define BLE_OTS_OLCP_RSP_SUCCESS                                          (0x01)
#define BLE_OTS_OLCP_RSP_NOT_SUPPORT                                      (0x02)
#define BLE_OTS_OLCP_RSP_INVALID_PARAMETER                                (0x03)
#define BLE_OTS_OLCP_RSP_OPERATION_FAIL                                   (0x04)
#define BLE_OTS_OLCP_RSP_OUT_OF_BOUNDS                                    (0x05)
#define BLE_OTS_OLCP_RSP_TOO_MANY_OBJECTS                                 (0x06)
#define BLE_OTS_OLCP_RSP_NO_OBJECT                                        (0x07)
#define BLE_OTS_OLCP_RSP_OBJECT_ID_NOT_FOUND                              (0x08)

/* Object List Filter Op Code */
#define BLE_OTS_FILTER_NAME_START                                         (0x01)
#define BLE_OTS_FILTER_NAME_END                                           (0x02)
#define BLE_OTS_FILTER_NAME_CONTAIN                                       (0x03)
#define BLE_OTS_FILTER_NAME_EXACTLY                                       (0x04)
#define BLE_OTS_FILTER_OBJ_TYPE                                           (0x05)
#define BLE_OTS_FILTER_CREATE_TIME                                        (0x06)
#define BLE_OTS_FILTER_MODIFY_TIME                                        (0x07)
#define BLE_OTS_FILTER_CURRENT_SIZE                                       (0x08)
#define BLE_OTS_FILTER_ALLOC_SIZE                                         (0x09)
#define BLE_OTS_FILTER_MARK_OBJECT                                        (0x0A)

/* Object Change Characteristic Flags Bit Masks */
#define BLE_OTS_OBJ_CHG_SRC                        (0x1 << 0)             //bit0 Source of Change
#define BLE_OTS_OBJ_CHG_CONTENT                    (0x1 << 1)             //bit1 Change occurred to the object contents
#define BLE_OTS_OBJ_CHG_METADATA                   (0x1 << 2)             //bit2 Change occurred to the object metadata
#define BLE_OTS_OBJ_CHG_CREATION                   (0x1 << 3)             //bit3 Object Creation
#define BLE_OTS_OBJ_CHG_DELETION                   (0x1 << 4)             //bit4 Object Deletion

/**
 * @brief   OTS Feature Characteristic
 */
typedef struct {
    struct {
        uint32_t create_op: 1;                                            /*!< Create Op Code Supported. 0: False, 1: True */
        uint32_t delete_op: 1;                                            /*!< Delete Op Code Supported. 0: False, 1: True */
        uint32_t calculate_op: 1;                                         /*!< Calculate Checksum Op Code Supported. 0: False, 1: True */
        uint32_t execute_op: 1;                                           /*!< Execute Op Code Supported. 0: False, 1: True */
        uint32_t read_op: 1;                                              /*!< Read Op Code Supported. 0: False, 1: True */
        uint32_t write_op: 1;                                             /*!< Write Op Code Supported. 0: False, 1: True */
        uint32_t appending_op: 1;                                         /*!< Appending Op Code Supported. 0: False, 1: True */
        uint32_t truncation_op: 1;                                        /*!< Truncation Op Code Supported. 0: False, 1: True */
        uint32_t patching_op: 1;                                          /*!< Patching Op Code Supported. 0: False, 1: True */
        uint32_t abort_op: 1;                                             /*!< Abort Op Code Supported. 0: False, 1: True */
    } oacp;                                                               /*!< The structure of the OACP Features field */

    struct {
        uint32_t goto_op: 1;                                              /*!< Go to Op Code Supported. 0: False, 1: True */
        uint32_t order_op: 1;                                             /*!< Order Op Code Supported. 0: False, 1: True */
        uint32_t req_num_op: 1;                                           /*!< Request Number of Objects Op Code Supported. 0: False, 1: True */
        uint32_t clear_mark_op: 1;                                        /*!< Clear Marking Op Code Supported. 0: False, 1: True */
    } olcp;                                                               /*!< The structure of the OLCP Features field */
} __attribute__((packed)) esp_ble_ots_feature_t;

/**
 * @brief   Object Size Characteristic
 */
typedef struct {
    uint32_t current_size;                                                /*!< Current Object Size */
    uint32_t allocated_size;                                              /*!< Allocated Object Size */
} __attribute__((packed)) esp_ble_ots_size_t;

/**
 * @brief   UTC
 */
typedef struct {
    uint16_t year;                                                        /*!< Year as defined by the Gregorian calendar, Valid range 1582 to 9999 */
    uint8_t  month;                                                       /*!< Month of the year as defined by the Gregorian calendar, Valid range 1 (January) to 12 (December) */
    uint8_t  day;                                                         /*!< Day of the month as defined by the Gregorian calendar, Valid range 1 to 31 */
    uint8_t  hours;                                                       /*!< Number of hours past midnight, Valid range 0 to 23 */
    uint8_t  minutes;                                                     /*!< Number of minutes since the start of the hour. Valid range 0 to 59 */
    uint8_t  seconds;                                                     /*!< Number of seconds since the start of the minute. Valid range 0 to 59 */
} __attribute__((packed)) esp_ble_ots_utc_t;

/**
 * @brief   Object ID
 */
typedef struct {
    uint8_t  id[6];                                                       /*!< Allocated ID. Valid range 0x000000000100 to 0xffffffffffff */
} __attribute__((packed)) esp_ble_ots_id_t;

/**
 * @brief   Object Properties Characteristic
 */
typedef struct {
    uint32_t delete_prop: 1;                                              /*!< Deletion of this object is permitted. 0: False, 1: True */
    uint32_t execute_prop: 1;                                             /*!< Execute of this object is permitted. 0: False, 1: True */
    uint32_t read_prop: 1;                                                /*!< Reading of this object is permitted. 0: False, 1: True */
    uint32_t write_prop: 1;                                               /*!< Writing of this object is permitted. 0: False, 1: True */
    uint32_t append_prop: 1;                                              /*!< Appending of this object is permitted. 0: False, 1: True */
    uint32_t truncate_prop: 1;                                            /*!< Truncation of this object is permitted. 0: False, 1: True */
    uint32_t patch_prop: 1;                                               /*!< Patching of this object is permitted. 0: False, 1: True */
    uint32_t mark_prop: 1;                                                /*!< This object is a marked object. 0: False, 1: True */
} __attribute__((packed)) esp_ble_ots_prop_t;

/**
 * @brief   Object Action Control Point (OACP)
 */
typedef struct {
    uint8_t op_code;                                                      /*!< OACP op code */
    uint8_t parameter[20];                                                /*!< OACP parameter */
} __attribute__((packed)) esp_ble_ots_oacp_t;

/**
 * @brief   Object List Control Point (OLCP)
 */
typedef struct {
    uint8_t op_code;                                                      /*!< OLCP op code */
    uint8_t parameter[6];                                                 /*!< OLCP parameter */
} __attribute__((packed)) esp_ble_ots_olcp_t;

/**
 * @brief   Object List Control Point (OLCP) Response Value
 */
typedef struct {
    uint8_t req_code;                                                     /*!< OLCP op code */
    uint8_t rsp_code;                                                     /*!< OLCP rsp op code */
    uint8_t rsp_parameter[6];                                             /*!< OLCP rsp parameter */
} __attribute__((packed)) esp_ble_ots_olcp_rsp_t;

/**
 * @brief   Object List Filter Characteristic
 */
typedef struct {
    uint8_t filter;                                                       /*!< Object filter type */
    uint8_t filter_parameter[BLE_OTS_ATT_VAL_LEN];                        /*!< Object filter parameter */
} __attribute__((packed)) esp_ble_ots_filter_t;

/**
 * @brief   Object Change Characteristic
 */
typedef struct {
    uint8_t flag;                                                         /*!< Flag fields */
    uint8_t object_id[6];                                                 /*!< Object ID */
} __attribute__((packed)) esp_ble_ots_change_t;

/**
 * @brief Read the value of OTS Feature Characteristic.
 *
 * @param[out]  out_val The pointer to store the OTS Feature Value.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_feature(esp_ble_ots_feature_t *out_val);

/**
 * @brief Set the value of OTS Feature Characteristic.
 *
 * @param[in]  in_val The pointer to store OTS Feature value.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_feature(esp_ble_ots_feature_t *in_val);

/**
 * @brief Read the value of Object Name Characteristic.
 *
 * @param[out]  out_buf The pointer to store the Object Name.
 * @param[in]   buf_len Maximum length of the output buffer.
 * @param[out]  out_len The pointer to store the actual length of the Object Name (can be NULL if not needed).
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_name(uint8_t *out_buf, size_t buf_len, size_t *out_len);

/**
 * @brief Set the Object Name Characteristic value.
 *
 * @param[in]  in_val The pointer to the Object Name data.
 * @param[in]  in_len The length of the Object Name data.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_name(const uint8_t *in_val, size_t in_len);

/**
 * @brief Read the value of Object Type Characteristic value.
 *
 * @param[out]  out_val The pointer to store the Object Type.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_type(uint16_t *out_val);

/**
 * @brief Set the Object Type Characteristic value.
 *
 * @param[in]  in_val The pointer to store Object Type.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_type(uint16_t *in_val);

/**
 * @brief Read the value of Object Size Characteristic value.
 *
 * @param[out]  out_val The pointer to store the Object Size.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_size(esp_ble_ots_size_t *out_val);

/**
 * @brief Set the Object Size Characteristic value.
 *
 * @param[in]  in_val The pointer to store Object Size.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_size(esp_ble_ots_size_t *in_val);

/**
 * @brief Read the value of Object First Create Characteristic value.
 *
 * @param[out]  out_val The pointer to store the Object First Create Time.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_first_create_time(esp_ble_ots_utc_t *out_val);

/**
 * @brief Set the Object First Create Characteristic value.
 *
 * @param[in]  in_val The pointer to store the Object First Create Time.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_first_create_time(esp_ble_ots_utc_t *in_val);

/**
 * @brief Read the value of Object Last Modify Characteristic value.
 *
 * @param[out]  out_val The pointer to store the Object Last Modify Time.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_last_modify_time(esp_ble_ots_utc_t *out_val);

/**
 * @brief Set the Object Last Modify Characteristic value.
 *
 * @param[in]  in_val The pointer to store the Object Last Modify Time.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_last_modify_time(esp_ble_ots_utc_t *in_val);

/**
 * @brief Read the value of Object ID value.
 *
 * @param[out]  out_val The pointer to store the Object ID.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_id(esp_ble_ots_id_t *out_val);

/**
 * @brief Set the Object ID value.
 *
 * @param[in]  in_val The pointer to store the Object ID.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_id(esp_ble_ots_id_t *in_val);

/**
 * @brief Read the value of Object Properties value.
 *
 * @param[out]  out_val The pointer to store the Object Properties.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_prop(esp_ble_ots_prop_t *out_val);

/**
 * @brief Set the Object Properties value.
 *
 * @param[in]  in_val The pointer to store the Object Properties.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_prop(esp_ble_ots_prop_t *in_val);

/**
 * @brief Read the value of Object Action Control Point value.
 *
 * @param[out]  out_val The pointer to store the Object Action Control Point.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_oacp(esp_ble_ots_oacp_t *out_val);

/**
 * @brief Set the Object Action Control Point value
 *
 * @param[in]  in_val The pointer to store the Object Action Control Point value
 *
 * @param[in]  need_send If set to true, the OACP Value will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_oacp(esp_ble_ots_oacp_t *in_val, bool need_send);

/**
 * @brief Read the value of Object List Control Point value.
 *
 * @param[out]  out_val The pointer to store the Object List Control Point.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_olcp(esp_ble_ots_olcp_t *out_val);

/**
 * @brief Set the Object List Control Point value
 *
 * @param[in]  in_val The pointer to store the Object List Control Point value
 *
 * @param[in]  need_send If set to true, the OLCP Value will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_olcp(esp_ble_ots_olcp_t *in_val, bool need_send);

/**
 * @brief Read the value of Object Filter value.
 *
 * @param[out]  out_val The pointer to store the Object Filter.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_filter(esp_ble_ots_filter_t *out_val);

/**
 * @brief Set the value of Object Filter value.
 *
 * @param[in]  in_val The pointer to store the Object Filter.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_filter(esp_ble_ots_filter_t *in_val);

/**
 * @brief Read the value of Object Change value.
 *
 * @param[out]  out_val The pointer to store the Object Change.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong parameter
 */
esp_err_t esp_ble_ots_get_change(esp_ble_ots_change_t *out_val);

/**
 * @brief Set the Object Change value.
 *
 * @param[in]  in_val The pointer to store the Object Change.
 *
 * @param[in]  need_send If set to true, the Object Change value will send to remote client.
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 */
esp_err_t esp_ble_ots_set_change(esp_ble_ots_change_t *in_val, bool need_send);

/**
 * @brief Initialization Object Transfer Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_ERR_NO_MEM if failed to create mutex
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_ots_init(void);

/**
 * @brief Deinitialization Object Transfer Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong deinitialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_ots_deinit(void);

#ifdef __cplusplus
}
#endif
