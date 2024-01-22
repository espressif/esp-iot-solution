/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
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
/* BLE ANP EVENTS BASE */
ESP_EVENT_DECLARE_BASE(BLE_ANP_EVENTS);
/** @endcond **/

/* 16 Bit Alert Notification Service UUID */
#define BLE_ANP_UUID16                                  0x1811

/* 16 Bit Alert Notification Service Characteristic UUIDs */
#define BLE_ANP_CHR_UUID16_SUP_NEW_ALERT_CAT            0x2A47
#define BLE_ANP_CHR_UUID16_NEW_ALERT                    0x2A46
#define BLE_ANP_CHR_UUID16_SUP_UNR_ALERT_CAT            0x2A48
#define BLE_ANP_CHR_UUID16_UNR_ALERT_STAT               0x2A45
#define BLE_ANP_CHR_UUID16_ALERT_NOT_CTRL_PT            0x2A44

/* Alert Notification Service Category ID Bit Masks
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANP_CAT_BM_NONE                             0x00
#define BLE_ANP_CAT_BM_SIMPLE_ALERT                     0x01
#define BLE_ANP_CAT_BM_EMAIL                            0x02
#define BLE_ANP_CAT_BM_NEWS                             0x04
#define BLE_ANP_CAT_BM_CALL                             0x08
#define BLE_ANP_CAT_BM_MISSED_CALL                      0x10
#define BLE_ANP_CAT_BM_SMS                              0x20
#define BLE_ANP_CAT_BM_VOICE_MAIL                       0x40
#define BLE_ANP_CAT_BM_SCHEDULE                         0x80

/* Alert Notification Service Category IDs
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANP_CAT_ID_SIMPLE_ALERT                     0
#define BLE_ANP_CAT_ID_EMAIL                            1
#define BLE_ANP_CAT_ID_NEWS                             2
#define BLE_ANP_CAT_ID_CALL                             3
#define BLE_ANP_CAT_ID_MISSED_CALL                      4
#define BLE_ANP_CAT_ID_SMS                              5
#define BLE_ANP_CAT_ID_VOICE_MAIL                       6
#define BLE_ANP_CAT_ID_SCHEDULE                         7

/* Number of valid ANS categories
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANP_CAT_NUM                                 8

/* Alert Notification Control Point Command IDs */
#define BLE_ANP_CMD_EN_NEW_ALERT_CAT                    0
#define BLE_ANP_CMD_EN_UNR_ALERT_CAT                    1
#define BLE_ANP_CMD_DIS_NEW_ALERT_CAT                   2
#define BLE_ANP_CMD_DIS_UNR_ALERT_CAT                   3
#define BLE_ANP_CMD_NOT_NEW_ALERT_IMMEDIATE             4
#define BLE_ANP_CMD_NOT_UNR_ALERT_IMMEDIATE             5

/* Max length of new alert info string */
#define BLE_ANP_INFO_STR_MAX_LEN                        18

/* Max length of a new alert notification, max string length + 2 bytes for category ID and count. */
#define BLE_ANP_NEW_ALERT_MAX_LEN                       (BLE_ANP_INFO_STR_MAX_LEN + 2)

/**
 * @brief   The status of the new or unread alert
 */
typedef struct {
    union {
        struct {
            uint8_t cat_id;                             /*!< The predefined categories of unread alerts and messages */
            uint8_t count;                              /*!< The number of unread alerts in the server ranging from 0 to 255 */
        } unr_alert_stat;                               /*!< The status of unread alerts */

        struct {
            uint8_t cat_id;                             /*!< The predefined categories of new alerts and messages */
            uint8_t count;                              /*!< The number of new alerts in the server ranging from 0 to 255 */
            uint8_t cat_info[BLE_ANP_INFO_STR_MAX_LEN]; /*!< The brief text information for the last alert */
        } new_alert_val;                                /*!< The status of new alerts */
    };                                                  /*!< Alert notification status */
} esp_ble_anp_data_t;

/**
 * @brief   The option of the new or unread alert
 */
typedef enum {
    BLE_ANP_OPT_ENABLE,
    BLE_ANP_OPT_DISABLE,
    BLE_ANP_OPT_RECOVER,
} esp_ble_anp_option_t;

/**
 * @brief Read the value of or check supported new alert category.
 *
 * @attention 1. When cat_id is 0xFF, read the value of supported new alert category.
 * @attention 2. When cat_id isn't 0xFF, check supported new alert category is enable or disable.
 *
 * @param[in]  cat_id               The ID of the category to read or check
 * @param[out] cat_val              The value of read or check supported new alert category
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the alert
 */
esp_err_t esp_ble_anp_get_new_alert(uint8_t cat_id, uint8_t *cat_val);

/**
 * @brief Request or recovery supported new alert notification to the given category.
 *
 * @attention 1. When cat_id is 0xFF, recover for all supported new alert category to get the current message counts.
 * @attention 2. When cat_id isn't 0xFF, request for a supported new alert category to get the current message counts.
 *
 * @param[in] cat_id                The ID of the category to request or recover the notification to
 * @param[in] option                Disable or enable supported new alert category
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the alert
 */
esp_err_t esp_ble_anp_set_new_alert(uint8_t cat_id, esp_ble_anp_option_t option);

/**
 * @brief Read the value of or check supported unread alert status category.
 *
 * @attention 1. When cat_id is 0xFF, read the value of supported unread alert status category.
 * @attention 2. When cat_id isn't 0xFF, check supported unread alert status category is enable or disable.
 *
 * @param[in]  cat_id               The ID of the category to read or check
 * @param[out] cat_val              The value of read or check supported unread alert status category
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the alert
 */
esp_err_t esp_ble_anp_get_unr_alert(uint8_t cat_id, uint8_t *cat_val);

/**
 * @brief Request or recovery supported unread alert status notification to the given category.
 *
 * @attention 1. When cat_id is 0xFF, recover for all supported unread alert status category to get the current message counts.
 * @attention 2. When cat_id isn't 0xFF, request for an supported unread alert status category to get the current message counts.
 *
 * @param[in] cat_id                The ID of the category to request or recover the notification to
 * @param[in] option                Disable or enable supported unread alert status category
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the alert
 */
esp_err_t esp_ble_anp_set_unr_alert(uint8_t cat_id, esp_ble_anp_option_t option);

/**
 * @brief Initialization GATT Alert Notification Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_anp_init(void);

/**
 * @brief Deinitialization GATT Alert Notification Profile
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_anp_deinit(void);

#ifdef __cplusplus
}
#endif
