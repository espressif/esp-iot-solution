/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_ble.h"

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_bluedroid_hci.h"
#include "esp_bt_main.h"
#include "esp_hosted.h"
#include "esp_hosted_bluedroid.h"
#include "esp_hosted_misc.h"
#else
#include "esp_bt.h"
#include "esp_bt_main.h"
#endif

#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_hidd_prf_api.h"
#include "esp_bt_defs.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_device.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hidd_le_prf_int.h"

#define HID_DEMO_TAG "mouse HID"
#define HIDD_DEVICE_NAME            "myl HID"
#define AIRMOUSE_HID_NVS_NS         "airmouse_hid"

#ifndef CONFIG_AIRMOUSE_BLE_CCCD_RESTORE_DELAY_MS
#define CONFIG_AIRMOUSE_BLE_CCCD_RESTORE_DELAY_MS 10000
#endif

static airmouse_ble_handle_t s_ble_hid_ctx = {
    .hid_conn_id = 0,
    .sec_conn = false,
    .runtime_config_mode = false,
    .mouse_notify_enabled = false,
    .abs_mouse_notify_enabled = false,
    .key_notify_enabled = false,
    .cc_notify_enabled = false,
    .peer_bonded = false,
    .cccd_seen_this_connection = false,
    .cccd_restored_from_nvs = false,
    .has_peer = false,
    .peer_bda = {0},
    .peer_addr_type = 0,
};
#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))

static esp_err_t airmouse_ble_init_host_stack(void);
#if CONFIG_IDF_TARGET_ESP32P4
static esp_err_t airmouse_ble_init_hosted_bluedroid(void);
#else
static esp_err_t airmouse_ble_init_local_controller(void);
#endif

static bool airmouse_ble_make_peer_cccd_key(char *out,
                                            size_t out_len,
                                            char prefix,
                                            const esp_bd_addr_t bda)
{
    if (out == NULL || out_len < 16) {
        return false;
    }

    int written = snprintf(out, out_len,
                           "%c_%02X%02X%02X%02X%02X%02X",
                           prefix,
                           bda[0], bda[1], bda[2],
                           bda[3], bda[4], bda[5]);
    return written > 0 && (size_t)written < out_len;
}

static bool airmouse_ble_nvs_get_bool(const char *key,
                                      bool default_value,
                                      bool *found)
{
    if (found != NULL) {
        *found = false;
    }

    if (key == NULL) {
        return default_value;
    }

    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open(AIRMOUSE_HID_NVS_NS, NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "nvs_open(read) failed for key %s: %s",
                 key, esp_err_to_name(ret));
        return default_value;
    }

    uint8_t value = default_value ? 1 : 0;
    ret = nvs_get_u8(nvs, key, &value);
    nvs_close(nvs);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return default_value;
    }

    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "nvs_get_u8 failed for key %s: %s",
                 key, esp_err_to_name(ret));
        return default_value;
    }

    if (found != NULL) {
        *found = true;
    }
    return value != 0;
}

static void airmouse_ble_nvs_set_bool(const char *key, bool value)
{
    if (key == NULL) {
        return;
    }

    nvs_handle_t nvs = 0;
    esp_err_t ret = nvs_open(AIRMOUSE_HID_NVS_NS, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "nvs_open(write) failed for key %s: %s",
                 key, esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_u8(nvs, key, value ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "nvs_set_u8 failed for key %s: %s",
                 key, esp_err_to_name(ret));
        nvs_close(nvs);
        return;
    }

    ret = nvs_commit(nvs);
    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "nvs_commit failed for key %s: %s",
                 key, esp_err_to_name(ret));
    }
    nvs_close(nvs);
}

static bool airmouse_ble_is_peer_bonded(const esp_bd_addr_t peer_bda)
{
    int bond_num = esp_ble_get_bond_device_num();
    ESP_LOGI(HID_DEMO_TAG, "bonded device count = %d", bond_num);

    if (bond_num <= 0) {
        return false;
    }

    esp_ble_bond_dev_t *bond_list =
        (esp_ble_bond_dev_t *)calloc(bond_num, sizeof(esp_ble_bond_dev_t));
    if (bond_list == NULL) {
        ESP_LOGW(HID_DEMO_TAG, "failed to allocate bonded device list");
        return false;
    }

    int dev_num = bond_num;
    esp_err_t ret = esp_ble_get_bond_device_list(&dev_num, bond_list);
    if (ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "esp_ble_get_bond_device_list failed: %s",
                 esp_err_to_name(ret));
        free(bond_list);
        return false;
    }

    bool found = false;
    char bda_str[18];

    for (int i = 0; i < dev_num; ++i) {
        snprintf(bda_str, sizeof(bda_str),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 bond_list[i].bd_addr[0], bond_list[i].bd_addr[1],
                 bond_list[i].bd_addr[2], bond_list[i].bd_addr[3],
                 bond_list[i].bd_addr[4], bond_list[i].bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "bonded[%d] = %s", i, bda_str);

        if (memcmp(peer_bda, bond_list[i].bd_addr, ESP_BD_ADDR_LEN) == 0) {
            found = true;
        }
    }

    free(bond_list);
    return found;
}

static void airmouse_ble_clear_hid_ready_state(void)
{
    s_ble_hid_ctx.sec_conn = false;
    s_ble_hid_ctx.runtime_config_mode = false;
    s_ble_hid_ctx.mouse_notify_enabled = false;
    s_ble_hid_ctx.abs_mouse_notify_enabled = false;
    s_ble_hid_ctx.key_notify_enabled = false;
    s_ble_hid_ctx.cc_notify_enabled = false;
    s_ble_hid_ctx.peer_bonded = false;
    s_ble_hid_ctx.cccd_seen_this_connection = false;
    s_ble_hid_ctx.cccd_restored_from_nvs = false;
    s_ble_hid_ctx.has_peer = false;
    memset(s_ble_hid_ctx.peer_bda, 0, sizeof(s_ble_hid_ctx.peer_bda));
    s_ble_hid_ctx.peer_addr_type = 0;
}

static uint8_t hidd_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = ESP_BLE_GAP_CONN_ITVL_MS(7.5), //slave connection min interval
    .max_interval = ESP_BLE_GAP_CONN_ITVL_MS(20), //slave connection max interval
    .appearance = 0x03c0,       //HID Generic,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min        = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max        = ESP_BLE_GAP_ADV_ITVL_MS(30),
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void airmouse_ble_restore_cccd_task(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(CONFIG_AIRMOUSE_BLE_CCCD_RESTORE_DELAY_MS));

    if (!s_ble_hid_ctx.sec_conn) {
        ESP_LOGI(HID_DEMO_TAG, "skip CCCD restore because disconnected");
        vTaskDelete(NULL);
        return;
    }

    if (!s_ble_hid_ctx.has_peer) {
        ESP_LOGI(HID_DEMO_TAG, "skip CCCD restore: no current peer");
        vTaskDelete(NULL);
        return;
    }

    if (s_ble_hid_ctx.cccd_seen_this_connection) {
        ESP_LOGI(HID_DEMO_TAG,
                 "skip NVS restore because CCCD was written in this connection");
        vTaskDelete(NULL);
        return;
    }

    if (!s_ble_hid_ctx.peer_bonded) {
        ESP_LOGI(HID_DEMO_TAG, "skip NVS restore: peer is not bonded");
        vTaskDelete(NULL);
        return;
    }

    const char prefixes[] = {'m', 'k', 'c', 'a'};
    bool values[4] = {false, false, false, false};
    bool founds[4] = {false, false, false, false};
    bool any_found = false;
    char key[20];

    for (size_t i = 0; i < 4; ++i) {
        if (!airmouse_ble_make_peer_cccd_key(key,
                                             sizeof(key),
                                             prefixes[i],
                                             s_ble_hid_ctx.peer_bda)) {
            continue;
        }

        values[i] = airmouse_ble_nvs_get_bool(key, false, &founds[i]);
        if (founds[i]) {
            any_found = true;
        }
    }

    if (!any_found) {
        ESP_LOGI(HID_DEMO_TAG,
                 "no per-peer CCCD history, wait for host CCCD write");
        vTaskDelete(NULL);
        return;
    }

    s_ble_hid_ctx.mouse_notify_enabled = values[0];
    s_ble_hid_ctx.key_notify_enabled = values[1];
    s_ble_hid_ctx.cc_notify_enabled = values[2];
    s_ble_hid_ctx.abs_mouse_notify_enabled = values[3];
    s_ble_hid_ctx.cccd_restored_from_nvs = true;

    ESP_LOGI(HID_DEMO_TAG,
             "restore CCCD from per-peer NVS after timeout: "
             "mouse=%d key=%d cc=%d abs=%d",
             s_ble_hid_ctx.mouse_notify_enabled,
             s_ble_hid_ctx.key_notify_enabled,
             s_ble_hid_ctx.cc_notify_enabled,
             s_ble_hid_ctx.abs_mouse_notify_enabled);

    vTaskDelete(NULL);
}

// -----------------------------
// GAP callback
// -----------------------------
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
            ESP_LOGD(HID_DEMO_TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
        }
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(HID_DEMO_TAG, "remote BD_ADDR: %08x%04x",
                 (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                 (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(HID_DEMO_TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(HID_DEMO_TAG, "pair status = %s",
                 param->ble_security.auth_cmpl.success ? "success" : "fail");

        if (param->ble_security.auth_cmpl.success) {
            memcpy(s_ble_hid_ctx.peer_bda, bd_addr, ESP_BD_ADDR_LEN);
            s_ble_hid_ctx.peer_addr_type = param->ble_security.auth_cmpl.addr_type;
            s_ble_hid_ctx.has_peer = true;
            s_ble_hid_ctx.sec_conn = true;
            ESP_LOGI(HID_DEMO_TAG, "secure connection established, sec_conn = 1");

            bool peer_bonded = airmouse_ble_is_peer_bonded(bd_addr);
            s_ble_hid_ctx.peer_bonded = peer_bonded;

            char bda_str[18];
            snprintf(bda_str, sizeof(bda_str),
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     bd_addr[0], bd_addr[1], bd_addr[2],
                     bd_addr[3], bd_addr[4], bd_addr[5]);
            ESP_LOGI(HID_DEMO_TAG, "current peer %s bonded=%d",
                     bda_str, peer_bonded);

            ESP_LOGI(HID_DEMO_TAG, "start delayed CCCD restore, delay=%d ms",
                     CONFIG_AIRMOUSE_BLE_CCCD_RESTORE_DELAY_MS);
            BaseType_t task_ret = xTaskCreate(airmouse_ble_restore_cccd_task,
                                              "hid_cccd_restore",
                                              3072,
                                              NULL,
                                              5,
                                              NULL);
            if (task_ret != pdPASS) {
                ESP_LOGW(HID_DEMO_TAG, "failed to create hid_cccd_restore task");
            }
        } else {
            ESP_LOGE(HID_DEMO_TAG, "pairing failed, reason = 0x%x",
                     param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    }

    default:
        break;
    }
}

// -----------------------------
// HIDD callback
// -----------------------------
static void hidd_event_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param)
{
    switch (event) {
    case ESP_HIDD_EVENT_REG_FINISH:
        if (param->init_finish.state == ESP_HIDD_INIT_OK) {
            esp_ble_gap_set_device_name(HIDD_DEVICE_NAME);
            esp_ble_gap_config_adv_data(&hidd_adv_data);
        }
        break;

    case ESP_HIDD_EVENT_BLE_CONNECT:
        ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_CONNECT, conn_id=%u",
                 param->connect.conn_id);
        s_ble_hid_ctx.hid_conn_id = param->connect.conn_id;
        s_ble_hid_ctx.sec_conn = false;
        s_ble_hid_ctx.mouse_notify_enabled = false;
        s_ble_hid_ctx.abs_mouse_notify_enabled = false;
        s_ble_hid_ctx.key_notify_enabled = false;
        s_ble_hid_ctx.cc_notify_enabled = false;
        s_ble_hid_ctx.peer_bonded = false;
        s_ble_hid_ctx.cccd_seen_this_connection = false;
        s_ble_hid_ctx.cccd_restored_from_nvs = false;
        s_ble_hid_ctx.has_peer = false;
        memset(s_ble_hid_ctx.peer_bda, 0, sizeof(s_ble_hid_ctx.peer_bda));
        s_ble_hid_ctx.peer_addr_type = 0;
        break;

    case ESP_HIDD_EVENT_BLE_DISCONNECT:
        ESP_LOGI(HID_DEMO_TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
        airmouse_ble_clear_hid_ready_state();
        s_ble_hid_ctx.hid_conn_id = 0;
        esp_ble_gap_start_advertising(&hidd_adv_params);
        break;

    case ESP_HIDD_EVENT_BLE_REPORT_CCCD_WRITE_EVT:
        if (param == NULL) {
            ESP_LOGW(HID_DEMO_TAG, "ignore CCCD write with NULL param");
            break;
        }

        if (param->report_cccd_write.conn_id != s_ble_hid_ctx.hid_conn_id) {
            ESP_LOGW(HID_DEMO_TAG, "ignore CCCD write from stale conn_id=%u",
                     param->report_cccd_write.conn_id);
            break;
        }

        if (param->report_cccd_write.report_type != HID_REPORT_TYPE_INPUT) {
            break;
        }

        s_ble_hid_ctx.cccd_seen_this_connection = true;

        char prefix = '\0';
        const char *report_name = NULL;
        bool enabled = param->report_cccd_write.enabled;

        switch (param->report_cccd_write.report_id) {
        case HID_RPT_ID_MOUSE_IN:
            s_ble_hid_ctx.mouse_notify_enabled = enabled;
            prefix = 'm';
            report_name = "mouse";
            ESP_LOGI(HID_DEMO_TAG, "mouse_notify_enabled = %d",
                     s_ble_hid_ctx.mouse_notify_enabled);
            break;

        case HID_RPT_ID_ABS_MOUSE_IN:
            s_ble_hid_ctx.abs_mouse_notify_enabled = enabled;
            prefix = 'a';
            report_name = "abs_mouse";
            ESP_LOGI(HID_DEMO_TAG, "abs_mouse_notify_enabled = %d",
                     s_ble_hid_ctx.abs_mouse_notify_enabled);
            break;

        case HID_RPT_ID_KEY_IN:
            s_ble_hid_ctx.key_notify_enabled = enabled;
            prefix = 'k';
            report_name = "key";
            ESP_LOGI(HID_DEMO_TAG, "key_notify_enabled = %d",
                     s_ble_hid_ctx.key_notify_enabled);
            break;

        case HID_RPT_ID_CC_IN:
            s_ble_hid_ctx.cc_notify_enabled = enabled;
            prefix = 'c';
            report_name = "cc";
            ESP_LOGI(HID_DEMO_TAG, "cc_notify_enabled = %d",
                     s_ble_hid_ctx.cc_notify_enabled);
            break;

        default:
            break;
        }

        if (prefix != '\0') {
            ESP_LOGI(HID_DEMO_TAG,
                     "CCCD write from host, override per-peer NVS: "
                     "report=%s enabled=%d",
                     report_name, enabled);

            if (s_ble_hid_ctx.has_peer) {
                char key[20];
                if (airmouse_ble_make_peer_cccd_key(key,
                                                    sizeof(key),
                                                    prefix,
                                                    s_ble_hid_ctx.peer_bda)) {
                    ESP_LOGI(HID_DEMO_TAG, "save CCCD key=%s enabled=%d",
                             key, enabled);
                    airmouse_ble_nvs_set_bool(key, enabled);
                }
            }
        }

        break;

    default:
        break;
    }
}

// -----------------------------
// Public initialization entry
// -----------------------------
static esp_err_t airmouse_ble_init_host_stack(void)
{
    esp_err_t ret;

    // Initialize and start Bluedroid.
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s init bluedroid failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable bluedroid failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(HID_DEMO_TAG, "Bluedroid host initialized");

    ret = esp_hidd_profile_init();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s init hid profile failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    // Register the callback function to the GAP module.
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s register GAP callback failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_hidd_register_callbacks(hidd_event_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s register HIDD callback failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    /* Set the security iocap, auth_req, key size and key response parameters. */
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND;  // Bond with peer device after authentication.
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;        // Set the IO capability to no output and no input.
    uint8_t key_size = 16;                           // The key size should be 7~16 bytes.
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    ESP_LOGI(HID_DEMO_TAG, "AirMouse BLE HID service initialized");

    return ESP_OK;
}

#if CONFIG_IDF_TARGET_ESP32P4
static esp_err_t airmouse_ble_init_hosted_bluedroid(void)
{
    esp_err_t ret;
    esp_err_t bt_ctrl_init_ret;
    esp_err_t bt_ctrl_enable_ret;

    ESP_LOGI(HID_DEMO_TAG,
             "AirMouse BLE mode: ESP32-P4 + ESP32-C6 Hosted Bluedroid BLE");
    ESP_LOGI(HID_DEMO_TAG, "Bluetooth controller is disabled on P4");

    ret = esp_hosted_connect_to_slave();
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "esp_hosted_connect_to_slave failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    bt_ctrl_init_ret = esp_hosted_bt_controller_init();
    if (bt_ctrl_init_ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "esp_hosted_bt_controller_init failed: %s",
                 esp_err_to_name(bt_ctrl_init_ret));
    }

    bt_ctrl_enable_ret = esp_hosted_bt_controller_enable();
    if (bt_ctrl_enable_ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "esp_hosted_bt_controller_enable failed: %s",
                 esp_err_to_name(bt_ctrl_enable_ret));
    } else {
        ESP_LOGI(HID_DEMO_TAG, "ESP-Hosted Bluetooth support enabled");
    }

    hosted_hci_bluedroid_open();

    esp_bluedroid_hci_driver_operations_t operations = {
        .send = hosted_hci_bluedroid_send,
        .check_send_available = hosted_hci_bluedroid_check_send_available,
        .register_host_callback = hosted_hci_bluedroid_register_host_callback,
    };
    ret = esp_bluedroid_attach_hci_driver(&operations);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "esp_bluedroid_attach_hci_driver failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    if (bt_ctrl_init_ret != ESP_OK || bt_ctrl_enable_ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG,
                 "Hosted BT controller setup did not fully acknowledge, continuing with Bluedroid host init");
    }

    return airmouse_ble_init_host_stack();
}
#else
static esp_err_t airmouse_ble_init_local_controller(void)
{
    esp_err_t ret;

    ESP_LOGI(HID_DEMO_TAG, "AirMouse BLE mode: local controller");

    // Keep BLE only and release Classic BT memory.
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize and start the BT controller.
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s initialize controller failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "%s enable controller failed: %s",
                 __func__, esp_err_to_name(ret));
        return ret;
    }

    return airmouse_ble_init_host_stack();
}
#endif

esp_err_t airmouse_ble_init(void)
{
    esp_err_t ret;

#if CONFIG_IDF_TARGET_ESP32P4
    ret = airmouse_ble_init_hosted_bluedroid();
#else
    ret = airmouse_ble_init_local_controller();
#endif

    if (ret != ESP_OK) {
        ESP_LOGE(HID_DEMO_TAG, "airmouse_ble_init failed: %s",
                 esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t airmouse_ble_deinit(void)
{
    esp_err_t ret = ESP_OK;
    esp_err_t step_ret = ESP_OK;

    airmouse_ble_clear_hid_ready_state();
    s_ble_hid_ctx.hid_conn_id = 0;

    step_ret = esp_hidd_profile_deinit();
    if (step_ret != ESP_OK) {
        ESP_LOGW(HID_DEMO_TAG, "esp_hidd_profile_deinit failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }

    step_ret = esp_bluedroid_disable();
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_bluedroid_disable failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }

    step_ret = esp_bluedroid_deinit();
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_bluedroid_deinit failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }

#if CONFIG_IDF_TARGET_ESP32P4
    hosted_hci_bluedroid_close();

    step_ret = esp_hosted_bt_controller_disable();
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_hosted_bt_controller_disable failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }

    step_ret = esp_hosted_bt_controller_deinit(false);
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_hosted_bt_controller_deinit failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }
#else
    step_ret = esp_bt_controller_disable();
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_bt_controller_disable failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }

    step_ret = esp_bt_controller_deinit();
    if (step_ret != ESP_OK &&
            step_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(HID_DEMO_TAG, "esp_bt_controller_deinit failed: %s",
                 esp_err_to_name(step_ret));
        ret = step_ret;
    }
#endif

    return ret;
}

airmouse_ble_handle_t *airmouse_ble_hid_get_ctx(void)
{
    return &s_ble_hid_ctx;
}
