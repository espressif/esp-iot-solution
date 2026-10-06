/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Electronic Shelf Label AP Profile
 */

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_eslp_ap.h"
#include "esp_eslp_otp_priv.h"
#include "esp_eslp_ap_lifecycle_priv.h"
#include "esp_eslp_ap_cmd_priv.h"
#include "esp_eslp_ead_priv.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "ble_eslp_ap";

ESP_EVENT_DEFINE_BASE(BLE_ESLP_AP_EVENTS);

#ifndef CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS
#define CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS 16
#endif

typedef struct {
    bool valid;
    uint8_t len;
    uint8_t rsp_slot_start;
    uint8_t rsp_slot_count;
    uint8_t data[BLE_ESLP_EAD_OUTER_MAX_SIZE];
} eslp_ap_subev_cmd_t;

typedef struct {
    bool inited;
    bool pawr_running;
    bool has_ap_sync_key;
    esp_ble_esl_key_material_t ap_sync_key;
    eslp_ap_subev_cmd_t subev[CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS];
} eslp_ap_ctx_t;

static eslp_ap_ctx_t s_ap;

static void ap_post(esp_ble_eslp_ap_event_t id, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, id, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post AP event %d: %s", (int)id, esp_err_to_name(ret));
    }
}

static esp_err_t ap_write_uuid16(uint16_t conn_handle, uint16_t uuid16, const void *data, uint16_t len)
{
    esp_ble_conn_data_t buff = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = {
            .uuid16 = uuid16,
        },
        .data = (uint8_t *)data,
        .data_len = len,
    };
    return esp_ble_conn_write_by_handle(conn_handle, &buff);
}

esp_err_t esp_ble_eslp_ap_tag_config_default(esp_ble_eslp_ap_tag_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(cfg, 0, sizeof(*cfg));
    cfg->address.esl_id = 0x00;
    BLE_ESL_ADDR_SET_GROUP_ID(cfg->address, 0);
    cfg->subscribe_ecp = true;
    cfg->send_update_complete = false; /* App should OTP/ECP then call update_complete */
    return ESP_OK;
}

esp_err_t esp_ble_eslp_ap_configure_tag(uint16_t conn_handle, const esp_ble_eslp_ap_tag_config_t *cfg)
{
    if (!s_ap.inited || !cfg) {
        return !cfg ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }
    if (cfg->address.esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        ESP_LOGE(TAG, "ESL_ID 0xFF (Broadcast) is not allowed for Address write");
        return ESP_ERR_INVALID_ARG;
    }

    bool encrypted = false;
    if (esp_ble_conn_get_sec_state(conn_handle, &encrypted, NULL, NULL) != ESP_OK || !encrypted) {
        ESP_LOGE(TAG, "configure_tag requires an encrypted link");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;
    esp_ble_esl_abs_time_t abs_time = cfg->abs_time;
    if (abs_time == 0) {
        abs_time = (esp_ble_esl_abs_time_t)esp_log_timestamp();
    }

    ret = ap_write_uuid16(conn_handle, BLE_ESL_CHR_UUID16_ADDRESS, &cfg->address, sizeof(cfg->address));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write ADDRESS failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ap_write_uuid16(conn_handle, BLE_ESL_CHR_UUID16_AP_SYNC_KEY, &cfg->ap_sync_key, sizeof(cfg->ap_sync_key));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write AP_SYNC_KEY failed: %s", esp_err_to_name(ret));
        return ret;
    }
    s_ap.ap_sync_key = cfg->ap_sync_key;
    s_ap.has_ap_sync_key = true;

    ret = ap_write_uuid16(conn_handle, BLE_ESL_CHR_UUID16_RESP_KEY, &cfg->resp_key, sizeof(cfg->resp_key));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write RESP_KEY failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ap_write_uuid16(conn_handle, BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME, &abs_time, sizeof(abs_time));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write ABS_TIME failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (cfg->subscribe_ecp) {
        uint16_t cccd = 0x0001; /* Notify */
        esp_ble_conn_data_t sub = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = BLE_ESL_CHR_UUID16_ECP,
            },
            .data = (uint8_t *) &cccd,
            .data_len = sizeof(cccd),
        };
        ret = esp_ble_conn_subscribe_by_handle(conn_handle, ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "subscribe ECP indicate failed: %s", esp_err_to_name(ret));
        }
    }

    if (cfg->send_update_complete) {
        uint8_t update_complete[2] = { BLE_ESL_CMD_UPDATE_COMPLETE, cfg->address.esl_id };
        ret = ap_write_uuid16(conn_handle, BLE_ESL_CHR_UUID16_ECP, update_complete, sizeof(update_complete));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "write UPDATE_COMPLETE failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    ret = eslp_ap_lifecycle_on_configured(conn_handle, &cfg->address, &cfg->resp_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "eslp_ap_lifecycle_on_configured failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_ble_eslp_ap_tag_configured_t ev = {
        .conn_handle = conn_handle,
        .address = cfg->address,
    };
    ap_post(BLE_ESLP_AP_EVENT_TAG_CONFIGURED, &ev, sizeof(ev));
    return ESP_OK;
}

esp_err_t esp_ble_eslp_ap_write_ecp(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || len == 0 || len > BLE_ESL_ECP_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t esl_id = 0xFF;
    uint8_t group_id = 0;
    if (len >= 2) {
        esl_id = data[1];
    }
    esp_ble_esl_address_t addr = {0};
    if (eslp_ap_lifecycle_get_address(conn_handle, &addr) == ESP_OK) {
        group_id = (uint8_t)BLE_ESL_ADDR_GROUP_ID(addr);
        if (esl_id == 0xFF) {
            esl_id = addr.esl_id;
        }
    }
    return eslp_ap_cmd_write_ecp(conn_handle, data, (uint8_t)len, esl_id, group_id);
}

esp_err_t esp_ble_eslp_ap_pawr_start(const esp_ble_eslp_ap_pawr_config_t *pawr)
{
#if !defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
    (void)pawr;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
    esp_ble_conn_pawr_params_t params = {0};
    if (pawr) {
        if (pawr->num_subevents == 0 ||
                pawr->num_subevents > CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS) {
            return ESP_ERR_INVALID_ARG;
        }
        params.include_tx_power = pawr->include_tx_power;
        params.itvl_min = pawr->itvl_min;
        params.itvl_max = pawr->itvl_max;
        params.num_subevents = pawr->num_subevents;
        params.subevent_interval = pawr->subevent_interval;
        params.response_slot_delay = pawr->response_slot_delay;
        params.response_slot_spacing = pawr->response_slot_spacing;
        params.num_response_slots = pawr->num_response_slots;
    } else {
        params.itvl_min = 2400;
        params.itvl_max = 2400;
        params.num_subevents = 10;
        params.subevent_interval = 44;
        params.response_slot_delay = 20;
        params.response_slot_spacing = 32;
        params.num_response_slots = 5;
    }
    esp_err_t ret = esp_ble_conn_pawr_params_set(&params);
    if (ret != ESP_OK) {
        return ret;
    }
#else
    (void)pawr;
    esp_err_t ret;
#endif

    esp_ble_conn_adv_params_t adv_params = {0};
    adv_params.adv_event_properties = 0;
    adv_params.primary_phy = ESP_BLE_CONN_PHY_1M;
    adv_params.secondary_phy = ESP_BLE_CONN_PHY_1M;
    adv_params.sid = 0;
    adv_params.itvl_min = 0x60;
    adv_params.itvl_max = 0x60;
    (void)esp_ble_conn_adv_params_set(&adv_params);

    ret = esp_ble_conn_adv_start();
    if (ret != ESP_OK) {
        return ret;
    }
    s_ap.pawr_running = true;
    ap_post(BLE_ESLP_AP_EVENT_PAWR_STARTED, NULL, 0);
    return ESP_OK;
#endif
}

esp_err_t esp_ble_eslp_ap_pawr_stop(void)
{
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_ble_conn_adv_stop();
    s_ap.pawr_running = false;
    ap_post(BLE_ESLP_AP_EVENT_PAWR_STOPPED, NULL, 0);
    return ret;
}

esp_err_t esp_ble_eslp_ap_sync_transfer(uint16_t conn_handle, uint16_t service_data)
{
#if !defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
    (void)conn_handle;
    (void)service_data;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_ble_conn_periodic_sync_set_info(conn_handle, service_data);
    if (ret == ESP_OK) {
        (void)eslp_ap_lifecycle_on_sync_transferred(conn_handle);
        ap_post(BLE_ESLP_AP_EVENT_SYNC_TRANSFERRED, &conn_handle, sizeof(conn_handle));
    }
    return ret;
#endif
}

esp_err_t esp_ble_eslp_ap_queue_pawr_command(uint8_t subevent, const uint8_t *tlv, uint8_t tlv_len,
                                             uint8_t rsp_slot_start, uint8_t rsp_slot_count)
{
#if !defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
    (void)subevent;
    (void)tlv;
    (void)tlv_len;
    (void)rsp_slot_start;
    (void)rsp_slot_count;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!tlv || tlv_len == 0 || (uint16_t)(1 + tlv_len) > BLE_ESLP_PAYLOAD_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    if (subevent >= CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ap.has_ap_sync_key) {
        ESP_LOGE(TAG, "AP Sync Key not configured (call configure_tag first)");
        return ESP_ERR_INVALID_STATE;
    }

    eslp_ap_subev_cmd_t *slot = &s_ap.subev[subevent];
    uint8_t enc_len = 0;
    esp_err_t ret = eslp_ead_encrypt_sync_command(&s_ap.ap_sync_key, subevent,
                                                  tlv, tlv_len,
                                                  slot->data, sizeof(slot->data), &enc_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "EAD encrypt failed: %s", esp_err_to_name(ret));
        return ret;
    }

    slot->len = enc_len;
    slot->rsp_slot_start = rsp_slot_start;
    slot->rsp_slot_count = rsp_slot_count;
    slot->valid = true;
    return ESP_OK;
#endif
}

esp_err_t esp_ble_eslp_ap_queue_ping(uint8_t group_id, uint8_t esl_id)
{
    uint8_t tlv[2] = { BLE_ESL_CMD_PING, esl_id };
    /* Single TLV: N=1 -> slot 0 when individually addressed; broadcast listens to none. */
    uint8_t slot = 0;
    uint8_t count = (esl_id == BLE_ESL_BROADCAST_ADDRESS) ? 0 : 1;
    return esp_ble_eslp_ap_queue_pawr_command(group_id, tlv, sizeof(tlv), slot, count);
}

static void ap_on_conn_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS || !s_ap.inited) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
        if (event_data) {
            conn_handle = ((const uint16_t *)event_data)[0];
        }
        if (conn_handle != BLE_CONN_HANDLE_INVALID) {
            /* Start pairing / encryption before GATT config writes. */
            esp_err_t ret = esp_ble_conn_security_initiate(conn_handle);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "security_initiate failed: %s", esp_err_to_name(ret));
            }
            /* State change waits for encryption. */
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_ENC_CHANGE: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev || ev->enc_change.status != 0 || !ev->enc_change.encrypted) {
            break;
        }
        (void)eslp_ap_lifecycle_on_connected(ev->enc_change.conn_handle, NULL);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        uint16_t conn_handle = BLE_CONN_HANDLE_INVALID;
        if (event_data) {
            conn_handle = ((const uint16_t *)event_data)[0];
        }
        if (conn_handle != BLE_CONN_HANDLE_INVALID) {
            eslp_ap_cmd_on_disconnected(conn_handle);
            (void)eslp_ap_lifecycle_on_disconnected(conn_handle);
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        esp_ble_conn_data_t *conn_data = event_data;
        if (!conn_data || conn_data->type != BLE_CONN_UUID_TYPE_16 ||
                conn_data->uuid.uuid16 != BLE_ESL_CHR_UUID16_ECP) {
            break;
        }
        esp_ble_eslp_ap_ecp_response_t ev = {0};
        ev.conn_handle = conn_data->write_conn_id;
        ev.data_len = (uint8_t)MIN(conn_data->data_len, BLE_ESL_ECP_MAX_SIZE);
        if (ev.data_len && conn_data->data) {
            memcpy(ev.data, conn_data->data, ev.data_len);
        }
        eslp_ap_cmd_on_ecp_response(ev.conn_handle);
        ap_post(BLE_ESLP_AP_EVENT_ECP_RESPONSE, &ev, sizeof(ev));
        if (conn_data->data) {
            free(conn_data->data);
            conn_data->data = NULL;
        }
        break;
    }
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
    case ESP_BLE_CONN_EVENT_PER_SUBEV_DATA_REQ: {
        const esp_ble_conn_pawr_subev_data_req_t *req = event_data;
        if (!req || req->subevent_data_count == 0) {
            break;
        }

        uint8_t count = req->subevent_data_count;
        if (count > CONFIG_BLE_CONN_MGR_PAWR_MAX_SUBEV_SET) {
            count = CONFIG_BLE_CONN_MGR_PAWR_MAX_SUBEV_SET;
        }

        esp_ble_conn_pawr_subev_data_t items[CONFIG_BLE_CONN_MGR_PAWR_MAX_SUBEV_SET];
        uint8_t pending_clear[CONFIG_BLE_CONN_MGR_PAWR_MAX_SUBEV_SET];
        uint8_t pending_clear_n = 0;
        uint8_t filled = 0;
        for (uint8_t i = 0; i < count; i++) {
            uint8_t sub = (uint8_t)(req->subevent_start + i);
            items[filled].subevent = sub;
            if (sub < CONFIG_BLE_ESL_PROFILE_AP_MAX_SUBEVENTS && s_ap.subev[sub].valid) {
                eslp_ap_subev_cmd_t *slot = &s_ap.subev[sub];
                items[filled].data = slot->data;
                items[filled].data_len = slot->len;
                items[filled].response_slot_start = slot->rsp_slot_start;
                items[filled].response_slot_count = slot->rsp_slot_count;
                pending_clear[pending_clear_n++] = sub;
            } else {
                items[filled].data = NULL;
                items[filled].data_len = 0;
                items[filled].response_slot_start = 0;
                items[filled].response_slot_count = 0;
            }
            filled++;
        }
        if (filled > 0) {
            esp_err_t ret = esp_ble_conn_pawr_subev_data_set(filled, items);
            if (ret == ESP_OK) {
                for (uint8_t j = 0; j < pending_clear_n; j++) {
                    s_ap.subev[pending_clear[j]].valid = false;
                }
            } else {
                ESP_LOGW(TAG, "pawr_subev_data_set failed: %s (queue kept)", esp_err_to_name(ret));
            }
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_PER_SUBEV_RESP: {
        const esp_ble_conn_pawr_subev_resp_t *resp = event_data;
        if (!resp || resp->data_length == 0) {
            break;
        }
        esp_ble_eslp_ap_subev_resp_t ev = {0};
        ev.subevent = resp->subevent;
        ev.response_slot = resp->response_slot;
        ev.data_status = resp->data_status;

        esp_ble_esl_address_t addr = {0};
        uint8_t plain_len = 0;
        esp_err_t dret = eslp_ap_lifecycle_decrypt_pawr_response(
                             resp->subevent, resp->data, (uint8_t)resp->data_length,
                             ev.data, sizeof(ev.data), &plain_len, &addr);
        if (dret == ESP_OK) {
            ev.data_len = plain_len;
            ev.address = addr;
            ev.address_valid = true;
        } else {
            ESP_LOGW(TAG, "PAwR response EAD decrypt failed: %s", esp_err_to_name(dret));
            /* Keep raw encrypted bytes for diagnostics */
            ev.data_len = (uint8_t)MIN(resp->data_length, sizeof(ev.data));
            memcpy(ev.data, resp->data, ev.data_len);
            ev.address_valid = false;
        }
        ap_post(BLE_ESLP_AP_EVENT_SUBEV_RESP, &ev, sizeof(ev));
        break;
    }
#endif
    default:
        break;
    }
}

esp_err_t esp_ble_eslp_ap_init(void)
{
    if (s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_ap, 0, sizeof(s_ap));

    esp_err_t ret = eslp_ap_lifecycle_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = eslp_ap_cmd_init();
    if (ret != ESP_OK) {
        (void)eslp_ap_lifecycle_deinit();
        return ret;
    }

    ret = eslp_otp_ap_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "eslp_otp_ap_init failed: %s", esp_err_to_name(ret));
        (void)eslp_ap_cmd_deinit();
        (void)eslp_ap_lifecycle_deinit();
        return ret;
    }

    ret = esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, ap_on_conn_event, NULL);
    if (ret != ESP_OK) {
        (void)eslp_otp_ap_deinit();
        (void)eslp_ap_cmd_deinit();
        (void)eslp_ap_lifecycle_deinit();
        return ret;
    }

    s_ap.inited = true;
    ESP_LOGI(TAG, "ESL AP Profile initialized");
    return ESP_OK;
}

esp_err_t esp_ble_eslp_ap_deinit(void)
{
    if (!s_ap.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, ap_on_conn_event);
    (void)eslp_otp_ap_deinit();
    (void)eslp_ap_cmd_deinit();
    (void)eslp_ap_lifecycle_deinit();
    memset(&s_ap, 0, sizeof(s_ap));
    return ESP_OK;
}
