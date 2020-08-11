// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

#ifdef CONFIG_BT_ENABLED

/* c includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* esp includes */
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_bt_defs.h"
#include "esp_blufi_api.h"
#include "esp_gap_ble_api.h"

/* other includes */
#include "nvs_flash.h"
#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "esp32/rom/crc.h"
#include "iot_blufi.h"

static const char *TAG = "iot_blufi";

/*
   The SEC_TYPE_xxx is for self-defined packet data type in the procedure of "BLUFI negotiate key"
   If user use other negotiation procedure to exchange(or generate) key, should redefine the type by yourself.
 */
#define SEC_TYPE_DH_PARAM_LEN   0x00
#define SEC_TYPE_DH_PARAM_DATA  0x01

#define DH_SELF_PUB_KEY_LEN     128
#define DH_SELF_PUB_KEY_BIT_LEN (DH_SELF_PUB_KEY_LEN * 8)
#define SHARE_KEY_LEN           128
#define SHARE_KEY_BIT_LEN       (SHARE_KEY_LEN * 8)
#define PSK_LEN                 16

#define IOT_BLUFI_CHECK(con, err, format, ...) if(con){ \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        if(errno) { ESP_LOGE(TAG, "errno: %d, errno_str: %s", errno, strerror(errno)); } \
        return err; \
    }

#define IOT_BLUFI_CHECK_GOTO(con, lable, format, ...) if(con){ \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        if(errno) { ESP_LOGE(TAG, "errno: %d, errno_str: %s", errno, strerror(errno)); } \
        goto lable; \
    }

typedef struct blufi_security {
    uint8_t  self_public_key[DH_SELF_PUB_KEY_LEN];
    uint8_t  share_key[SHARE_KEY_LEN];
    size_t   share_len;
    uint8_t  psk[PSK_LEN];
    uint8_t  *dh_param;
    int      dh_param_len;
    uint8_t  iv[16];
    mbedtls_dhm_context dhm;
    mbedtls_aes_context aes;
} __attribute__((packed)) blufi_security_t;

typedef struct iot_blufi_data {
    uint8_t service_uuid128[32];
    blufi_security_t *blufi_sec;
    esp_ble_adv_data_t adv_data;
    esp_ble_adv_params_t adv_params;
    /* ble connect infor */
    uint8_t server_if;
    uint16_t conn_id;
    /* store the wifi configuration from ble */
    wifi_config_t sta_config;
    /* store the station info for send back to phone */
    bool sta_connected;
    uint8_t sta_bssid[6];
    uint8_t sta_ssid[32];
    int sta_ssid_len;
    esp_blufi_cb_event_t status;
} __attribute__((packed)) iot_blufi_data_t;

static xSemaphoreHandle g_iot_blufi_sem = NULL;
static iot_blufi_data_t *g_iot_blufi_data = NULL;

static int myrand(void *rng_state, unsigned char *output, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        output[i] = esp_random();
    }

    return (0);
}

extern void btc_blufi_report_error(esp_blufi_error_state_t state);

static void blufi_dh_negotiate_data_handler(uint8_t *data, int len,
        uint8_t **output_data, int *output_len, bool *need_free)
{
    int ret;
    uint8_t type = data[0];

    if (g_iot_blufi_data->blufi_sec == NULL) {
        ESP_LOGE(TAG, "BLUFI Security is not initialized");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    switch (type) {
        case SEC_TYPE_DH_PARAM_LEN: {
            g_iot_blufi_data->blufi_sec->dh_param_len = ((data[1] << 8) | data[2]);

            if (g_iot_blufi_data->blufi_sec->dh_param) {
                free(g_iot_blufi_data->blufi_sec->dh_param);
                g_iot_blufi_data->blufi_sec->dh_param = NULL;
            }

            g_iot_blufi_data->blufi_sec->dh_param = (uint8_t *)malloc(g_iot_blufi_data->blufi_sec->dh_param_len);

            if (g_iot_blufi_data->blufi_sec->dh_param == NULL) {
                ESP_LOGE(TAG, "g_iot_blufi_data->blufi_sec->dh_param malloc failed");
                btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
                return;
            }

            break;
        }

        case SEC_TYPE_DH_PARAM_DATA: {
            if (g_iot_blufi_data->blufi_sec->dh_param == NULL) {
                ESP_LOGE(TAG, "g_iot_blufi_data->blufi_sec->dh_param is NULL");
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }

            uint8_t *param = g_iot_blufi_data->blufi_sec->dh_param;
            memcpy(g_iot_blufi_data->blufi_sec->dh_param, &data[1],
                   g_iot_blufi_data->blufi_sec->dh_param_len);
            ret = mbedtls_dhm_read_params(&g_iot_blufi_data->blufi_sec->dhm, &param,
                                          &param[g_iot_blufi_data->blufi_sec->dh_param_len]);

            if (ret) {
                ESP_LOGE(TAG, "mbedtls_dhm_read_params failed, ret: %d", ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }

            free(g_iot_blufi_data->blufi_sec->dh_param);
            g_iot_blufi_data->blufi_sec->dh_param = NULL;
            ret = mbedtls_dhm_make_public(&g_iot_blufi_data->blufi_sec->dhm,
                                          (int) mbedtls_mpi_size(&g_iot_blufi_data->blufi_sec->dhm.P),
                                          g_iot_blufi_data->blufi_sec->self_public_key,
                                          g_iot_blufi_data->blufi_sec->dhm.len, myrand, NULL);

            if (ret) {
                ESP_LOGE(TAG, "mbedtls_dhm_make_public failed, ret: %d", ret);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            mbedtls_dhm_calc_secret(&g_iot_blufi_data->blufi_sec->dhm, g_iot_blufi_data->blufi_sec->share_key,
                                    SHARE_KEY_BIT_LEN, &g_iot_blufi_data->blufi_sec->share_len, NULL, NULL);
            mbedtls_md5_ret(g_iot_blufi_data->blufi_sec->share_key, g_iot_blufi_data->blufi_sec->share_len,
                            g_iot_blufi_data->blufi_sec->psk);
            mbedtls_aes_setkey_enc(&g_iot_blufi_data->blufi_sec->aes, g_iot_blufi_data->blufi_sec->psk, 128);

            /* alloc output data */
            *output_data = &g_iot_blufi_data->blufi_sec->self_public_key[0];
            *output_len = g_iot_blufi_data->blufi_sec->dhm.len;
            *need_free = false;
            break;
        }
    }
}

static int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, g_iot_blufi_data->blufi_sec->iv, sizeof(g_iot_blufi_data->blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&g_iot_blufi_data->blufi_sec->aes, MBEDTLS_AES_ENCRYPT,
                                   crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
    IOT_BLUFI_CHECK(ret != ESP_OK, ret, "mbedtls_aes_crypt_cfb128, ret: %d", ret);
    return crypt_len;
}

static int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    int ret;
    size_t iv_offset = 0;
    uint8_t iv0[16];

    memcpy(iv0, g_iot_blufi_data->blufi_sec->iv, sizeof(g_iot_blufi_data->blufi_sec->iv));
    iv0[0] = iv8; /* set iv8 as the iv0[0] */

    ret = mbedtls_aes_crypt_cfb128(&g_iot_blufi_data->blufi_sec->aes, MBEDTLS_AES_DECRYPT,
                                   crypt_len, &iv_offset, iv0, crypt_data, crypt_data);
    IOT_BLUFI_CHECK(ret != ESP_OK, ret, "mbedtls_aes_crypt_cfb128, ret: %d", ret);
    return crypt_len;
}

static uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    /* This iv8 ignore, not used */
    return crc16_be(0, data, len);
}

static esp_err_t iot_blufi_security_init(void)
{
    if (g_iot_blufi_data->blufi_sec) {
        return ESP_OK;
    }

    g_iot_blufi_data->blufi_sec = (blufi_security_t *)calloc(1, sizeof(blufi_security_t));
    IOT_BLUFI_CHECK(!g_iot_blufi_data->blufi_sec, ESP_FAIL, "malloc g_iot_blufi_data->blufi_sec error");

    mbedtls_dhm_init(&g_iot_blufi_data->blufi_sec->dhm);
    mbedtls_aes_init(&g_iot_blufi_data->blufi_sec->aes);
    memset(g_iot_blufi_data->blufi_sec->iv, 0x0, 16);
    return ESP_OK;
}

static void iot_blufi_security_deinit(void)
{
    if (!g_iot_blufi_data->blufi_sec) {
        return;
    }

    if (g_iot_blufi_data->blufi_sec->dh_param) {
        free(g_iot_blufi_data->blufi_sec->dh_param);
        g_iot_blufi_data->blufi_sec->dh_param = NULL;
    }

    mbedtls_dhm_free(&g_iot_blufi_data->blufi_sec->dhm);
    mbedtls_aes_free(&g_iot_blufi_data->blufi_sec->aes);
    free(g_iot_blufi_data->blufi_sec);
    g_iot_blufi_data->blufi_sec = NULL;
}

static esp_err_t system_event_handler(void *ctx, system_event_t *event)
{
    wifi_mode_t mode;
    ESP_LOGD(TAG, "system_event_handler, event_id: %d", event->event_id);

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP: {
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            esp_blufi_extra_info_t info = {0};
            memcpy(info.sta_bssid, g_iot_blufi_data->sta_config.sta.bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = g_iot_blufi_data->sta_config.sta.ssid;
            info.sta_ssid_len = strlen((char *)g_iot_blufi_data->sta_config.sta.ssid);
            esp_wifi_get_mode(&mode);
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);

            xSemaphoreGive(g_iot_blufi_sem);
            break;
        }

        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
            g_iot_blufi_data->sta_connected = true;
            memcpy(g_iot_blufi_data->sta_config.sta.bssid, event->event_info.connected.bssid, 6);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED: {
            system_event_sta_disconnected_t *disconnected = &event->event_info.disconnected;
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED, reason: %d", disconnected->reason);
            /* This is a workaround as ESP32 WiFi libs don't currently auto-reassociate. */
            g_iot_blufi_data->sta_connected = false;
            esp_wifi_connect();
            break;
        }

        default:
            break;
    }

    return ESP_OK;
}

esp_blufi_cb_event_t iot_blufi_get_status()
{
    if (!g_iot_blufi_data) {
        return IOT_BLUFI_STATUS_INVAILD;
    } else {
        return g_iot_blufi_data->status;
    }
}

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    g_iot_blufi_data->status = event;

    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH: {
            ESP_LOGI(TAG, "BLUFI init finish");
            char blufi_name[16] = {0};
            const uint8_t *addr = esp_bt_dev_get_address();
            sprintf(blufi_name, "BLUFI_%02x%02x%02x", addr[3], addr[4], addr[5]);
            esp_ble_gap_set_device_name(blufi_name);
            esp_ble_gap_config_adv_data(&g_iot_blufi_data->adv_data);
            break;
        }

        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(TAG, "BLUFI deinit finish");
            break;

        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "BLUFI ble connect");
            g_iot_blufi_data->server_if = param->connect.server_if;
            g_iot_blufi_data->conn_id = param->connect.conn_id;
            esp_ble_gap_stop_advertising();
            iot_blufi_security_init();
            break;

        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(TAG, "BLUFI ble disconnect");
            iot_blufi_security_deinit();
            esp_ble_gap_start_advertising(&g_iot_blufi_data->adv_params);
            break;

        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            ESP_LOGI(TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
            break;

        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            ESP_LOGI(TAG, "BLUFI requset wifi connect to AP");
            esp_wifi_disconnect();
            esp_wifi_connect();
            break;

        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(TAG, "BLUFI requset wifi disconnect from AP");
            esp_wifi_disconnect();
            break;

        case ESP_BLUFI_EVENT_REPORT_ERROR:
            ESP_LOGE(TAG, "BLUFI report error, error code %d", param->report_error.state);
            esp_blufi_send_error_info(param->report_error.state);
            break;

        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            ESP_LOGI(TAG, "BLUFI get wifi status from AP");
            wifi_mode_t mode;
            esp_blufi_extra_info_t info;
            esp_wifi_get_mode(&mode);

            if (g_iot_blufi_data->sta_connected) {
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, g_iot_blufi_data->sta_bssid, 6);
                info.sta_bssid_set = true;
                info.sta_ssid = g_iot_blufi_data->sta_ssid;
                info.sta_ssid_len = g_iot_blufi_data->sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            }

            break;
        }

        case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
            ESP_LOGI(TAG, "BLUFI close a gatt connection");
            esp_blufi_close(g_iot_blufi_data->server_if, g_iot_blufi_data->conn_id);
            break;

        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(g_iot_blufi_data->sta_config.sta.bssid, param->sta_bssid.bssid, 6);
            g_iot_blufi_data->sta_config.sta.bssid_set = 1;
            esp_wifi_set_config(WIFI_IF_STA, &g_iot_blufi_data->sta_config);
            ESP_LOGI(TAG, "Recv STA BSSID %s", g_iot_blufi_data->sta_config.sta.bssid);
            break;

        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy((char *)g_iot_blufi_data->sta_config.sta.ssid,
                    (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            g_iot_blufi_data->sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            esp_wifi_set_config(WIFI_IF_STA, &g_iot_blufi_data->sta_config);
            ESP_LOGI(TAG, "Recv STA SSID %s", g_iot_blufi_data->sta_config.sta.ssid);
            break;

        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char *)g_iot_blufi_data->sta_config.sta.password,
                    (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
            g_iot_blufi_data->sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            esp_wifi_set_config(WIFI_IF_STA, &g_iot_blufi_data->sta_config);
            ESP_LOGI(TAG, "Recv STA PASSWORD %s", g_iot_blufi_data->sta_config.sta.password);
            break;

        default:
            break;
    }
}

static void iot_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&g_iot_blufi_data->adv_params);
            break;

        default:
            break;
    }
}

static esp_blufi_callbacks_t iot_blufi_callbacks = {
    .event_cb = blufi_event_callback,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
};

static void iot_blufi_data_init()
{
    //first uuid, 16bit, [12],[13] is the value
    g_iot_blufi_data->service_uuid128[0] = 0xfb;  // LSB
    g_iot_blufi_data->service_uuid128[1] = 0x34;  //  ^
    g_iot_blufi_data->service_uuid128[2] = 0x9b;  //  |
    g_iot_blufi_data->service_uuid128[3] = 0x5f;  //  |
    g_iot_blufi_data->service_uuid128[4] = 0x80;  //  |
    g_iot_blufi_data->service_uuid128[5] = 0x00;  //  |
    g_iot_blufi_data->service_uuid128[6] = 0x00;  //  |
    g_iot_blufi_data->service_uuid128[7] = 0x80;  //  |
    g_iot_blufi_data->service_uuid128[8] = 0x00;  //  |
    g_iot_blufi_data->service_uuid128[9] = 0x10;  //  |
    g_iot_blufi_data->service_uuid128[10] = 0x00; //  |
    g_iot_blufi_data->service_uuid128[11] = 0x00; //  |
    g_iot_blufi_data->service_uuid128[12] = 0xFF; //  |
    g_iot_blufi_data->service_uuid128[13] = 0xFF; //  |
    g_iot_blufi_data->service_uuid128[14] = 0x00; //  V
    g_iot_blufi_data->service_uuid128[16] = 0x00; // MSB

    /* set blufi advertise data with default value */
    g_iot_blufi_data->adv_data.set_scan_rsp        = false;
    g_iot_blufi_data->adv_data.include_name        = true;
    g_iot_blufi_data->adv_data.include_txpower     = true;
    //slave connection min interval, Time = min_interval * 1.25 msec
    g_iot_blufi_data->adv_data.min_interval        = 0x0006;
    //slave connection max interval, Time = max_interval * 1.25 msec
    g_iot_blufi_data->adv_data.max_interval        = 0x0010;
    g_iot_blufi_data->adv_data.appearance          = 0x00;
    g_iot_blufi_data->adv_data.manufacturer_len    = 0;
    g_iot_blufi_data->adv_data.p_manufacturer_data = NULL;
    g_iot_blufi_data->adv_data.service_data_len    = 0;
    g_iot_blufi_data->adv_data.p_service_data      = NULL;
    g_iot_blufi_data->adv_data.service_uuid_len    = 16;
    g_iot_blufi_data->adv_data.p_service_uuid      = g_iot_blufi_data->service_uuid128;
    g_iot_blufi_data->adv_data.flag                = 0x6;

    /* set blufi advertise parameters with default value */
    g_iot_blufi_data->adv_params.adv_int_min       = 0x100;
    g_iot_blufi_data->adv_params.adv_int_max       = 0x100;
    g_iot_blufi_data->adv_params.adv_type          = ADV_TYPE_IND;
    g_iot_blufi_data->adv_params.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
    g_iot_blufi_data->adv_params.channel_map       = ADV_CHNL_ALL;
    g_iot_blufi_data->adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    /* set iot blufi status */
    g_iot_blufi_data->status = IOT_BLUFI_STATUS_INVAILD;
}

esp_err_t iot_blufi_start(bool release_classic_bt, uint32_t ticks_to_wait)
{
    IOT_BLUFI_CHECK(g_iot_blufi_data, ESP_FAIL,
                    "iot blufi has been started, call iot_blufi_stop first");

    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    g_iot_blufi_sem = xSemaphoreCreateBinary();
    IOT_BLUFI_CHECK(!g_iot_blufi_sem, ESP_FAIL, "create g_iot_blufi_sem fail");

    g_iot_blufi_data = calloc(1, sizeof(iot_blufi_data_t));

    if (!g_iot_blufi_data) {
        vSemaphoreDelete(g_iot_blufi_sem);
        g_iot_blufi_sem = NULL;

        ESP_LOGW(TAG, "calloc g_iot_blufi_data error");
        return ESP_FAIL;
    }

    iot_blufi_data_init();

    esp_event_loop_set_cb(system_event_handler, NULL);

    if (release_classic_bt) {
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    }

    ret = esp_bt_controller_init(&bt_cfg);
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_bt_controller_init, ret: %d", ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_bt_controller_enable, ret: %d", ret);

    ret = esp_bluedroid_init();
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_bluedroid_init, ret: %d", ret);

    ret = esp_bluedroid_enable();
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_bluedroid_enable, ret: %d", ret);

    ret = esp_ble_gap_register_callback(iot_ble_gap_event_handler);
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_ble_gap_register_callback, ret: %d", ret);

    ret = esp_blufi_register_callbacks(&iot_blufi_callbacks);
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_blufi_register_callbacks, ret: %d", ret);

    ret = esp_blufi_profile_init();
    IOT_BLUFI_CHECK_GOTO(ret != ESP_OK, ERR_EXIT, "esp_blufi_profile_init, ret: %d", ret);

    ESP_LOGI(TAG, "BD ADDR: "ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    ESP_LOGI(TAG, "BLUFI VERSION: %04x", esp_blufi_get_version());

    if (ticks_to_wait == 0) {
        return ESP_OK;
    } else if (xSemaphoreTake(g_iot_blufi_sem, ticks_to_wait) != pdTRUE) {
        xSemaphoreGive(g_iot_blufi_sem);
        return ESP_ERR_TIMEOUT;
    }

    xSemaphoreGive(g_iot_blufi_sem);
    return ESP_OK;

ERR_EXIT:
    free(g_iot_blufi_data);
    g_iot_blufi_data = NULL;

    vSemaphoreDelete(g_iot_blufi_sem);
    g_iot_blufi_sem = NULL;

    return ESP_FAIL;
}

esp_err_t iot_blufi_stop(bool release_ble)
{
    IOT_BLUFI_CHECK(!g_iot_blufi_data, ESP_FAIL,
                    "iot blufi not started, call iot_blufi_start first");

    esp_err_t ret;

    xSemaphoreGive(g_iot_blufi_sem);

    ret = esp_blufi_profile_deinit();
    IOT_BLUFI_CHECK(ret != ESP_OK, ESP_FAIL, "esp_blufi_profile_deinit, ret: %d", ret);

    iot_blufi_security_deinit();

    ret = esp_bluedroid_disable();
    IOT_BLUFI_CHECK(ret != ESP_OK, ESP_FAIL, "esp_bluedroid_disable, ret: %d", ret);

    ret = esp_bluedroid_deinit();
    IOT_BLUFI_CHECK(ret != ESP_OK, ESP_FAIL, "esp_bluedroid_deinit, ret: %d", ret);

    ret = esp_bt_controller_disable();
    IOT_BLUFI_CHECK(ret != ESP_OK, ESP_FAIL, "esp_bt_controller_disable, ret: %d", ret);

    ret = esp_bt_controller_deinit();
    IOT_BLUFI_CHECK(ret != ESP_OK, ESP_FAIL, "esp_bt_controller_deinit, ret: %d", ret);

    if (release_ble) {
        esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    }

    esp_event_loop_set_cb(NULL, NULL);

    free(g_iot_blufi_data);
    g_iot_blufi_data = NULL;

    vSemaphoreDelete(g_iot_blufi_sem);
    g_iot_blufi_sem = NULL;

    return ESP_OK;
}

#endif /**< CONFIG_BT_ENABLED */