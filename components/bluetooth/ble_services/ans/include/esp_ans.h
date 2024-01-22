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

/* 16 Bit Alert Notification Service UUID */
#define BLE_ANS_UUID16                                  0x1811

/* 16 Bit Alert Notification Service Characteristic UUIDs */
#define BLE_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT            0x2A47
#define BLE_ANS_CHR_UUID16_NEW_ALERT                    0x2A46
#define BLE_ANS_CHR_UUID16_SUP_UNR_ALERT_CAT            0x2A48
#define BLE_ANS_CHR_UUID16_UNR_ALERT_STAT               0x2A45
#define BLE_ANS_CHR_UUID16_ALERT_NOT_CTRL_PT            0x2A44

/* Alert Notification Service Category ID Bit Masks
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANS_CAT_BM_NONE                             0x00
#define BLE_ANS_CAT_BM_SIMPLE_ALERT                     0x01
#define BLE_ANS_CAT_BM_EMAIL                            0x02
#define BLE_ANS_CAT_BM_NEWS                             0x04
#define BLE_ANS_CAT_BM_CALL                             0x08
#define BLE_ANS_CAT_BM_MISSED_CALL                      0x10
#define BLE_ANS_CAT_BM_SMS                              0x20
#define BLE_ANS_CAT_BM_VOICE_MAIL                       0x40
#define BLE_ANS_CAT_BM_SCHEDULE                         0x80

/* Alert Notification Service Category IDs
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANS_CAT_ID_SIMPLE_ALERT                     0
#define BLE_ANS_CAT_ID_EMAIL                            1
#define BLE_ANS_CAT_ID_NEWS                             2
#define BLE_ANS_CAT_ID_CALL                             3
#define BLE_ANS_CAT_ID_MISSED_CALL                      4
#define BLE_ANS_CAT_ID_SMS                              5
#define BLE_ANS_CAT_ID_VOICE_MAIL                       6
#define BLE_ANS_CAT_ID_SCHEDULE                         7

/* Number of valid ANS categories
 *
 * TODO: Add remaining 2 optional categories */
#define BLE_ANS_CAT_NUM                                 8

/* Alert Notification Control Point Command IDs */
#define BLE_ANS_CMD_EN_NEW_ALERT_CAT                    0
#define BLE_ANS_CMD_EN_UNR_ALERT_CAT                    1
#define BLE_ANS_CMD_DIS_NEW_ALERT_CAT                   2
#define BLE_ANS_CMD_DIS_UNR_ALERT_CAT                   3
#define BLE_ANS_CMD_NOT_NEW_ALERT_IMMEDIATE             4
#define BLE_ANS_CMD_NOT_UNR_ALERT_IMMEDIATE             5

/* Max length of new alert info string */
#define BLE_ANS_INFO_STR_MAX_LEN        18

/* Max length of a new alert notification, max string length + 2 bytes for category ID and count. */
#define BLE_ANS_NEW_ALERT_MAX_LEN   (BLE_ANS_INFO_STR_MAX_LEN + 2)

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
 *  - ESP_ERR_INVALID_ARG on wrong category of the new alert
 */
esp_err_t esp_ble_ans_get_new_alert(uint8_t cat_id, uint8_t *cat_val);

/**
 * Send a new alert notification to the given category with the given info string.
 *
 * @param cat_id                The ID of the category to send the notification to
 * @param cat_info              The info string to send with the notification
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the new alert
 */
esp_err_t esp_ble_ans_set_new_alert(uint8_t cat_id, const char *cat_info);

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
 *  - ESP_ERR_INVALID_ARG on wrong category of the unread alert
 */
esp_err_t esp_ble_ans_get_unread_alert(uint8_t cat_id, uint8_t *cat_val);

/**
 * Send an unread alert to the given category then notifies the client if the given category is valid and enabled.
 *
 * @param cat_id The ID of the category which should be incremented and notified
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong category of the unread alert
 */
esp_err_t esp_ble_ans_set_unread_alert(uint8_t cat_id);

/**
 * @brief Initialization GATT Alert Notification Service
 *
 * @return
 *  - ESP_OK on successful
 *  - ESP_ERR_INVALID_ARG on wrong initialization
 *  - ESP_FAIL on error
 */
esp_err_t esp_ble_ans_init(void);

#ifdef __cplusplus
}
#endif
