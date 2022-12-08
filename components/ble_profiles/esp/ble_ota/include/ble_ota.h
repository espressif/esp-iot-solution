// Copyright 2022-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief BLE OAT receive data callback function type
 * @param buf : The pointer to the receive data buffer.
 * @param length : The length of receive data buffer.
 */
typedef void (* esp_ble_ota_recv_fw_cb_t)(uint8_t *buf, uint32_t length);

/**
 * @brief BLE OTA callback functions struct
 */
typedef struct esp_ble_ota_callback_funs {
    esp_ble_ota_recv_fw_cb_t recv_fw_cb;    /*!< BLE OTA data receive callback function */
} esp_ble_ota_callback_funs_t;

/**
 * @brief BLE OTA notification flags struct
 */
typedef struct esp_ble_ota_notification_check {
    bool recv_fw_ntf_enable;        /*!< BLE OTA receive firmware characteristic */
    bool process_bar_ntf_enable;    /*!< BLE OTA notify process bar characteristic */
    bool command_ntf_enable;        /*!< BLE OTA command characteristic */
    bool customer_ntf_enable;       /*!< BLE OTA customer data characteristic */
} esp_ble_ota_notification_check_t;

/// BLE OTA characteristics
typedef enum {
    RECV_FW_CHAR,
    RECV_FW_CHAR_CCC,

    OTA_STATUS_CHAR,
    OTA_STATUS_CHAR_CCC,

    CMD_CHAR,
    CMD_CHAR_CCC,

    CUS_CHAR,
    CUS_CHAR_CCC,

    INVALID_CHAR,
} esp_ble_ota_char_t;

/// BLE DIS characteristics
typedef enum {
    DIS_SVC_IDX,

    DIS_MODEL_CHAR_IDX,
    DIS_MODEL_CHAR_VAL_IDX,

    DIS_SN_CHAR_IDX,
    DIS_SN_CHAR_VAL_IDX,

    DIS_FW_CHAR_IDX,
    DIS_FW_CHAR_VAL_IDX,

    DIS_IDX_NB,
} esp_ble_dis_service_index_t;

/// BLE OTA characteristics Index
typedef enum {
    OTA_SVC_IDX,

    RECV_FW_CHAR_IDX,
    RECV_FW_CHAR_VAL_IDX,
    RECV_FW_CHAR_NTF_CFG,

    OTA_STATUS_CHAR_IDX,
    OTA_STATUS_CHAR_VAL_IDX,
    OTA_STATUS_NTF_CFG,

    CMD_CHAR_IDX,
    CMD_CHAR_VAL_IDX,
    CMD_CHAR_NTF_CFG,

    CUS_CHAR_IDX,
    CUS_CHAR_VAL_IDX,
    CUS_CHAR_NTF_CFG,

    OTA_IDX_NB,
} esp_ble_ota_service_index_t;

/**
 * @brief           This function is called to Initialization ble ota host
 *
 * @return
 *                  - ESP_OK: success
 *                  - other: failed
 *
 */
esp_err_t esp_ble_ota_host_init(void);

/**
 * @brief           This function is called to register ble ota receive firmware data callback function
 *
 * @param[in]       callback : pointer to the application callback function.
 *
 * @return
 *                  - ESP_OK: success
 *                  - other: failed
 *
 */
esp_err_t esp_ble_ota_recv_fw_data_callback(esp_ble_ota_recv_fw_cb_t callback);

/**
 * @brief           This function is called to Initialization ble ota process
 *
 * @return
 *                  - length of ota firmware
 *
 */
unsigned int esp_ble_ota_get_fw_length(void);

#ifdef __cplusplus
}
#endif
