/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Electronic Shelf Label Profile
 */

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_dis.h"
#include "esp_eslp_tag.h"
#include "esp_eslp_state.h"
#include "esp_eslp_ead_priv.h"
#include "esp_eslp_tag_cmd_priv.h"
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
#include "esp_eslp_otp_priv.h"
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define ESLP_ECP_TIMEOUT_US     ((uint64_t)BLE_ESL_ECP_PROCEDURE_TIMEOUT_S * 1000000ULL)

static const char *TAG = "ble_eslp";

ESP_EVENT_DEFINE_BASE(BLE_ESLP_EVENTS);

typedef struct {
    bool inited;
    bool associated;
    bool auto_respond_ecp;
    bool enable_past_receive;
    bool auto_pawr_response;
    bool address_written;
    bool ap_sync_key_written;
    bool resp_key_written;
    bool abs_time_written;
    bool factory_reset_pending; /* Reject further ECP until disconnect */
    bool update_complete_pending; /* Wait for PAST, then disconnect */
    bool ecp_proc_pending;      /* ECP procedure awaiting Notification */
    bool has_bonded_peer;       /* Peer BD_ADDR stored after successful encryption */
    uint8_t bonded_peer_addr[6];
    uint8_t bonded_peer_addr_type;
    uint16_t conn_handle;
    uint16_t sync_handle;
    uint16_t basic_state;
    esp_timer_handle_t ecp_proc_timer;
    esp_ble_eslp_sm_t sm;
    esp_ble_eslp_ecp_handler_t ecp_handler;
    void *ecp_handler_priv;
    esp_ble_eslp_service_needed_hold_cb_t svc_needed_hold_cb;
    void *svc_needed_hold_priv;
} eslp_ctx_t;

static eslp_ctx_t s_eslp;

static void eslp_post_event(esp_ble_eslp_event_t id, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_EVENTS, id, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post ESLP event %d: %s", (int)id, esp_err_to_name(ret));
    }
}

static void eslp_clear_association(void);

static bool eslp_config_complete(void)
{
    /* Config complete when Address, both Keys, and Absolute Time are written. */
    return s_eslp.address_written && s_eslp.ap_sync_key_written &&
           s_eslp.resp_key_written && s_eslp.abs_time_written;
}

static void eslp_service_reset_apply(void)
{
    bool keep = false;
    if (s_eslp.svc_needed_hold_cb) {
        keep = s_eslp.svc_needed_hold_cb(s_eslp.svc_needed_hold_priv);
    }
    if (keep) {
        s_eslp.basic_state |= BLE_ESL_BASIC_STATE_SERVICE_NEEDED;
        ESP_LOGI(TAG, "Service Reset: underlying condition still present");
    } else {
        s_eslp.basic_state &= (uint16_t)~BLE_ESL_BASIC_STATE_SERVICE_NEEDED;
    }
}

static void eslp_sm_on_changed(const esp_ble_eslp_state_changed_t *changed, void *ctx)
{
    (void)ctx;
    if (changed) {
        eslp_post_event(BLE_ESLP_EVENT_STATE_CHANGED, changed, sizeof(*changed));
    }
}

static void eslp_sm_on_timeout(esp_ble_eslp_timeout_kind_t kind, void *ctx)
{
    (void)ctx;
    if (kind == BLE_ESLP_TIMEOUT_SYNC) {
        s_eslp.sync_handle = 0;
        s_eslp.basic_state &= (uint16_t)~BLE_ESL_BASIC_STATE_SYNCHRONIZED;
        eslp_post_event(BLE_ESLP_EVENT_SYNC_LOST, NULL, 0);
        (void)esp_ble_eslp_sm_set_state(&s_eslp.sm, BLE_ESLP_STATE_UNSYNCHRONIZED);
        return;
    }
    if (kind == BLE_ESLP_TIMEOUT_UNSYNC) {
        eslp_clear_association();
        eslp_post_event(BLE_ESLP_EVENT_UNASSOCIATED, NULL, 0);
        (void)esp_ble_eslp_sm_set_state(&s_eslp.sm, BLE_ESLP_STATE_UNASSOCIATED);
    }
}

static esp_err_t eslp_set_state(esp_ble_eslp_state_t new_state)
{
    return esp_ble_eslp_sm_set_state(&s_eslp.sm, new_state);
}

static void eslp_clear_association(void)
{
    /* Drop stored commands with association. */
    eslp_tag_cmd_cancel_pending();

    esp_ble_esl_address_t addr = {0};
    esp_ble_esl_key_material_t key = {0};
    esp_ble_esl_abs_time_t abs_time = 0;

    (void)esp_ble_esl_set_address(&addr);
    (void)esp_ble_esl_set_ap_sync_key(&key);
    (void)esp_ble_esl_set_resp_key(&key);
    (void)esp_ble_esl_set_current_abs_time(&abs_time);

    if (s_eslp.has_bonded_peer) {
        (void)esp_ble_conn_delete_bond(s_eslp.bonded_peer_addr, s_eslp.bonded_peer_addr_type);
        s_eslp.has_bonded_peer = false;
        memset(s_eslp.bonded_peer_addr, 0, sizeof(s_eslp.bonded_peer_addr));
    }

    /* Allow bonding again when unassociated. */
    (void)esp_ble_conn_sm_set_bonding(true);
    (void)esp_ble_conn_set_pairing_allowed(true);

    s_eslp.associated = false;
    s_eslp.address_written = false;
    s_eslp.ap_sync_key_written = false;
    s_eslp.resp_key_written = false;
    s_eslp.abs_time_written = false;
    s_eslp.sync_handle = 0;
    s_eslp.basic_state &= (uint16_t)~BLE_ESL_BASIC_STATE_SYNCHRONIZED;
}

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
static void eslp_enable_past_receive(uint16_t conn_handle)
{
    if (!s_eslp.enable_past_receive || conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }
    esp_ble_conn_periodic_sync_params_t params = {
        .skip = 0,
        .sync_timeout = 4000,
        .reports_disabled = false,
    };
    esp_err_t ret = esp_ble_conn_periodic_sync_receive(conn_handle, &params);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAST receive enable failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "PAST receive enabled on conn=%u", (unsigned)conn_handle);
    }
}
#endif

static void eslp_ecp_proc_complete(void)
{
    if (s_eslp.ecp_proc_timer) {
        (void)esp_timer_stop(s_eslp.ecp_proc_timer);
    }
    s_eslp.ecp_proc_pending = false;
}

static void eslp_ecp_proc_timeout_cb(void *arg)
{
    (void)arg;
    if (!s_eslp.inited || !s_eslp.ecp_proc_pending) {
        return;
    }
    s_eslp.ecp_proc_pending = false;
    ESP_LOGW(TAG, "ECP procedure timeout (%d s)", BLE_ESL_ECP_PROCEDURE_TIMEOUT_S);
    /* Best-effort Error response on procedure timeout. */
    (void)esp_ble_esl_notify_ecp_response(
    (const uint8_t[]) {
        BLE_ESL_RESP_ERROR, BLE_ESL_ERR_UNSPECIFIED
    }, 2);
}

static void eslp_ecp_proc_arm(void)
{
    eslp_ecp_proc_complete();
    if (!s_eslp.ecp_proc_timer) {
        const esp_timer_create_args_t args = {
            .callback = eslp_ecp_proc_timeout_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "eslp_ecp",
        };
        if (esp_timer_create(&args, &s_eslp.ecp_proc_timer) != ESP_OK) {
            ESP_LOGW(TAG, "ECP procedure timer create failed");
            return;
        }
    }
    if (esp_timer_start_once(s_eslp.ecp_proc_timer, ESLP_ECP_TIMEOUT_US) == ESP_OK) {
        s_eslp.ecp_proc_pending = true;
    }
}

esp_err_t esp_ble_eslp_indicate_ecp_response(const uint8_t *data, uint16_t len)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ecp_proc_complete();
    return esp_ble_esl_notify_ecp_response(data, len);
}

esp_err_t esp_ble_eslp_indicate_error(uint8_t error_code)
{
    uint8_t tlv[2] = { BLE_ESL_RESP_ERROR, error_code };
    return esp_ble_eslp_indicate_ecp_response(tlv, sizeof(tlv));
}

esp_err_t esp_ble_eslp_indicate_basic_state(const uint16_t *basic_state)
{
    uint16_t value = basic_state ? *basic_state : s_eslp.basic_state;
    uint8_t tlv[3] = {
        BLE_ESL_RESP_BASIC_STATE,
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
    };
    return esp_ble_eslp_indicate_ecp_response(tlv, sizeof(tlv));
}

esp_err_t esp_ble_eslp_get_state(esp_ble_eslp_state_t *state)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    *state = esp_ble_eslp_sm_get_state(&s_eslp.sm);
    return ESP_OK;
}

esp_err_t esp_ble_eslp_is_associated(bool *associated)
{
    if (!associated) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    *associated = s_eslp.associated;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_get_basic_state(uint16_t *basic_state)
{
    if (!basic_state) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    *basic_state = s_eslp.basic_state;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_set_basic_state(uint16_t basic_state)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    s_eslp.basic_state = basic_state;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_set_service_needed(bool needed)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (needed) {
        s_eslp.basic_state |= BLE_ESL_BASIC_STATE_SERVICE_NEEDED;
    } else {
        s_eslp.basic_state &= (uint16_t)~BLE_ESL_BASIC_STATE_SERVICE_NEEDED;
    }
    return ESP_OK;
}

esp_err_t esp_ble_eslp_register_service_needed_hold(esp_ble_eslp_service_needed_hold_cb_t cb, void *priv)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    s_eslp.svc_needed_hold_cb = cb;
    s_eslp.svc_needed_hold_priv = priv;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_register_ecp_handler(esp_ble_eslp_ecp_handler_t handler, void *priv)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }
    s_eslp.ecp_handler = handler;
    s_eslp.ecp_handler_priv = priv;
    return ESP_OK;
}

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
static bool eslp_pawr_cmd_addressed_to_us(uint8_t esl_id)
{
    /* Broadcast Address is valid on PAwR */
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return true;
    }

    esp_ble_esl_address_t addr = {0};
    if (esp_ble_esl_get_address(&addr) != ESP_OK) {
        return false;
    }
    return addr.esl_id == esl_id;
}
#endif

/* ACL ECP rejects Broadcast Address */
static bool eslp_ecp_esl_id_valid(uint8_t esl_id)
{
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return false;
    }

    esp_ble_esl_address_t addr = {0};
    if (esp_ble_esl_get_address(&addr) != ESP_OK) {
        return false;
    }
    return addr.esl_id == esl_id;
}

static esp_err_t eslp_default_handle_ecp(const esp_ble_eslp_ecp_command_t *cmd)
{
    switch (cmd->opcode) {
    case BLE_ESL_CMD_PING:
        if (cmd->params_len != 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        return esp_ble_eslp_indicate_basic_state(NULL);

    case BLE_ESL_CMD_UNASSOCIATE:
        if (cmd->params_len != 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        /* Send response, then clear association. */
        {
            esp_err_t ret = esp_ble_eslp_indicate_basic_state(NULL);
            eslp_clear_association();
            eslp_post_event(BLE_ESLP_EVENT_UNASSOCIATED, NULL, 0);
            (void)eslp_set_state(BLE_ESLP_STATE_UNASSOCIATED);
            return ret;
        }

    case BLE_ESL_CMD_SERVICE_RESET:
        if (cmd->params_len != 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        /* Apply Service Reset; hold callback may keep Service Needed. */
        eslp_service_reset_apply();
        return esp_ble_eslp_indicate_basic_state(NULL);

    case BLE_ESL_CMD_FACTORY_RESET:
        if (cmd->params_len != 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        /* No ECP response; disconnect then unassociate */
        eslp_ecp_proc_complete();
        s_eslp.factory_reset_pending = true;
        eslp_tag_cmd_cancel_pending();
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
        eslp_otp_tag_clear_images();
#endif
        if (s_eslp.conn_handle != BLE_CONN_HANDLE_INVALID) {
            (void)esp_ble_conn_disconnect_by_handle(s_eslp.conn_handle);
        } else {
            (void)esp_ble_conn_disconnect();
        }
        return ESP_OK;

    case BLE_ESL_CMD_UPDATE_COMPLETE:
        if (cmd->params_len != 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!(s_eslp.address_written && s_eslp.ap_sync_key_written &&
                s_eslp.resp_key_written && s_eslp.abs_time_written)) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_STATE);
        }
        /* No ECP response. */
        eslp_ecp_proc_complete();
        s_eslp.associated = true;
        (void)esp_ble_conn_set_pairing_allowed(false);
        (void)esp_ble_conn_sm_set_bonding(false);
        eslp_post_event(BLE_ESLP_EVENT_ASSOCIATED, NULL, 0);

        /* Already synchronized: drop ACL immediately. */
        if ((s_eslp.basic_state & BLE_ESL_BASIC_STATE_SYNCHRONIZED) != 0) {
            s_eslp.update_complete_pending = false;
            (void)eslp_set_state(BLE_ESLP_STATE_SYNCHRONIZED);
            if (s_eslp.conn_handle != BLE_CONN_HANDLE_INVALID) {
                (void)esp_ble_conn_disconnect_by_handle(s_eslp.conn_handle);
            } else {
                (void)esp_ble_conn_disconnect();
            }
            return ESP_OK;
        }

        /* Not synchronized: wait for PAST, then disconnect. */
        s_eslp.update_complete_pending = true;
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
        eslp_enable_past_receive(s_eslp.conn_handle);
#else
        if (s_eslp.conn_handle != BLE_CONN_HANDLE_INVALID) {
            (void)esp_ble_conn_disconnect_by_handle(s_eslp.conn_handle);
        } else {
            (void)esp_ble_conn_disconnect();
        }
#endif
        return ESP_OK;

    default: {
        if (cmd->params_len < 1) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        if (!eslp_ecp_esl_id_valid(cmd->params[0])) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        }
        eslp_tag_cmd_result_t cmd_res = {0};
        esp_err_t hr = eslp_tag_cmd_handle(cmd, &cmd_res, false);
        if (hr == ESP_ERR_NOT_SUPPORTED) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_OPCODE);
        }
        if (hr != ESP_OK) {
            return esp_ble_eslp_indicate_error(BLE_ESL_ERR_UNSPECIFIED);
        }
        if (cmd_res.has_response) {
            return esp_ble_eslp_indicate_ecp_response(cmd_res.rsp, cmd_res.rsp_len);
        }
        /* Deferred response; keep timer armed */
        return ESP_OK;
    }
    }
}

static void eslp_handle_ecp_write(const uint8_t *data, size_t len)
{
    if (s_eslp.factory_reset_pending) {
        (void)esp_ble_eslp_indicate_error(BLE_ESL_ERR_UNSPECIFIED);
        return;
    }

    if (!data || len < 2) {
        (void)esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        return;
    }

    uint8_t opcode = data[0];
    uint8_t params_len = BLE_ESL_TLV_PARAMS_LEN(opcode);
    if ((size_t)(1 + params_len) != len) {
        (void)esp_ble_eslp_indicate_error(BLE_ESL_ERR_INVALID_PARAMS);
        return;
    }

    /* Factory Reset / Update Complete have no Notification response. */
    bool no_response = (opcode == BLE_ESL_CMD_FACTORY_RESET ||
                        opcode == BLE_ESL_CMD_UPDATE_COMPLETE);

    /*
     * One ECP procedure at a time. Reject a second command that expects a
     * response without clearing the active procedure timer.
     */
    if (!no_response && s_eslp.ecp_proc_pending) {
        ESP_LOGW(TAG, "ECP already pending; reject opcode=0x%02x", opcode);
        uint8_t err_tlv[2] = { BLE_ESL_RESP_ERROR, BLE_ESL_ERR_INVALID_STATE };
        (void)esp_ble_esl_notify_ecp_response(err_tlv, sizeof(err_tlv));
        return;
    }

    esp_ble_eslp_ecp_command_t cmd = {0};
    cmd.opcode = opcode;
    cmd.params_len = params_len;
    memcpy(cmd.params, &data[1], params_len);

    eslp_post_event(BLE_ESLP_EVENT_ECP_COMMAND, &cmd, sizeof(cmd));

    if (!no_response) {
        eslp_ecp_proc_arm();
    } else {
        eslp_ecp_proc_complete();
    }

    if (s_eslp.ecp_handler) {
        esp_err_t hr = s_eslp.ecp_handler(&cmd, s_eslp.ecp_handler_priv);
        if (hr == ESP_OK) {
            if (!no_response) {
                /* App claimed the command; notify elsewhere if needed */
            }
            return;
        }
        if (hr != ESP_ERR_NOT_FINISHED) {
            (void)esp_ble_eslp_indicate_error(BLE_ESL_ERR_UNSPECIFIED);
            return;
        }
    }

    if (!s_eslp.auto_respond_ecp) {
        return;
    }

    (void)eslp_default_handle_ecp(&cmd);
}

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
static void eslp_handle_pawr_report(const esp_ble_conn_periodic_report_t *report)
{
    if (!report || report->data_length == 0) {
        return;
    }

    /* PAwR commands are only handled in Synchronized. */
    if (esp_ble_eslp_sm_get_state(&s_eslp.sm) != BLE_ESLP_STATE_SYNCHRONIZED) {
        ESP_LOGD(TAG, "PAwR report ignored (not Synchronized)");
        return;
    }

    esp_ble_esl_key_material_t ap_sync_key = {0};
    if (esp_ble_esl_get_ap_sync_key(&ap_sync_key) != ESP_OK) {
        ESP_LOGW(TAG, "PAwR report: AP Sync Key unavailable");
        return;
    }

    esp_ble_eslp_pawr_command_t ev = {0};
    uint8_t payload_len = 0;
    esp_err_t ret = eslp_ead_decrypt_sync_command(&ap_sync_key,
                                                  report->data, (uint8_t)report->data_length,
                                                  ev.data, sizeof(ev.data), &payload_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAwR report: EAD decrypt failed: %s", esp_err_to_name(ret));
        return;
    }

    /* ESL Payload: Group_ID | RFU | TLV... */
    if (payload_len < 2) {
        return;
    }
    if ((ev.data[0] & 0x80) != 0) {
        return;
    }

    esp_ble_esl_address_t addr = {0};
    if (esp_ble_esl_get_address(&addr) == ESP_OK) {
        uint8_t our_gid = (uint8_t)BLE_ESL_ADDR_GROUP_ID(addr);
        if ((ev.data[0] & 0x7F) != our_gid) {
            return;
        }
    }

    ev.sync_handle = report->sync_handle;
    ev.event_counter = report->event_counter;
    ev.subevent = report->subevent;
    ev.data_len = payload_len;
    eslp_post_event(BLE_ESLP_EVENT_PAWR_COMMAND, &ev, sizeof(ev));

    (void)esp_ble_eslp_sm_note_sync_activity(&s_eslp.sm);

    if (!s_eslp.auto_pawr_response) {
        return;
    }

    /* Aggregate responses; use response slot of the last addressed TLV */
    const uint8_t *tlvs = &ev.data[1];
    uint8_t tlvs_len = (uint8_t)(payload_len - 1);
    uint8_t offset = 0;
    uint8_t tlv_index = 0; /* 1-based among all TLVs in payload */
    uint8_t rsp_buf[BLE_ESLP_PAYLOAD_MAX_SIZE];
    uint8_t rsp_len = 0;
    uint8_t last_relevant_n = 0;

    while (offset < tlvs_len) {
        uint8_t opcode = tlvs[offset];
        uint8_t total = (uint8_t)BLE_ESL_TLV_TOTAL_LEN(opcode);
        if (total < 2 || (uint16_t)(offset + total) > tlvs_len) {
            break;
        }
        tlv_index++;

        uint8_t params_len = BLE_ESL_TLV_PARAMS_LEN(opcode);
        uint8_t esl_id = tlvs[offset + 1];
        if (!eslp_pawr_cmd_addressed_to_us(esl_id)) {
            offset = (uint8_t)(offset + total);
            continue;
        }

        /* Broadcast: apply effects, no response. */
        if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
            if (opcode != BLE_ESL_CMD_FACTORY_RESET &&
                    opcode != BLE_ESL_CMD_UPDATE_COMPLETE &&
                    opcode != BLE_ESL_CMD_PING) {
                esp_ble_eslp_ecp_command_t cmd = {0};
                cmd.opcode = opcode;
                cmd.params_len = params_len;
                memcpy(cmd.params, &tlvs[offset + 1], params_len);
                eslp_tag_cmd_result_t cmd_res = {0};
                (void)eslp_tag_cmd_handle(&cmd, &cmd_res, true);
            }
            offset = (uint8_t)(offset + total);
            continue;
        }

        uint8_t one[BLE_ESL_ECP_MAX_SIZE];
        uint8_t one_len = 0;
        bool have = false;

        if (opcode == BLE_ESL_CMD_FACTORY_RESET ||
                opcode == BLE_ESL_CMD_UPDATE_COMPLETE) {
            /* Invalid on PAwR; return Error for unicast. */
            one[0] = BLE_ESL_RESP_ERROR;
            one[1] = BLE_ESL_ERR_INVALID_STATE;
            one_len = 2;
            have = true;
        } else if (opcode == BLE_ESL_CMD_PING) {
            one[0] = BLE_ESL_RESP_BASIC_STATE;
            one[1] = (uint8_t)(s_eslp.basic_state & 0xFF);
            one[2] = (uint8_t)((s_eslp.basic_state >> 8) & 0xFF);
            one_len = 3;
            have = true;
        } else if (opcode == BLE_ESL_CMD_UNASSOCIATE) {
            if (params_len != 1) {
                one[0] = BLE_ESL_RESP_ERROR;
                one[1] = BLE_ESL_ERR_INVALID_PARAMS;
                one_len = 2;
            } else {
                one[0] = BLE_ESL_RESP_BASIC_STATE;
                one[1] = (uint8_t)(s_eslp.basic_state & 0xFF);
                one[2] = (uint8_t)((s_eslp.basic_state >> 8) & 0xFF);
                one_len = 3;
                eslp_clear_association();
                eslp_post_event(BLE_ESLP_EVENT_UNASSOCIATED, NULL, 0);
                (void)eslp_set_state(BLE_ESLP_STATE_UNASSOCIATED);
            }
            have = true;
        } else if (opcode == BLE_ESL_CMD_SERVICE_RESET) {
            if (params_len != 1) {
                one[0] = BLE_ESL_RESP_ERROR;
                one[1] = BLE_ESL_ERR_INVALID_PARAMS;
                one_len = 2;
            } else {
                eslp_service_reset_apply();
                one[0] = BLE_ESL_RESP_BASIC_STATE;
                one[1] = (uint8_t)(s_eslp.basic_state & 0xFF);
                one[2] = (uint8_t)((s_eslp.basic_state >> 8) & 0xFF);
                one_len = 3;
            }
            have = true;
        } else {
            esp_ble_eslp_ecp_command_t cmd = {0};
            cmd.opcode = opcode;
            cmd.params_len = params_len;
            memcpy(cmd.params, &tlvs[offset + 1], params_len);
            eslp_tag_cmd_result_t cmd_res = {0};
            esp_err_t hr = eslp_tag_cmd_handle(&cmd, &cmd_res, true);
            if (hr == ESP_OK && cmd_res.has_response) {
                memcpy(one, cmd_res.rsp, cmd_res.rsp_len);
                one_len = cmd_res.rsp_len;
                have = true;
            } else if (hr == ESP_ERR_NOT_SUPPORTED) {
                one[0] = BLE_ESL_RESP_ERROR;
                one[1] = BLE_ESL_ERR_INVALID_OPCODE;
                one_len = 2;
                have = true;
            } else {
                one[0] = BLE_ESL_RESP_ERROR;
                one[1] = BLE_ESL_ERR_UNSPECIFIED;
                one_len = 2;
                have = true;
            }
        }

        if (have) {
            /* Keep responses that fit; replace overflow with Capacity Limit Error. */
            const uint8_t *to_copy = one;
            uint8_t copy_len = one_len;
            if ((uint16_t)rsp_len + one_len > BLE_ESLP_PAYLOAD_MAX_SIZE) {
                static const uint8_t cl_err[2] = {
                    BLE_ESL_RESP_ERROR, BLE_ESL_ERR_CAPACITY_LIMIT
                };
                if ((uint16_t)rsp_len + sizeof(cl_err) > BLE_ESLP_PAYLOAD_MAX_SIZE) {
                    break;
                }
                to_copy = cl_err;
                copy_len = (uint8_t)sizeof(cl_err);
            }
            memcpy(&rsp_buf[rsp_len], to_copy, copy_len);
            rsp_len = (uint8_t)(rsp_len + copy_len);
            last_relevant_n = tlv_index;
        }

        offset = (uint8_t)(offset + total);
    }

    if (rsp_len == 0 || last_relevant_n == 0) {
        return;
    }

    esp_ble_esl_key_material_t resp_key = {0};
    if (esp_ble_esl_get_resp_key(&resp_key) != ESP_OK) {
        ESP_LOGW(TAG, "PAwR response: Response Key unavailable");
        return;
    }

    uint8_t enc_ad[BLE_ESLP_EAD_OUTER_MAX_SIZE];
    uint8_t enc_len = 0;
    ret = eslp_ead_encrypt_sync_response(&resp_key, rsp_buf, rsp_len,
                                         enc_ad, sizeof(enc_ad), &enc_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAwR response EAD encrypt failed: %s", esp_err_to_name(ret));
        return;
    }

    esp_ble_conn_pawr_response_params_t rsp = {
        .request_event = report->event_counter,
        .request_subevent = report->subevent,
        .response_subevent = report->subevent,
        .response_slot = (uint8_t)(last_relevant_n - 1),
    };
    ret = esp_ble_conn_pawr_response_data_set(report->sync_handle, &rsp, enc_ad, enc_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAwR response set failed: %s", esp_err_to_name(ret));
    }
}
#endif

static void eslp_on_esl_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_ESL_EVENTS || !s_eslp.inited) {
        return;
    }

    switch (id) {
    case BLE_ESL_CHR_UUID16_ADDRESS:
        s_eslp.address_written = true;
        eslp_post_event(BLE_ESLP_EVENT_ADDRESS_WRITTEN, event_data, sizeof(esp_ble_esl_address_t));
        break;
    case BLE_ESL_CHR_UUID16_AP_SYNC_KEY:
        s_eslp.ap_sync_key_written = true;
        eslp_post_event(BLE_ESLP_EVENT_AP_SYNC_KEY_WRITTEN, event_data, sizeof(esp_ble_esl_key_material_t));
        break;
    case BLE_ESL_CHR_UUID16_RESP_KEY:
        s_eslp.resp_key_written = true;
        eslp_post_event(BLE_ESLP_EVENT_RESP_KEY_WRITTEN, event_data, sizeof(esp_ble_esl_key_material_t));
        break;
    case BLE_ESL_CHR_UUID16_CURRENT_ABS_TIME:
        s_eslp.abs_time_written = true;
        eslp_post_event(BLE_ESLP_EVENT_ABS_TIME_WRITTEN, event_data, sizeof(esp_ble_esl_abs_time_t));
        break;
    case BLE_ESL_CHR_UUID16_ECP: {
        const uint8_t *buf = event_data;
        if (!buf) {
            break;
        }
        uint8_t total = BLE_ESL_TLV_TOTAL_LEN(buf[0]);
        eslp_handle_ecp_write(buf, total);
        break;
    }
    default:
        break;
    }
}

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
static esp_err_t eslp_select_pawr_subev(uint16_t sync_handle, uint8_t num_subevents)
{
    if (num_subevents == 0) {
        return ESP_OK;
    }

    esp_ble_esl_address_t addr = {0};
    if (esp_ble_esl_get_address(&addr) != ESP_OK) {
        ESP_LOGW(TAG, "PAST: no ESL address for PAwR subevent");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t sub = (uint8_t)BLE_ESL_ADDR_GROUP_ID(addr);
    if (sub >= num_subevents) {
        ESP_LOGW(TAG, "PAST: Group_ID %u >= num_subevents %u", sub, num_subevents);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_ble_conn_pawr_sync_subev(sync_handle, 0, 1, &sub);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "PAST: pawr sync subev failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
#endif

static void eslp_on_conn_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS || !s_eslp.inited) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        if (event_data) {
            s_eslp.conn_handle = ((const uint16_t *)event_data)[0];
        } else {
            uint16_t h = BLE_CONN_HANDLE_INVALID;
            if (esp_ble_conn_get_conn_handle(&h) == ESP_OK) {
                s_eslp.conn_handle = h;
            }
        }
        /* Wait for encryption before Configuring / Updating */
        if (s_eslp.associated) {
            /* Reject pairing while associated */
            (void)esp_ble_conn_set_pairing_allowed(false);
            (void)esp_ble_conn_sm_set_bonding(false);
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_ENC_CHANGE: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        if (ev->enc_change.status != 0 || !ev->enc_change.encrypted) {
            ESP_LOGW(TAG, "Encryption failed status=%d encrypted=%d",
                     ev->enc_change.status, (int)ev->enc_change.encrypted);
            break;
        }

        if (s_eslp.associated) {
            /* Enter Updating only for the bonded peer. */
            if (s_eslp.has_bonded_peer &&
                    (memcmp(s_eslp.bonded_peer_addr, ev->enc_change.peer_addr, 6) != 0 ||
                     s_eslp.bonded_peer_addr_type != ev->enc_change.peer_addr_type)) {
                ESP_LOGW(TAG, "Untrusted peer encrypted — stay out of Updating");
                (void)esp_ble_conn_set_pairing_allowed(false);
                (void)esp_ble_conn_disconnect_by_handle(ev->enc_change.conn_handle);
                break;
            }
            memcpy(s_eslp.bonded_peer_addr, ev->enc_change.peer_addr, sizeof(s_eslp.bonded_peer_addr));
            s_eslp.bonded_peer_addr_type = ev->enc_change.peer_addr_type;
            s_eslp.has_bonded_peer = true;
            (void)esp_ble_conn_set_pairing_allowed(false);
            (void)esp_ble_conn_sm_set_bonding(false);
            (void)eslp_set_state(BLE_ESLP_STATE_UPDATING);
        } else {
            memcpy(s_eslp.bonded_peer_addr, ev->enc_change.peer_addr, sizeof(s_eslp.bonded_peer_addr));
            s_eslp.bonded_peer_addr_type = ev->enc_change.peer_addr_type;
            s_eslp.has_bonded_peer = true;
            (void)eslp_set_state(BLE_ESLP_STATE_CONFIGURING);
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_PASSKEY_ACTION: {
        const esp_ble_conn_event_data_t *ev = event_data;
        if (!ev) {
            break;
        }
        esp_ble_eslp_state_t st = esp_ble_eslp_sm_get_state(&s_eslp.sm);
        /* Reject pairing while associated or Updating. */
        if (st == BLE_ESLP_STATE_UPDATING || s_eslp.associated) {
            ESP_LOGW(TAG, "Reject pairing (state=%d associated=%d)", (int)st, (int)s_eslp.associated);
            if (ev->passkey_action.action == ESP_BLE_CONN_SM_ACT_NUMCMP) {
                (void)esp_ble_conn_numcmp_reply(ev->passkey_action.conn_handle, false);
            }
            (void)esp_ble_conn_disconnect_by_handle(ev->passkey_action.conn_handle);
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        s_eslp.conn_handle = BLE_CONN_HANDLE_INVALID;
        eslp_ecp_proc_complete();
        if (s_eslp.factory_reset_pending) {
            s_eslp.factory_reset_pending = false;
            s_eslp.update_complete_pending = false;
            eslp_clear_association();
            eslp_post_event(BLE_ESLP_EVENT_UNASSOCIATED, NULL, 0);
            (void)eslp_set_state(BLE_ESLP_STATE_UNASSOCIATED);
            break;
        }
        s_eslp.update_complete_pending = false;
        /* Keep Synchronized if PAST already completed */
        if (esp_ble_eslp_sm_get_state(&s_eslp.sm) == BLE_ESLP_STATE_SYNCHRONIZED) {
            break;
        }
        if (s_eslp.associated) {
            ESP_LOGI(TAG, "Link loss while associated → Unsynchronized");
            (void)eslp_set_state(BLE_ESLP_STATE_UNSYNCHRONIZED);
        } else if (eslp_config_complete()) {
            /* Config complete before Update Complete: keep materials. */
            ESP_LOGI(TAG, "Link loss after config complete (pre-UC) → Unsynchronized");
            s_eslp.associated = true;
            (void)esp_ble_conn_set_pairing_allowed(false);
            (void)esp_ble_conn_sm_set_bonding(false);
            eslp_post_event(BLE_ESLP_EVENT_ASSOCIATED, NULL, 0);
            (void)eslp_set_state(BLE_ESLP_STATE_UNSYNCHRONIZED);
        } else {
            ESP_LOGI(TAG, "Link loss with incomplete config → Unassociated");
            eslp_clear_association();
            (void)eslp_set_state(BLE_ESLP_STATE_UNASSOCIATED);
        }
        break;
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
    case ESP_BLE_CONN_EVENT_PERIODIC_TRANSFER: {
        const esp_ble_conn_periodic_transfer_t *tr = event_data;
        if (!tr || tr->status != 0) {
            ESP_LOGW(TAG, "PAST failed status=%u", tr ? (unsigned)tr->status : 0);
            break;
        }

        /* Synchronized only from Configuring or Updating after PAST succeeds. */
        esp_ble_eslp_state_t st = esp_ble_eslp_sm_get_state(&s_eslp.sm);
        if (st == BLE_ESLP_STATE_SYNCHRONIZED) {
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
            if (eslp_select_pawr_subev(tr->sync_handle, tr->num_subevents) != ESP_OK) {
                break;
            }
#endif
            s_eslp.sync_handle = tr->sync_handle;
            break;
        }
        if (st != BLE_ESLP_STATE_CONFIGURING && st != BLE_ESLP_STATE_UPDATING) {
            ESP_LOGW(TAG, "PAST ignored in state %s", esp_ble_eslp_state_str(st));
            break;
        }

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
        if (eslp_select_pawr_subev(tr->sync_handle, tr->num_subevents) != ESP_OK) {
            break;
        }
#endif
        s_eslp.sync_handle = tr->sync_handle;
        if (eslp_set_state(BLE_ESLP_STATE_SYNCHRONIZED) != ESP_OK) {
            ESP_LOGW(TAG, "PAST: Synchronized transition rejected");
            s_eslp.sync_handle = 0;
            break;
        }

        s_eslp.basic_state |= BLE_ESL_BASIC_STATE_SYNCHRONIZED;
        eslp_post_event(BLE_ESLP_EVENT_SYNCHRONIZED, NULL, 0);

        /* Update Complete already received: drop the ACL link. */
        if (s_eslp.update_complete_pending) {
            s_eslp.update_complete_pending = false;
            if (s_eslp.conn_handle != BLE_CONN_HANDLE_INVALID) {
                (void)esp_ble_conn_disconnect_by_handle(s_eslp.conn_handle);
            } else {
                (void)esp_ble_conn_disconnect();
            }
        }
        break;
    }
#endif
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC) || defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_TRANSFER)
    case ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST:
        s_eslp.sync_handle = 0;
        s_eslp.basic_state &= (uint16_t)~BLE_ESL_BASIC_STATE_SYNCHRONIZED;
        eslp_post_event(BLE_ESLP_EVENT_SYNC_LOST, event_data, sizeof(esp_ble_conn_periodic_sync_lost_t));
        if (s_eslp.associated) {
            (void)eslp_set_state(BLE_ESLP_STATE_UNSYNCHRONIZED);
        } else {
            (void)eslp_set_state(BLE_ESLP_STATE_UNASSOCIATED);
        }
        break;
#endif
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV_WITH_RESP)
    case ESP_BLE_CONN_EVENT_PERIODIC_REPORT:
        eslp_handle_pawr_report(event_data);
        break;
#endif
    default:
        break;
    }
}

esp_err_t esp_ble_eslp_init(const esp_ble_eslp_config_t *config)
{
    if (s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_eslp, 0, sizeof(s_eslp));
    s_eslp.auto_respond_ecp = true;
    s_eslp.enable_past_receive = true;
    s_eslp.auto_pawr_response = true;
    s_eslp.conn_handle = BLE_CONN_HANDLE_INVALID;
    if (config) {
        s_eslp.basic_state = config->basic_state;
        s_eslp.auto_respond_ecp = !config->skip_auto_ecp_response;
        s_eslp.enable_past_receive = !config->skip_past_receive;
        s_eslp.auto_pawr_response = !config->skip_auto_pawr_response;
    }

    esp_err_t ret = esp_ble_eslp_sm_init(&s_eslp.sm, eslp_sm_on_changed, eslp_sm_on_timeout, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_eslp_sm_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_esl_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_esl_init failed: %s", esp_err_to_name(ret));
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }

    ret = esp_ble_dis_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_dis_init failed: %s", esp_err_to_name(ret));
        (void)esp_ble_esl_deinit();
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }

#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
    ret = eslp_otp_tag_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "eslp_otp_tag_init failed: %s", esp_err_to_name(ret));
        (void)esp_ble_dis_deinit();
        (void)esp_ble_esl_deinit();
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }
#endif

    ret = eslp_tag_cmd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "eslp_tag_cmd_init failed: %s", esp_err_to_name(ret));
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
        (void)eslp_otp_tag_deinit();
#endif
        (void)esp_ble_dis_deinit();
        (void)esp_ble_esl_deinit();
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }
    eslp_tag_cmd_sync_basic_state(&s_eslp.basic_state);

    ret = esp_event_handler_register(BLE_ESL_EVENTS, ESP_EVENT_ANY_ID, eslp_on_esl_event, NULL);
    if (ret != ESP_OK) {
        (void)eslp_tag_cmd_deinit();
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
        (void)eslp_otp_tag_deinit();
#endif
        (void)esp_ble_dis_deinit();
        (void)esp_ble_esl_deinit();
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }

    ret = esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, eslp_on_conn_event, NULL);
    if (ret != ESP_OK) {
        (void)esp_event_handler_unregister(BLE_ESL_EVENTS, ESP_EVENT_ANY_ID, eslp_on_esl_event);
        (void)eslp_tag_cmd_deinit();
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
        (void)eslp_otp_tag_deinit();
#endif
        (void)esp_ble_dis_deinit();
        (void)esp_ble_esl_deinit();
        (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
        return ret;
    }

    s_eslp.inited = true;
    /* Bondable until associated. */
    (void)esp_ble_conn_sm_set_bonding(true);
    (void)esp_ble_conn_set_pairing_allowed(true);
    ESP_LOGI(TAG, "ESL Profile initialized");
    return ESP_OK;
}

esp_err_t esp_ble_eslp_deinit(void)
{
    if (!s_eslp.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    eslp_ecp_proc_complete();
    if (s_eslp.ecp_proc_timer) {
        (void)esp_timer_delete(s_eslp.ecp_proc_timer);
        s_eslp.ecp_proc_timer = NULL;
    }

    (void)esp_event_handler_unregister(BLE_ESL_EVENTS, ESP_EVENT_ANY_ID, eslp_on_esl_event);
    (void)esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, eslp_on_conn_event);
    (void)eslp_tag_cmd_deinit();
#ifdef CONFIG_BLE_ESL_OTS_SUPPORT
    (void)eslp_otp_tag_deinit();
#endif
    (void)esp_ble_dis_deinit();
    (void)esp_ble_esl_deinit();
    (void)esp_ble_eslp_sm_deinit(&s_eslp.sm);
    memset(&s_eslp, 0, sizeof(s_eslp));
    return ESP_OK;
}

#ifndef CONFIG_BLE_ESL_OTS_SUPPORT
esp_err_t esp_ble_eslp_get_image(uint8_t image_index, const uint8_t **data, uint32_t *len)
{
    (void)image_index;
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
