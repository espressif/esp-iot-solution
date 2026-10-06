/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief ESL Profile AP OTP Object Client (image transfer)
 *
 *  Pattern follows examples/bluetooth/ble_profiles/ble_otp/ble_otp_client write flow:
 *  Go To -> read_object_info -> write_object -> send_data on channel connect.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_eslp_ap.h"
#include "esp_otp.h"
#include "esp_eslp_otp_priv.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "ble_eslp_otp_ap";

typedef enum {
    ESLP_XFER_IDLE = 0,
    ESLP_XFER_GOTO,
    ESLP_XFER_WRITE,
    ESLP_XFER_SENDING,
} eslp_xfer_phase_t;

typedef struct {
    bool active;
    eslp_xfer_phase_t phase;
    uint16_t conn_handle;
    uint8_t image_index;
    const uint8_t *data;
    uint32_t data_len;
    uint32_t progress;
    uint16_t mtu;
    esp_ble_otp_transfer_info_t transfer_info;
} eslp_ap_image_xfer_t;

static bool s_inited;
static bool s_ots_discovered;
static eslp_ap_image_xfer_t s_xfer;

static void eslp_ap_xfer_finish(esp_err_t status);

static bool eslp_ap_xfer_matches_conn(uint16_t conn_handle)
{
    return s_xfer.active && s_xfer.conn_handle == conn_handle;
}

static bool eslp_ap_xfer_matches_transfer(const esp_ble_otp_transfer_info_t *ti)
{
    return ti && eslp_ap_xfer_matches_conn(ti->conn_handle);
}

static void eslp_ap_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS || !s_inited) {
        return;
    }
    if (id == ESP_BLE_CONN_EVENT_DISCONNECTED) {
        const esp_ble_conn_event_data_t *ev = event_data;
        uint16_t conn = ev ? ev->disconnected.conn_handle : BLE_CONN_HANDLE_INVALID;
        /* Single-connection OTS discovery flag: clear when the xfer/discovered peer drops. */
        if (!s_xfer.active || eslp_ap_xfer_matches_conn(conn)) {
            s_ots_discovered = false;
        }
        if (eslp_ap_xfer_matches_conn(conn)) {
            eslp_ap_xfer_finish(ESP_ERR_INVALID_STATE);
        }
    }
}

static void eslp_ap_xfer_finish(esp_err_t status)
{
    if (!s_xfer.active) {
        return;
    }

    esp_ble_eslp_ap_image_transferred_t ev = {
        .conn_handle = s_xfer.conn_handle,
        .image_index = s_xfer.image_index,
        .status = status,
    };
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, BLE_ESLP_AP_EVENT_IMAGE_TRANSFERRED,
                                   &ev, sizeof(ev), portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post IMAGE_TRANSFERRED failed: %s", esp_err_to_name(ret));
    }

    memset(&s_xfer, 0, sizeof(s_xfer));
}

static esp_err_t eslp_ap_send_next_chunk(void)
{
    if (!s_xfer.active || s_xfer.phase != ESLP_XFER_SENDING) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_xfer.progress >= s_xfer.data_len) {
        return esp_ble_otp_client_disconnect_transfer_channel(&s_xfer.transfer_info);
    }

    uint32_t remaining = s_xfer.data_len - s_xfer.progress;
    uint16_t mtu = s_xfer.mtu ? s_xfer.mtu : BLE_OTP_L2CAP_COC_MTU_DEFAULT;
    uint16_t chunk = (uint16_t)MIN(remaining, mtu);

    esp_err_t ret = esp_ble_otp_client_send_data(&s_xfer.transfer_info,
                                                 &s_xfer.data[s_xfer.progress], chunk);
    if (ret == ESP_OK) {
        s_xfer.progress += chunk;
    }
    return ret;
}

static void eslp_ap_otp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_OTP_EVENTS || !event_data || !s_inited) {
        return;
    }

    esp_ble_otp_event_data_t *evt = event_data;

    switch (id) {
    case BLE_OTP_EVENT_OTS_DISCOVERED:
        s_ots_discovered = true;
        ESP_LOGI(TAG, "OTS discovered on peer");
        break;

    case BLE_OTP_EVENT_OTS_DISCOVERY_FAILED:
        s_ots_discovered = false;
        if (s_xfer.active) {
            ESP_LOGE(TAG, "OTS discovery failed during transfer");
            eslp_ap_xfer_finish(ESP_FAIL);
        }
        break;

    case BLE_OTP_EVENT_OLCP_RESPONSE:
        if (!eslp_ap_xfer_matches_conn(evt->olcp_response.conn_handle) ||
                s_xfer.phase != ESLP_XFER_GOTO) {
            break;
        }
        if (evt->olcp_response.response.req_op_code != BLE_OTS_OLCP_GO_TO) {
            break;
        }
        if (evt->olcp_response.response.rsp_code != BLE_OTS_OLCP_RSP_SUCCESS) {
            ESP_LOGE(TAG, "Go To failed, rsp=0x%02x", evt->olcp_response.response.rsp_code);
            eslp_ap_xfer_finish(ESP_FAIL);
            break;
        }
        {
            esp_ble_otp_object_info_t info = {0};
            esp_err_t ret = esp_ble_otp_client_read_object_info(s_xfer.conn_handle, &info);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "read_object_info failed: %s", esp_err_to_name(ret));
                eslp_ap_xfer_finish(ret);
                break;
            }
            if (s_xfer.data_len > info.object_size.allocated_size) {
                ESP_LOGE(TAG, "image too large (%lu > %lu)",
                         (unsigned long)s_xfer.data_len,
                         (unsigned long)info.object_size.allocated_size);
                eslp_ap_xfer_finish(ESP_ERR_INVALID_SIZE);
                break;
            }
            s_xfer.phase = ESLP_XFER_WRITE;
            /* Truncate so smaller images resize Current Size. */
            ret = esp_ble_otp_client_write_object(s_xfer.conn_handle, 0, s_xfer.data_len,
                                                  BLE_OTP_WRITE_MODE_TRUNCATE);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "write_object failed: %s", esp_err_to_name(ret));
                eslp_ap_xfer_finish(ret);
            }
        }
        break;

    case BLE_OTP_EVENT_OACP_RESPONSE:
        if (!eslp_ap_xfer_matches_conn(evt->oacp_response.conn_handle) ||
                s_xfer.phase != ESLP_XFER_WRITE) {
            break;
        }
        if (evt->oacp_response.response.req_op_code != BLE_OTS_OACP_WRITE) {
            break;
        }
        if (evt->oacp_response.response.rsp_code != BLE_OTS_OACP_RSP_SUCCESS) {
            ESP_LOGE(TAG, "OACP Write failed, rsp=0x%02x", evt->oacp_response.response.rsp_code);
            eslp_ap_xfer_finish(ESP_FAIL);
        }
        /* Channel open is started by OTP on Write success; wait for CONNECTED */
        break;

    case BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED:
        if (!eslp_ap_xfer_matches_transfer(&evt->transfer_channel_connected.transfer_info)) {
            break;
        }
        s_xfer.transfer_info = evt->transfer_channel_connected.transfer_info;
        s_xfer.mtu = evt->transfer_channel_connected.transfer_info.mtu;
        s_xfer.phase = ESLP_XFER_SENDING;
        s_xfer.progress = 0;
        if (eslp_ap_send_next_chunk() != ESP_OK) {
            eslp_ap_xfer_finish(ESP_FAIL);
        }
        break;

    case BLE_OTP_EVENT_TRANSFER_DATA_SENT:
        if (!eslp_ap_xfer_matches_transfer(&evt->transfer_data_sent.transfer_info) ||
                s_xfer.phase != ESLP_XFER_SENDING) {
            break;
        }
        if (s_xfer.progress < s_xfer.data_len) {
            if (eslp_ap_send_next_chunk() != ESP_OK) {
                eslp_ap_xfer_finish(ESP_FAIL);
            }
        } else {
            (void)esp_ble_otp_client_disconnect_transfer_channel(&s_xfer.transfer_info);
        }
        break;

    case BLE_OTP_EVENT_TRANSFER_COMPLETE:
        if (eslp_ap_xfer_matches_transfer(&evt->transfer_complete.transfer_info)) {
            eslp_ap_xfer_finish(evt->transfer_complete.success ? ESP_OK : ESP_FAIL);
        }
        break;

    case BLE_OTP_EVENT_TRANSFER_ERROR:
        if (eslp_ap_xfer_matches_transfer(&evt->transfer_error.transfer_info)) {
            ESP_LOGE(TAG, "transfer error: %s", esp_err_to_name(evt->transfer_error.error));
            eslp_ap_xfer_finish(evt->transfer_error.error);
        }
        break;

    case BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED:
        /* Completion is reported via TRANSFER_COMPLETE; ignore stray closes. */
        break;

    default:
        break;
    }
}

esp_err_t esp_ble_eslp_ap_transfer_image(const esp_ble_eslp_ap_image_transfer_t *params)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!params || !params->data || params->data_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_xfer.active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_ots_discovered) {
        ESP_LOGW(TAG, "OTS not discovered yet; call after BLE_OTP_EVENT_OTS_DISCOVERED");
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_xfer, 0, sizeof(s_xfer));
    s_xfer.active = true;
    s_xfer.phase = ESLP_XFER_GOTO;
    s_xfer.conn_handle = params->conn_handle;
    s_xfer.image_index = params->image_index;
    s_xfer.data = params->data;
    s_xfer.data_len = params->data_len;
    s_xfer.mtu = BLE_OTP_L2CAP_COC_MTU_DEFAULT;

    esp_ble_ots_id_t object_id = {0};
    eslp_otp_image_index_to_id(params->image_index, &object_id);

    esp_err_t ret = esp_ble_otp_client_select_by_id(params->conn_handle, &object_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "select_by_id failed: %s", esp_err_to_name(ret));
        memset(&s_xfer, 0, sizeof(s_xfer));
        return ret;
    }

    ESP_LOGI(TAG, "transfer_image started (index=%u, len=%lu)",
             params->image_index, (unsigned long)params->data_len);
    return ESP_OK;
}

esp_err_t eslp_otp_ap_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_xfer, 0, sizeof(s_xfer));
    s_ots_discovered = false;

    esp_err_t ret = esp_ble_conn_l2cap_coc_mem_init();
    if (ret != ESP_OK) {
        return ret;
    }

    esp_ble_otp_config_t otp_cfg = {
        .role = BLE_OTP_ROLE_CLIENT,
        .psm = BLE_OTP_PSM_DEFAULT,
        .l2cap_coc_mtu = BLE_OTP_L2CAP_COC_MTU_DEFAULT,
        .auto_discover_ots = true,
    };
    ret = esp_ble_otp_init(&otp_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, eslp_ap_otp_event_handler, NULL);
    if (ret != ESP_OK) {
        (void)esp_ble_otp_deinit();
        return ret;
    }

    ret = esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, eslp_ap_conn_event_handler, NULL);
    if (ret != ESP_OK) {
        (void)esp_event_handler_unregister(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, eslp_ap_otp_event_handler);
        (void)esp_ble_otp_deinit();
        return ret;
    }

    s_inited = true;
    ESP_LOGI(TAG, "ESL OTP Object Client ready");
    return ESP_OK;
}

esp_err_t eslp_otp_ap_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_xfer.active) {
        eslp_ap_xfer_finish(ESP_ERR_INVALID_STATE);
    }
    (void)esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, eslp_ap_conn_event_handler);
    (void)esp_event_handler_unregister(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, eslp_ap_otp_event_handler);
    (void)esp_ble_otp_deinit();
    s_ots_discovered = false;
    s_inited = false;
    return ESP_OK;
}
