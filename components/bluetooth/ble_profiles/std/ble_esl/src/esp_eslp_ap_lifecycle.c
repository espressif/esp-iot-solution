/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief AP-side remote ESL lifecycle tracking (uses shared esp_eslp_state)
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_eslp_ap.h"
#include "esp_eslp_state.h"
#include "esp_eslp_ap_lifecycle_priv.h"
#include "esp_eslp_ead_priv.h"
#include "esp_ble_conn_mgr.h"

static const char *TAG = "ble_eslp_ap_life";

/* Tag drops the ACL itself with HCI 0x13 (Remote User Terminated Connection)
 * after a PAwR report or the 1.5 s PAST wait. NimBLE GAP events store that as
 * BLE_HS_ERR_HCI_BASE (0x200) + the HCI code. */
#define ESLP_HCI_REMOTE_USER_TERM    0x13
#define ESLP_NIMBLE_HCI_ERR_BASE     0x200

static bool eslp_disconnect_is_tag_teardown(uint16_t reason)
{
    if (reason == ESLP_HCI_REMOTE_USER_TERM) {
        return true;
    }
    if (reason >= ESLP_NIMBLE_HCI_ERR_BASE) {
        return (uint16_t)(reason - ESLP_NIMBLE_HCI_ERR_BASE) == ESLP_HCI_REMOTE_USER_TERM;
    }
    return false;
}

#ifndef CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS
#define CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS 8
#endif

typedef struct {
    bool used;
    bool has_resp_key;
    bool configured; /* Provisioning characteristic writes completed */
    bool past_pending; /* PAST set_info accepted; Synchronized only if the Tag drops the ACL */
    bool from_synchronized; /* ACL opened while profile was Synchronized (PAwR image update) */
    bool update_complete_in_flight; /* GATT write has started, result not known yet */
    bool update_complete_confirmed; /* GATT write returned ESP_OK; next Tag 0x13 may commit sync */
    bool update_complete_disc_pending; /* Tag 0x13 arrived before the write result */
    uint16_t update_complete_conn_handle;
    uint16_t update_complete_disc_reason;
    bool has_peer_addr;
    uint8_t peer_addr[6];
    uint8_t peer_addr_type;
    uint16_t conn_handle;
    esp_ble_esl_address_t address;
    esp_ble_esl_key_material_t resp_key;
    esp_ble_eslp_sm_t sm;
} eslp_ap_tag_t;

static bool s_inited;
static eslp_ap_tag_t s_tags[CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS];

static void ap_life_post(esp_ble_eslp_ap_event_t id, const void *data, size_t len)
{
    esp_err_t ret = esp_event_post(BLE_ESLP_AP_EVENTS, id, data, len, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "post event %d failed: %s", (int)id, esp_err_to_name(ret));
    }
}

static bool ap_addr_equal(const esp_ble_esl_address_t *a, const esp_ble_esl_address_t *b)
{
    return a && b && a->esl_id == b->esl_id && a->group_id_rfu == b->group_id_rfu;
}

static eslp_ap_tag_t *ap_life_find_by_addr(const esp_ble_esl_address_t *addr)
{
    if (!addr) {
        return NULL;
    }
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && ap_addr_equal(&s_tags[i].address, addr)) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static eslp_ap_tag_t *ap_life_find_by_peer(const uint8_t peer_addr[6], uint8_t peer_addr_type)
{
    if (!peer_addr) {
        return NULL;
    }
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].has_peer_addr &&
                s_tags[i].peer_addr_type == peer_addr_type &&
                memcmp(s_tags[i].peer_addr, peer_addr, 6) == 0) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static eslp_ap_tag_t *ap_life_find_by_conn(uint16_t conn_handle)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used && s_tags[i].conn_handle == conn_handle) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static eslp_ap_tag_t *ap_life_find_by_update_complete_conn(uint16_t conn_handle)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        eslp_ap_tag_t *tag = &s_tags[i];
        if (tag->used &&
                (tag->update_complete_in_flight || tag->update_complete_confirmed) &&
                tag->update_complete_conn_handle == conn_handle) {
            return tag;
        }
    }
    return NULL;
}

static eslp_ap_tag_t *ap_life_alloc(void)
{
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (!s_tags[i].used) {
            return &s_tags[i];
        }
    }
    return NULL;
}

static void ap_life_clear_update_complete(eslp_ap_tag_t *tag)
{
    if (!tag) {
        return;
    }
    tag->update_complete_in_flight = false;
    tag->update_complete_confirmed = false;
    tag->update_complete_disc_pending = false;
    tag->update_complete_conn_handle = BLE_CONN_HANDLE_INVALID;
    tag->update_complete_disc_reason = 0;
}

static void ap_life_on_changed(const esp_ble_eslp_state_changed_t *changed, void *ctx)
{
    eslp_ap_tag_t *tag = ctx;
    if (!tag || !changed) {
        return;
    }
    esp_ble_eslp_ap_tag_state_changed_t ev = {
        .address = tag->address,
        .conn_handle = tag->conn_handle,
        .prev_state = changed->prev_state,
        .new_state = changed->new_state,
    };
    ap_life_post(BLE_ESLP_AP_EVENT_TAG_STATE_CHANGED, &ev, sizeof(ev));
}

static void ap_life_on_timeout(esp_ble_eslp_timeout_kind_t kind, void *ctx)
{
    eslp_ap_tag_t *tag = ctx;
    if (!tag) {
        return;
    }

    esp_ble_eslp_ap_tag_timeout_t ev = {
        .address = tag->address,
        .conn_handle = tag->conn_handle,
        .kind = kind,
    };
    ap_life_post(BLE_ESLP_AP_EVENT_TAG_TIMEOUT, &ev, sizeof(ev));

    if (kind == BLE_ESLP_TIMEOUT_SYNC) {
        (void)esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNSYNCHRONIZED);
    } else if (kind == BLE_ESLP_TIMEOUT_UNSYNC) {
        tag->configured = false;
        tag->has_resp_key = false;
        (void)esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNASSOCIATED);
        tag->conn_handle = BLE_CONN_HANDLE_INVALID;
    }
}

esp_err_t eslp_ap_lifecycle_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(s_tags, 0, sizeof(s_tags));
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        s_tags[i].conn_handle = BLE_CONN_HANDLE_INVALID;
        s_tags[i].update_complete_conn_handle = BLE_CONN_HANDLE_INVALID;
    }
    s_inited = true;
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        if (s_tags[i].used) {
            (void)esp_ble_eslp_sm_deinit(&s_tags[i].sm);
            s_tags[i].used = false;
        }
    }
    s_inited = false;
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_forget(const esp_ble_esl_address_t *addr)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    eslp_ap_tag_t *tag = ap_life_find_by_addr(addr);
    if (tag == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    (void)esp_ble_eslp_sm_deinit(&tag->sm);
    memset(tag, 0, sizeof(*tag));
    tag->conn_handle = BLE_CONN_HANDLE_INVALID;
    tag->update_complete_conn_handle = BLE_CONN_HANDLE_INVALID;
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_on_configured(uint16_t conn_handle, const esp_ble_esl_address_t *addr,
                                          const esp_ble_esl_key_material_t *resp_key)
{
    if (!s_inited || !addr) {
        return ESP_ERR_INVALID_STATE;
    }

    eslp_ap_tag_t *tag = ap_life_find_by_addr(addr);
    if (!tag) {
        tag = ap_life_alloc();
        if (!tag) {
            ESP_LOGW(TAG, "tag table full");
            return ESP_ERR_NO_MEM;
        }
        memset(tag, 0, sizeof(*tag));
        tag->conn_handle = BLE_CONN_HANDLE_INVALID;
        tag->update_complete_conn_handle = BLE_CONN_HANDLE_INVALID;
        tag->address = *addr;
        esp_err_t ret = esp_ble_eslp_sm_init(&tag->sm, ap_life_on_changed, ap_life_on_timeout, tag);
        if (ret != ESP_OK) {
            return ret;
        }
        tag->used = true;
    }

    tag->conn_handle = conn_handle;
    if (resp_key) {
        tag->resp_key = *resp_key;
        tag->has_resp_key = true;
    }
    tag->configured = true;

    uint8_t peer[6] = {0};
    uint8_t peer_type = 0;
    if (esp_ble_conn_get_peer_addr_by_handle(conn_handle, peer, &peer_type) == ESP_OK) {
        memcpy(tag->peer_addr, peer, sizeof(tag->peer_addr));
        tag->peer_addr_type = peer_type;
        tag->has_peer_addr = true;
    }

    /* Stay in Configuring until PAST completes. */
    esp_ble_eslp_state_t cur = esp_ble_eslp_sm_get_state(&tag->sm);
    if (cur == BLE_ESLP_STATE_UNASSOCIATED) {
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_CONFIGURING);
    }
    if (cur == BLE_ESLP_STATE_CONFIGURING) {
        return ESP_OK;
    }
    /* Re-configure while Updating: keep Updating. */
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_decrypt_pawr_response(uint8_t group_id,
                                                  const uint8_t *enc_ad, uint8_t enc_ad_len,
                                                  uint8_t *out_tlvs, uint8_t out_tlvs_cap,
                                                  uint8_t *out_tlvs_len,
                                                  esp_ble_esl_address_t *out_addr)
{
    if (!s_inited || !enc_ad || !out_tlvs || !out_tlvs_len) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t gid = (uint8_t)(group_id & 0x7F);
    for (int i = 0; i < CONFIG_BLE_ESL_PROFILE_AP_MAX_TAGS; i++) {
        eslp_ap_tag_t *tag = &s_tags[i];
        if (!tag->used || !tag->has_resp_key) {
            continue;
        }
        if (BLE_ESL_ADDR_GROUP_ID(tag->address) != gid) {
            continue;
        }
        esp_err_t ret = eslp_ead_decrypt_sync_response(&tag->resp_key, enc_ad, enc_ad_len,
                                                       out_tlvs, out_tlvs_cap, out_tlvs_len);
        if (ret == ESP_OK) {
            if (out_addr) {
                *out_addr = tag->address;
            }
            /* Promote to Synchronized after a decryptable PAwR response. */
            esp_ble_eslp_state_t st = esp_ble_eslp_sm_get_state(&tag->sm);
            if (st == BLE_ESLP_STATE_CONFIGURING || st == BLE_ESLP_STATE_UPDATING) {
                (void)esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_SYNCHRONIZED);
            } else if (st == BLE_ESLP_STATE_UNSYNCHRONIZED) {
                if (esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UPDATING) == ESP_OK) {
                    (void)esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_SYNCHRONIZED);
                }
            }
            (void)esp_ble_eslp_sm_note_sync_activity(&tag->sm);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t eslp_ap_lifecycle_on_sync_transferred(uint16_t conn_handle)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    /* Stay in Configuring/Updating until the Tag drops this ACL. A supervision
     * timeout, 0x3e, or a local disconnect is not that teardown. */
    tag->past_pending = true;
    ESP_LOGI(TAG, "PAST set_info ok (conn=%u); Synchronized only if the Tag drops the ACL",
             (unsigned)conn_handle);
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_begin_update_complete(uint16_t conn_handle)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    tag->update_complete_in_flight = true;
    tag->update_complete_confirmed = false;
    tag->update_complete_disc_pending = false;
    tag->update_complete_conn_handle = conn_handle;
    tag->update_complete_disc_reason = 0;
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_confirm_update_complete(uint16_t conn_handle)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        tag = ap_life_find_by_update_complete_conn(conn_handle);
    }
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }

    tag->update_complete_in_flight = false;
    tag->update_complete_confirmed = true;

    if (tag->update_complete_disc_pending) {
        uint16_t reason = tag->update_complete_disc_reason;
        esp_ble_eslp_state_t cur = esp_ble_eslp_sm_get_state(&tag->sm);
        const bool in_acl_state =
            (cur == BLE_ESLP_STATE_CONFIGURING || cur == BLE_ESLP_STATE_UPDATING);

        ap_life_clear_update_complete(tag);
        tag->past_pending = false;
        tag->from_synchronized = false;
        if (in_acl_state) {
            ESP_LOGI(TAG, "Tag dropped ACL after confirmed Update Complete (reason=0x%03x); %s -> SYNCHRONIZED",
                     reason, esp_ble_eslp_state_str(cur));
            return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_SYNCHRONIZED);
        }
    }

    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_cancel_update_complete(uint16_t conn_handle)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        tag = ap_life_find_by_update_complete_conn(conn_handle);
    }
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ble_eslp_state_t cur = esp_ble_eslp_sm_get_state(&tag->sm);
    if (cur == BLE_ESLP_STATE_SYNCHRONIZED) {
        ap_life_clear_update_complete(tag);
        return ESP_OK;
    }

    bool deferred_disconnect = tag->update_complete_disc_pending;
    uint16_t reason = tag->update_complete_disc_reason;
    ap_life_clear_update_complete(tag);

    if (deferred_disconnect) {
        tag->past_pending = false;
        tag->from_synchronized = false;
        ESP_LOGW(TAG, "Update Complete write failed after Tag 0x13 (reason=0x%03x); %s -> UNSYNCHRONIZED",
                 reason, esp_ble_eslp_state_str(cur));
        if (tag->configured || esp_ble_eslp_state_is_associated(cur)) {
            return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNSYNCHRONIZED);
        }
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNASSOCIATED);
    }

    ESP_LOGW(TAG, "Update Complete write failed; cleared pending mark conn=%u",
             (unsigned)conn_handle);
    return ESP_OK;
}

bool eslp_ap_lifecycle_keeps_sync_without_past(uint16_t conn_handle)
{
    if (!s_inited) {
        return false;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    return tag && tag->from_synchronized;
}

esp_err_t eslp_ap_lifecycle_on_disconnected(uint16_t conn_handle, uint16_t reason)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        return ESP_OK;
    }
    tag->conn_handle = BLE_CONN_HANDLE_INVALID;
    esp_ble_eslp_state_t cur = esp_ble_eslp_sm_get_state(&tag->sm);
    if (cur == BLE_ESLP_STATE_SYNCHRONIZED) {
        tag->past_pending = false;
        ap_life_clear_update_complete(tag);
        tag->from_synchronized = false;
        return ESP_OK;
    }

    const bool in_acl_state =
        (cur == BLE_ESLP_STATE_CONFIGURING || cur == BLE_ESLP_STATE_UPDATING);
    const bool tag_teardown = eslp_disconnect_is_tag_teardown(reason);

    /* PAST path (Configuring / Unsynchronized Updating): Tag drops ACL after sync. */
    if (tag->past_pending && in_acl_state && tag_teardown) {
        tag->past_pending = false;
        ap_life_clear_update_complete(tag);
        tag->from_synchronized = false;
        ESP_LOGI(TAG, "Tag dropped ACL after PAST (reason=0x%03x); %s -> SYNCHRONIZED",
                 reason, esp_ble_eslp_state_str(cur));
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_SYNCHRONIZED);
    }

    /* Update Complete only commits after the GATT write has returned ESP_OK.
     * If the Tag 0x13 arrives first, defer the state decision until the write
     * result is known; an AP-side failed write is not proof the Tag accepted UC. */
    if (tag->update_complete_in_flight && in_acl_state && tag_teardown) {
        tag->update_complete_disc_pending = true;
        tag->update_complete_disc_reason = reason;
        ESP_LOGI(TAG, "Tag dropped ACL while Update Complete write is pending (reason=0x%03x); defer sync commit",
                 reason);
        return ESP_OK;
    }

    if (tag->update_complete_confirmed && in_acl_state && tag_teardown) {
        tag->past_pending = false;
        ap_life_clear_update_complete(tag);
        tag->from_synchronized = false;
        ESP_LOGI(TAG, "Tag dropped ACL after Update Complete (reason=0x%03x); %s -> SYNCHRONIZED",
                 reason, esp_ble_eslp_state_str(cur));
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_SYNCHRONIZED);
    }

    if (tag->past_pending && in_acl_state) {
        ESP_LOGW(TAG, "ACL lost after PAST reason=0x%03x; %s -> UNSYNCHRONIZED",
                 reason, esp_ble_eslp_state_str(cur));
    }
    tag->past_pending = false;
    ap_life_clear_update_complete(tag);
    tag->from_synchronized = false;
    if (tag->configured || esp_ble_eslp_state_is_associated(cur)) {
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNSYNCHRONIZED);
    }
    return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNASSOCIATED);
}

esp_err_t eslp_ap_lifecycle_on_connected(uint16_t conn_handle, const esp_ble_esl_address_t *addr)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = NULL;
    if (addr) {
        tag = ap_life_find_by_addr(addr);
    }
    if (!tag) {
        uint8_t peer[6] = {0};
        uint8_t peer_type = 0;
        if (esp_ble_conn_get_peer_addr_by_handle(conn_handle, peer, &peer_type) == ESP_OK) {
            tag = ap_life_find_by_peer(peer, peer_type);
        }
    }
    if (!tag) {
        tag = ap_life_find_by_conn(conn_handle);
    }
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    tag->conn_handle = conn_handle;
    tag->past_pending = false;
    ap_life_clear_update_complete(tag);
    esp_ble_eslp_state_t prev = esp_ble_eslp_sm_get_state(&tag->sm);
    tag->from_synchronized = (prev == BLE_ESLP_STATE_SYNCHRONIZED);
    if (esp_ble_eslp_state_is_associated(prev)) {
        return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UPDATING);
    }
    return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_CONFIGURING);
}

esp_err_t eslp_ap_lifecycle_note_sync_activity(uint16_t conn_handle)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    return esp_ble_eslp_sm_note_sync_activity(&tag->sm);
}

esp_err_t eslp_ap_lifecycle_get_conn(const esp_ble_esl_address_t *addr, uint16_t *out_conn)
{
    if (!addr || !out_conn) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_addr(addr);
    if (!tag || tag->conn_handle == BLE_CONN_HANDLE_INVALID) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_conn = tag->conn_handle;
    return ESP_OK;
}

esp_err_t eslp_ap_lifecycle_get_address(uint16_t conn_handle, esp_ble_esl_address_t *out_addr)
{
    if (!out_addr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_conn(conn_handle);
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_addr = tag->address;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_ap_get_tag_state(const esp_ble_esl_address_t *addr, esp_ble_eslp_state_t *out_state)
{
    if (!addr || !out_state) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    eslp_ap_tag_t *tag = ap_life_find_by_addr(addr);
    if (!tag) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_state = esp_ble_eslp_sm_get_state(&tag->sm);
    return ESP_OK;
}

esp_err_t esp_ble_eslp_ap_note_tag_sync_activity(uint16_t conn_handle)
{
    return eslp_ap_lifecycle_note_sync_activity(conn_handle);
}

esp_err_t eslp_ap_lifecycle_restore(const esp_ble_esl_address_t *addr,
                                    const uint8_t ble_addr[6], uint8_t ble_addr_type,
                                    const esp_ble_esl_key_material_t *resp_key)
{
    if (!s_inited || addr == NULL || ble_addr == NULL || resp_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    eslp_ap_tag_t *tag = ap_life_find_by_addr(addr);
    if (!tag) {
        tag = ap_life_alloc();
        if (!tag) {
            ESP_LOGW(TAG, "tag table full on restore");
            return ESP_ERR_NO_MEM;
        }
        memset(tag, 0, sizeof(*tag));
        tag->address = *addr;
        tag->conn_handle = BLE_CONN_HANDLE_INVALID;
        esp_err_t ret = esp_ble_eslp_sm_init(&tag->sm, ap_life_on_changed, ap_life_on_timeout, tag);
        if (ret != ESP_OK) {
            return ret;
        }
        tag->used = true;
    }

    tag->resp_key = *resp_key;
    tag->has_resp_key = true;
    tag->configured = true;
    memcpy(tag->peer_addr, ble_addr, 6);
    tag->peer_addr_type = ble_addr_type;
    tag->has_peer_addr = true;
    tag->conn_handle = BLE_CONN_HANDLE_INVALID;

    esp_err_t ret = ESP_OK;
    esp_ble_eslp_state_t cur = esp_ble_eslp_sm_get_state(&tag->sm);
    if (cur == BLE_ESLP_STATE_UNASSOCIATED) {
        ret = esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_CONFIGURING);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return esp_ble_eslp_sm_set_state(&tag->sm, BLE_ESLP_STATE_UNSYNCHRONIZED);
}
