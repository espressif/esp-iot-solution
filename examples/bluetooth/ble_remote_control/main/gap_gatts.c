/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gap_gatts.c
 * @brief GAP GATT Server
 *
 * @details This files defines the GAP and GATT event callback and handlers
*/
#include "gap_gatts.h"

#define HID_DEMO_TAG "HID_RC_GAP_GATTS"

extern bool is_connected; // Need this here in the case of connection establishment
extern const char* DEVICE_NAME;

static uint8_t hidd_service_uuid128[] = {
    // Universally Unique Identifier (UUID) for Human Interface Device (HID) Service
    /* LSB <--------------------------------------------------------------------------------> MSB */
    // [12],[13] identifies it as a HID Service (0x1812)
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x03c0,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static gatts_profile_inst_t hid_profile = {
    .gatts_interface = ESP_GATT_IF_NONE,    // default value
    .callback = &hid_event_callback,        // user defined callback function (in hidd.c)
};

/**
 * Private function headers
*/
void gap_security_request_event(esp_ble_gap_cb_param_t *param);
void gatts_register_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

esp_err_t gap_gatts_deinit(void)
{
    ESP_ERROR_CHECK(esp_ble_gatts_close(hid_profile.gatts_interface, hid_profile.conn_id));
    ESP_ERROR_CHECK(esp_ble_gatts_app_unregister(hid_profile.gatts_interface));
    ESP_ERROR_CHECK(esp_ble_gap_disconnect(hid_profile.remote_bda));

    return ESP_OK;
}

/****************************************************************************************
 * Generic Access Profile - GAP Related Functions
****************************************************************************************/

void gap_set_security(void)
{
    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;     // bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           // set the IO capability to No output No input

    uint8_t KEY_SIZE = 16;      // the key size should be 7~16 bytes
    uint8_t KEY_INIT = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t KEY_RSP = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

    /*
    If BLE device as a Slave:
        - KEY_INIT: expected key type from the Master
        - KEY_RSP: key type to distribute to the Master
    If BLE device as a master:
        - KEY_INIT: key type to distribute to the Slave
        - KEY_RSP: expected key type from the Slave
    */
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &KEY_SIZE, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &KEY_INIT, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &KEY_RSP, sizeof(uint8_t));
}

void gap_event_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(HID_DEMO_TAG, "ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT");
        esp_ble_gap_start_advertising(&adv_params); // Trigger ADV_START_COMPLETE_EVT once done
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(HID_DEMO_TAG, "GAP Start Advertising %s\n",
                 (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) ? "Success" : "Fail");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d", \
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        memcpy(hid_profile.remote_bda, param->update_conn_params.bda, sizeof(esp_bd_addr_t));
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(HID_DEMO_TAG, "ESP_GAP_BLE_SEC_REQ_EVT");
        gap_security_request_event(param);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        ESP_LOGI(HID_DEMO_TAG, "Authentication %s", (param->ble_security.auth_cmpl.success) ? "Successful" : "Failed");
        if (param->ble_security.auth_cmpl.success) {
            // Manage to authenticate, start to send HID reports
            is_connected = true;
        } else {
            // Failed to authenticate, restart advertising
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
    default:
        // Note not all events need to be handled, depending on the application
        // Refer to ESP-IDF esp_gap_ble_api.h for more event definitions
        ESP_LOGI(HID_DEMO_TAG, "Unhandled GAP_EVT, event %d\n", event);
        break;
    }
}

/**
 * @brief Callback function for handling GAP security request events
*/
void gap_security_request_event(esp_ble_gap_cb_param_t *param)
{
    /*
    If to accept the security request, send a positive security response to the peer device
    Else, send a negative security response

    Abstracted out for user customization on authentication/security requirements
    */
    ESP_LOGI(HID_DEMO_TAG, BD_ADDR_2_STR(param->ble_security.ble_req.bd_addr));
    esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
}

/****************************************************************************************
 * Generic ATTribute Profile (Server) - GATTS Related Functions
****************************************************************************************/

void gatts_event_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        ESP_LOGI(HID_DEMO_TAG, "Register GATTS profile");
        gatts_register_event(gatts_if, param);
    } else if (event == ESP_GATTS_CONNECT_EVT) {
        // Store connection id for tear down
        hid_profile.conn_id = param->connect.conn_id;
    } else if (event == ESP_GATTS_DISCONNECT_EVT) {
        // Restart advertising when disconnected
        esp_ble_gap_start_advertising(&adv_params);
    } else {
        ESP_LOGI(HID_DEMO_TAG, "GATTS to be handled by profile, event %d", event);
    }

    // Cascade the gatts event to the profile (in this example, there is only 1)
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == hid_profile.gatts_interface) {
        if (hid_profile.callback) {
            hid_profile.callback(event, gatts_if, param);
        } else {
            ESP_LOGW(HID_DEMO_TAG, "GATTS Profile callback is not defined");
        }
    }

    return;
}

/**
 * @brief Callback function for handling GATTS registration events
*/
void gatts_register_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (param->reg.status != ESP_GATT_OK) {
        ESP_LOGE(HID_DEMO_TAG, "reg app failed, app_id %04x, status %d",
                 param->reg.app_id,
                 param->reg.status);
        return;
    }
    // Store interface for tear down
    hid_profile.gatts_interface = gatts_if;

    esp_ble_gap_set_device_name(DEVICE_NAME);
    esp_ble_gap_config_local_icon(ESP_BLE_APPEARANCE_HID_JOYSTICK);

    // If success, trigger ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT
    esp_ble_gap_config_adv_data(&adv_data);
}
