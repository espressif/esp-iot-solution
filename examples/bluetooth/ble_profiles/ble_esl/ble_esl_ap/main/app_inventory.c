/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_inventory.h"

#include <string.h>
#include <stdio.h>
#include "esl_nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ble_esl_ap.h"
#include "sdkconfig.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"

#ifndef CONFIG_BT_NIMBLE_NVS_PERSIST
#error "ESL AP reboot recovery requires CONFIG_BT_NIMBLE_NVS_PERSIST"
#endif

#if CONFIG_BT_NIMBLE_MAX_BONDS < ESL_PRODUCT_MAX_TAGS
#error "CONFIG_BT_NIMBLE_MAX_BONDS must cover every persistent ESL inventory slot"
#endif

static const char *TAG = "app_inventory";

static esl_product_inventory_t s_inventory;
static esl_product_tag_record_t s_selected_view;
static esl_product_tag_record_t s_find_view;

static esl_ap_orphan_bond_t s_orphan_bonds[ESL_AP_MAX_ORPHAN_BONDS];
static uint8_t s_orphan_count;

#define INVENTORY_CONN_INVALID  ((uint16_t)0xFFFF)

static bool addr_is_zero(const uint8_t addr[ESL_PRODUCT_ADDR_LEN])
{
    static const uint8_t zero[ESL_PRODUCT_ADDR_LEN] = { 0 };

    return memcmp(addr, zero, ESL_PRODUCT_ADDR_LEN) == 0;
}

static bool keys_are_zero(const uint8_t *key, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (key[i] != 0) {
            return false;
        }
    }
    return true;
}

static bool addr_equal(const uint8_t a[ESL_PRODUCT_ADDR_LEN], const uint8_t b[ESL_PRODUCT_ADDR_LEN])
{
    return memcmp(a, b, ESL_PRODUCT_ADDR_LEN) == 0;
}

static void refresh_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (s_inventory.blob.tags[i].valid == 1) {
            count++;
        }
    }
    s_inventory.blob.header.count = count;
    s_inventory.count = count;
}

static void clear_runtime_all(void)
{
    memset(s_inventory.runtime, 0, sizeof(s_inventory.runtime));
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        s_inventory.runtime[i].conn_handle = INVENTORY_CONN_INVALID;
        s_inventory.runtime[i].esl_runtime_state = BLE_ESL_STATE_UNASSOCIATED;
        s_inventory.runtime[i].product_state = ESL_PRODUCT_STATE_INVALID;
        s_inventory.runtime[i].failure_reason = ESL_AP_FAIL_NONE;
    }
}

static esl_product_tag_record_t *make_view(uint8_t slot, esl_product_tag_record_t *storage)
{
    if (slot >= ESL_PRODUCT_MAX_TAGS || storage == NULL) {
        return NULL;
    }
    storage->wire = &s_inventory.blob.tags[slot];
    storage->rt = &s_inventory.runtime[slot];
    storage->slot = slot;
    return storage;
}

static int find_slot_by_addr(const uint8_t addr[6])
{
    if (addr == NULL) {
        return -1;
    }
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        if (w->valid == 1 && addr_equal(w->addr, addr)) {
            return (int)i;
        }
    }
    return -1;
}

static int find_slot_by_esl(uint8_t esl_id, uint8_t group_id)
{
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        if (w->valid == 1 && w->esl_id == esl_id && w->group_id == group_id) {
            return (int)i;
        }
    }
    return -1;
}

static int find_slot_by_esl_addr(uint16_t esl_addr)
{
    uint8_t esl_id = (uint8_t)(esl_addr & 0xFF);
    uint8_t group_id = (uint8_t)((esl_addr >> 8) & 0x7F);
    return find_slot_by_esl(esl_id, group_id);
}

static int find_free_slot(void)
{
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (s_inventory.blob.tags[i].valid == 0) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t next_record_generation(uint8_t slot)
{
    uint32_t prev = esl_product_rd_le32(s_inventory.blob.tags[slot].record_generation_le);
    uint32_t next = prev + 1;
    if (next == 0) {
        next = 1;
    }
    return next;
}

static void format_addr_diag(const uint8_t addr[ESL_PRODUCT_ADDR_LEN], char *out, size_t out_len)
{
    snprintf(out, out_len, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static const char *phase_str(uint8_t phase)
{
    switch (phase) {
    case ESL_PRODUCT_PHASE_PREPARED: return "prepared";
    case ESL_PRODUCT_PHASE_BONDED: return "bonded";
    case ESL_PRODUCT_PHASE_CONFIGURED: return "configured";
    case ESL_PRODUCT_PHASE_ACTIVE: return "active";
    default: return "invalid";
    }
}

static const char *product_state_str(esl_product_state_t st)
{
    switch (st) {
    case ESL_PRODUCT_STATE_RECOVERING: return "recovering";
    case ESL_PRODUCT_STATE_ONLINE: return "online";
    case ESL_PRODUCT_STATE_RETRY_WAIT: return "retry_wait";
    case ESL_PRODUCT_STATE_OFFLINE: return "offline";
    case ESL_PRODUCT_STATE_QUARANTINED: return "quarantined";
    default: return "invalid";
    }
}

static const char *esl_rt_str(ble_esl_state_t state)
{
    switch (state) {
    case BLE_ESL_STATE_UNASSOCIATED: return "unassociated";
    case BLE_ESL_STATE_CONFIGURING: return "configuring";
    case BLE_ESL_STATE_SYNCHRONIZED: return "synchronized";
    case BLE_ESL_STATE_UPDATING: return "updating";
    case BLE_ESL_STATE_UNSYNCHRONIZED: return "unsynchronized";
    default: return "unknown";
    }
}

bool app_inventory_tag_view(uint8_t slot, esl_product_tag_record_t *out)
{
    if (out == NULL || slot >= ESL_PRODUCT_MAX_TAGS) {
        return false;
    }
    make_view(slot, out);
    return s_inventory.blob.tags[slot].valid == 1;
}

bool app_inventory_runtime_generation_valid(uint8_t slot)
{
    if (slot >= ESL_PRODUCT_MAX_TAGS) {
        return false;
    }
    esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
    if (w->valid != 1) {
        return false;
    }
    return s_inventory.runtime[slot].bound_record_generation ==
           esl_product_rd_le32(w->record_generation_le);
}

static void inventory_release_conn(uint16_t conn_handle)
{
    if (conn_handle == INVENTORY_CONN_INVALID) {
        return;
    }

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (s_inventory.blob.tags[i].valid == 1 &&
                s_inventory.runtime[i].conn_handle == conn_handle) {
            s_inventory.runtime[i].conn_handle = INVENTORY_CONN_INVALID;
        }
    }
}

void app_inventory_dump(const char *reason)
{
    const char *tag = reason != NULL ? reason : "?";

    refresh_count();
    ESP_LOGI(TAG, "INV [%s]: count=%u sel=%u blob_gen=%lu",
             tag, s_inventory.count, s_inventory.selected_index,
             (unsigned long)esl_product_rd_le32(s_inventory.blob.header.generation_le));

    if (s_inventory.count == 0) {
        ESP_LOGI(TAG, "INV [%s]: (count=0, no entries)", tag);
    }

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        if (w->valid != 1) {
            continue;
        }
        esl_ap_tag_runtime_t *rt = &s_inventory.runtime[i];
        char addr[18];
        format_addr_diag(w->addr, addr, sizeof(addr));
        uint16_t esl_addr = esl_product_wire_esl_addr(w);
        ESP_LOGI(TAG, "INV [%s]: [%u] esl_id=%u group=%u esl_addr=0x%04x phase=%s "
                 "desired=%u q=%u gen=%lu product=%s esl_rt=%s addr=%s",
                 tag, i, w->esl_id, w->group_id, esl_addr, phase_str(w->provision_phase),
                 w->desired_state, w->quarantine_reason,
                 (unsigned long)esl_product_rd_le32(w->record_generation_le),
                 product_state_str(rt->product_state),
                 esl_rt_str(rt->esl_runtime_state), addr);
    }
}

esp_err_t app_inventory_init(void)
{
    memset(&s_inventory, 0, sizeof(s_inventory));
    clear_runtime_all();
    s_orphan_count = 0;
    esl_nvs_blob_prepare_header(&s_inventory.blob, 0);
    refresh_count();
    return ESP_OK;
}

esp_err_t app_inventory_load(void)
{
    esp_err_t ret = esl_nvs_load_inventory(&s_inventory.blob);
    clear_runtime_all();
    s_orphan_count = 0;

    if (ret == ESP_OK) {
        refresh_count();
        ESP_LOGI(TAG, "Loaded inventory count=%u", s_inventory.count);
        app_inventory_dump("after_load");
        return ESP_OK;
    }

    if (ret == ESP_ERR_INVALID_CRC || ret == ESP_ERR_INVALID_STATE ||
            ret == ESP_ERR_INVALID_SIZE) {
        ESP_LOGE(TAG, "Inventory unreadable: %s (NVS preserved)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGW(TAG, "Failed to load inventory: %s", esp_err_to_name(ret));
    return ret;
}

esp_err_t app_inventory_save(void)
{
    refresh_count();
    return esl_nvs_save_inventory(&s_inventory.blob);
}

esl_product_inventory_t *app_inventory_get(void)
{
    refresh_count();
    return &s_inventory;
}

esl_inventory_blob_t *app_inventory_get_blob(void)
{
    return &s_inventory.blob;
}

esl_ap_tag_runtime_t *app_inventory_get_runtime(uint8_t slot)
{
    if (slot >= ESL_PRODUCT_MAX_TAGS) {
        return NULL;
    }
    return &s_inventory.runtime[slot];
}

esl_product_tag_wire_t *app_inventory_selected_wire(void)
{
    refresh_count();
    if (s_inventory.count == 0) {
        return NULL;
    }
    if (s_inventory.selected_index >= ESL_PRODUCT_MAX_TAGS ||
            s_inventory.blob.tags[s_inventory.selected_index].valid != 1) {
        for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
            if (s_inventory.blob.tags[i].valid == 1) {
                s_inventory.selected_index = i;
                break;
            }
        }
    }
    if (s_inventory.blob.tags[s_inventory.selected_index].valid != 1) {
        return NULL;
    }
    return &s_inventory.blob.tags[s_inventory.selected_index];
}

esl_product_tag_record_t *app_inventory_selected_tag(void)
{
    esl_product_tag_wire_t *w = app_inventory_selected_wire();
    if (w == NULL) {
        return NULL;
    }
    return make_view(s_inventory.selected_index, &s_selected_view);
}

static bool provision_phase_incomplete(uint8_t phase)
{
    return phase == ESL_PRODUCT_PHASE_PREPARED ||
           phase == ESL_PRODUCT_PHASE_BONDED ||
           phase == ESL_PRODUCT_PHASE_CONFIGURED;
}

esp_err_t app_inventory_prepare_provision(const uint8_t addr[6], uint8_t addr_type,
                                          uint8_t esl_id, uint8_t group_id)
{
    if (addr == NULL || addr_is_zero(addr) || esl_id > 0xFE || group_id > 0x7F) {
        return ESP_ERR_INVALID_ARG;
    }

    int slot = find_slot_by_addr(addr);
    if (slot >= 0) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
        if (!provision_phase_incomplete(w->provision_phase)) {
            return ESP_ERR_INVALID_STATE;
        }
        if (w->esl_id != esl_id || w->group_id != group_id) {
            return ESP_ERR_INVALID_ARG;
        }

        w->addr_type = addr_type;
        w->quarantine_reason = ESL_PRODUCT_QUARANTINE_NONE;
        w->last_failure_reason = ESL_AP_FAIL_NONE;
        ESP_LOGI(TAG, "Resume provision slot=%d esl_id=%u group=%u phase=%s",
                 slot, esl_id, group_id, phase_str(w->provision_phase));
        return app_inventory_save();
    }

    if (find_slot_by_esl(esl_id, group_id) >= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    slot = find_free_slot();
    if (slot < 0) {
        return ESP_ERR_NO_MEM;
    }

    esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
    esl_ap_tag_runtime_t *rt = &s_inventory.runtime[slot];
    uint32_t gen = next_record_generation((uint8_t)slot);

    memset(w, 0, sizeof(*w));
    memset(rt, 0, sizeof(*rt));
    memcpy(w->addr, addr, ESL_PRODUCT_ADDR_LEN);
    w->addr_type = addr_type;
    w->esl_id = esl_id;
    w->group_id = group_id;
    w->valid = 1;
    w->provision_phase = ESL_PRODUCT_PHASE_PREPARED;
    w->desired_state = ESL_PRODUCT_DESIRED_ACTIVE;
    w->quarantine_reason = ESL_PRODUCT_QUARANTINE_NONE;
    w->record_generation_le = esl_product_wr_le32(gen);
    w->last_failure_reason = ESL_AP_FAIL_NONE;

    rt->conn_handle = INVENTORY_CONN_INVALID;
    rt->esl_runtime_state = BLE_ESL_STATE_CONFIGURING;
    rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
    rt->bound_record_generation = gen;
    rt->failure_reason = ESL_AP_FAIL_NONE;

    refresh_count();
    ESP_LOGI(TAG, "Prepared provision slot=%d esl_id=%u group=%u", slot, esl_id, group_id);
    return app_inventory_save();
}

esp_err_t app_inventory_upsert_from_connection(uint16_t conn_handle, const uint8_t addr[6],
                                               uint8_t addr_type, bool has_display,
                                               bool has_led, bool has_sensor)
{
    inventory_release_conn(conn_handle);

    int slot = find_slot_by_addr(addr);
    if (slot >= 0) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
        esl_ap_tag_runtime_t *rt = &s_inventory.runtime[slot];
        w->addr_type = addr_type;
        w->capability_flags = 0;
        if (has_display) {
            w->capability_flags |= ESL_PRODUCT_CAP_DISPLAY;
        }
        if (has_led) {
            w->capability_flags |= ESL_PRODUCT_CAP_LED;
        }
        if (has_sensor) {
            w->capability_flags |= ESL_PRODUCT_CAP_SENSOR;
        }
        rt->conn_handle = conn_handle;
        return ESP_OK; /* conn_handle is RAM-only; no NVS write */
    }

    ESP_LOGW(TAG, "upsert: peer has no prepared inventory transaction");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t app_inventory_release_conn(uint16_t conn_handle)
{
    inventory_release_conn(conn_handle);
    return ESP_OK;
}

esl_product_tag_wire_t *app_inventory_find_wire_for_conn(uint16_t conn_handle, const uint8_t addr[6])
{
    if (addr != NULL && !addr_is_zero(addr)) {
        int slot = find_slot_by_addr(addr);
        if (slot >= 0) {
            return &s_inventory.blob.tags[slot];
        }
    }

    esl_product_tag_wire_t *match = NULL;
    uint8_t matches = 0;
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        if (s_inventory.blob.tags[i].valid == 1 &&
                s_inventory.runtime[i].conn_handle == conn_handle) {
            match = &s_inventory.blob.tags[i];
            matches++;
        }
    }
    return matches == 1 ? match : NULL;
}

esl_product_tag_record_t *app_inventory_find_for_conn(uint16_t conn_handle, const uint8_t addr[6])
{
    esl_product_tag_wire_t *w = app_inventory_find_wire_for_conn(conn_handle, addr);
    if (w == NULL) {
        return NULL;
    }
    uint8_t slot = (uint8_t)(w - s_inventory.blob.tags);
    return make_view(slot, &s_find_view);
}

esp_err_t app_inventory_set_configured(uint16_t conn_handle, const uint8_t addr[6],
                                       uint8_t esl_id, uint8_t group_id,
                                       const uint8_t ap_sync_key[16], const uint8_t ap_sync_iv[8],
                                       const uint8_t resp_key[16], const uint8_t resp_iv[8])
{
    esl_product_tag_wire_t *w = app_inventory_find_wire_for_conn(conn_handle, addr);
    if (w == NULL) {
        ESP_LOGW(TAG, "set_configured: no inventory record conn=%u", conn_handle);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t slot = (uint8_t)(w - s_inventory.blob.tags);
    esl_ap_tag_runtime_t *rt = &s_inventory.runtime[slot];

    rt->conn_handle = conn_handle;
    w->esl_id = esl_id;
    w->group_id = group_id;
    memcpy(w->ap_sync_key, ap_sync_key, ESL_PRODUCT_KEY_LEN);
    memcpy(w->ap_sync_iv, ap_sync_iv, ESL_PRODUCT_IV_LEN);
    memcpy(w->resp_key, resp_key, ESL_PRODUCT_KEY_LEN);
    memcpy(w->resp_iv, resp_iv, ESL_PRODUCT_IV_LEN);
    w->provision_phase = ESL_PRODUCT_PHASE_CONFIGURED;
    w->quarantine_reason = ESL_PRODUCT_QUARANTINE_NONE;
    w->associated = 0;
    rt->esl_runtime_state = BLE_ESL_STATE_CONFIGURING;
    rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
    rt->bound_record_generation = esl_product_rd_le32(w->record_generation_le);
    return app_inventory_save();
}

esp_err_t app_inventory_set_synchronized(uint16_t esl_addr, uint8_t current_image_index)
{
    int slot = find_slot_by_esl_addr(esl_addr);
    if (slot < 0) {
        ESP_LOGW(TAG, "set_synchronized: esl_addr=0x%04x not in inventory", esl_addr);
        return ESP_ERR_NOT_FOUND;
    }

    esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
    esl_ap_tag_runtime_t *rt = &s_inventory.runtime[slot];

    rt->conn_handle = INVENTORY_CONN_INVALID;
    w->current_image_index = current_image_index;
    w->image_known = 1;
    w->associated = 1;
    w->desired_state = ESL_PRODUCT_DESIRED_ACTIVE;
    w->provision_phase = ESL_PRODUCT_PHASE_ACTIVE;
    w->quarantine_reason = ESL_PRODUCT_QUARANTINE_NONE;

    /* First CONFIGURED→ACTIVE confirmation: product ONLINE (not via recovery). */
    rt->esl_runtime_state = BLE_ESL_STATE_SYNCHRONIZED;
    rt->product_state = ESL_PRODUCT_STATE_ONLINE;
    rt->time_synced = true;
    rt->failure_reason = ESL_AP_FAIL_NONE;
    rt->bound_record_generation = esl_product_rd_le32(w->record_generation_le);

    s_inventory.selected_index = (uint8_t)slot;
    return app_inventory_save();
}

esp_err_t app_inventory_select_next(void)
{
    refresh_count();
    if (s_inventory.count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t start = s_inventory.selected_index;
    for (uint8_t step = 1; step <= ESL_PRODUCT_MAX_TAGS; step++) {
        uint8_t idx = (uint8_t)((start + step) % ESL_PRODUCT_MAX_TAGS);
        if (s_inventory.blob.tags[idx].valid == 1) {
            s_inventory.selected_index = idx;
            esl_product_tag_wire_t *w = &s_inventory.blob.tags[idx];
            ESP_LOGI(TAG, "Selected tag esl_id=%u group_id=%u", w->esl_id, w->group_id);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

bool app_inventory_has_synchronized_tag(void)
{
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        esl_ap_tag_runtime_t *rt = &s_inventory.runtime[i];
        if (w->valid != 1) {
            continue;
        }
        if (w->provision_phase == ESL_PRODUCT_PHASE_ACTIVE &&
                rt->product_state == ESL_PRODUCT_STATE_ONLINE &&
                rt->time_synced) {
            return true;
        }
    }
    return false;
}

esl_product_tag_wire_t *app_inventory_find_wire_by_addr(const uint8_t addr[6])
{
    int slot = find_slot_by_addr(addr);
    return slot >= 0 ? &s_inventory.blob.tags[slot] : NULL;
}

esl_product_tag_record_t *app_inventory_find_by_addr(const uint8_t addr[6])
{
    int slot = find_slot_by_addr(addr);
    if (slot < 0) {
        return NULL;
    }
    return make_view((uint8_t)slot, &s_find_view);
}

uint16_t app_inventory_esl_addr_wire(const esl_product_tag_wire_t *wire)
{
    return esl_product_wire_esl_addr(wire);
}

uint16_t app_inventory_esl_addr(const esl_product_tag_record_t *rec)
{
    if (rec == NULL || rec->wire == NULL) {
        return 0;
    }
    return esl_product_wire_esl_addr(rec->wire);
}

esp_err_t app_inventory_restore_runtime(void)
{
    uint8_t restored = 0;
    uint8_t skipped = 0;

    app_inventory_dump("before_restore_runtime");
    (void)app_inventory_reconcile_bonds();

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        esl_ap_tag_runtime_t *rt = &s_inventory.runtime[i];

        memset(rt, 0, sizeof(*rt));
        rt->conn_handle = INVENTORY_CONN_INVALID;
        rt->failure_reason = ESL_AP_FAIL_NONE;

        if (w->valid != 1) {
            rt->esl_runtime_state = BLE_ESL_STATE_UNASSOCIATED;
            rt->product_state = ESL_PRODUCT_STATE_INVALID;
            continue;
        }

        rt->bound_record_generation = esl_product_rd_le32(w->record_generation_le);

        if (w->quarantine_reason != ESL_PRODUCT_QUARANTINE_NONE &&
                w->quarantine_reason != ESL_PRODUCT_QUARANTINE_INVALID) {
            rt->esl_runtime_state = BLE_ESL_STATE_UNASSOCIATED;
            rt->product_state = ESL_PRODUCT_STATE_QUARANTINED;
            switch (w->quarantine_reason) {
            case ESL_PRODUCT_QUARANTINE_BOND_MISMATCH:
                rt->failure_reason = ESL_AP_FAIL_BOND_MISMATCH;
                break;
            case ESL_PRODUCT_QUARANTINE_KEY_MISSING:
                rt->failure_reason = ESL_AP_FAIL_KEY_MISSING;
                break;
            case ESL_PRODUCT_QUARANTINE_RECORD_CORRUPT:
                rt->failure_reason = ESL_AP_FAIL_RECORD_CORRUPT;
                break;
            case ESL_PRODUCT_QUARANTINE_IDENTITY_UNCERTAIN:
                rt->failure_reason = ESL_AP_FAIL_IDENTITY_UNCERTAIN;
                break;
            case ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED:
                rt->failure_reason = ESL_AP_FAIL_ADVERTISING_DETECTED;
                break;
            default:
                rt->failure_reason = ESL_AP_FAIL_BOND_MISMATCH;
                break;
            }
            skipped++;
            ESP_LOGI(TAG, "Restore [%u]: quarantined reason=%u", i, w->quarantine_reason);
            continue;
        }

        /* PREPARED/BONDED/CONFIGURED: first-config path, not recovery coordinator. */
        if (w->provision_phase != ESL_PRODUCT_PHASE_ACTIVE) {
            rt->esl_runtime_state = BLE_ESL_STATE_CONFIGURING;
            rt->product_state = ESL_PRODUCT_STATE_RECOVERING; /* placeholder only */
            skipped++;
            ESP_LOGI(TAG, "Restore skip recovery [%u]: phase=%s", i, phase_str(w->provision_phase));
            continue;
        }

        if (w->desired_state != ESL_PRODUCT_DESIRED_ACTIVE) {
            rt->esl_runtime_state = BLE_ESL_STATE_UNASSOCIATED;
            rt->product_state = ESL_PRODUCT_STATE_OFFLINE;
            skipped++;
            continue;
        }

        if (addr_is_zero(w->addr)) {
            ESP_LOGW(TAG, "Skip restore index=%u: missing BLE addr", i);
            w->quarantine_reason = ESL_PRODUCT_QUARANTINE_RECORD_CORRUPT;
            rt->product_state = ESL_PRODUCT_STATE_QUARANTINED;
            rt->failure_reason = ESL_AP_FAIL_RECORD_CORRUPT;
            skipped++;
            continue;
        }

        /* Do NOT inject default keys — missing keys quarantine the record. */
        if (keys_are_zero(w->ap_sync_key, ESL_PRODUCT_KEY_LEN) ||
                keys_are_zero(w->ap_sync_iv, ESL_PRODUCT_IV_LEN) ||
                keys_are_zero(w->resp_key, ESL_PRODUCT_KEY_LEN) ||
                keys_are_zero(w->resp_iv, ESL_PRODUCT_IV_LEN)) {
            ESP_LOGW(TAG, "Restore [%u]: key material missing -> quarantine", i);
            w->quarantine_reason = ESL_PRODUCT_QUARANTINE_KEY_MISSING;
            rt->product_state = ESL_PRODUCT_STATE_QUARANTINED;
            rt->failure_reason = ESL_AP_FAIL_KEY_MISSING;
            skipped++;
            continue;
        }

        uint16_t esl_addr = esl_product_wire_esl_addr(w);
        ble_esl_ap_persisted_esl_t info = {
            .esl_address = ble_esl_addr_make(w->esl_id, w->group_id),
            .ble_addr_type = w->addr_type,
        };
        memcpy(info.ble_addr, w->addr, ESL_PRODUCT_ADDR_LEN);
        memcpy(info.ap_sync_key.session_key, w->ap_sync_key, ESL_PRODUCT_KEY_LEN);
        memcpy(info.ap_sync_key.iv, w->ap_sync_iv, ESL_PRODUCT_IV_LEN);
        memcpy(info.resp_key.session_key, w->resp_key, ESL_PRODUCT_KEY_LEN);
        memcpy(info.resp_key.iv, w->resp_iv, ESL_PRODUCT_IV_LEN);

        esp_err_t ret = ble_esl_ap_restore_persisted_esl(&info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Restore tag %u failed: %s", i, esp_err_to_name(ret));
            rt->esl_runtime_state = BLE_ESL_STATE_UNSYNCHRONIZED;
            rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
            continue;
        }

        /* Seed UNSYNCHRONIZED + RECOVERING — never treat NVS as online. */
        rt->esl_runtime_state = BLE_ESL_STATE_UNSYNCHRONIZED;
        rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
        rt->time_synced = false;
        restored++;
        ESP_LOGI(TAG, "Restore ok [%u]: esl_id=%u esl_addr=0x%04x -> RECOVERING",
                 i, w->esl_id, esl_addr);
    }

    ESP_LOGI(TAG, "Restored %u recovering tag(s), skipped %u", restored, skipped);
    app_inventory_dump("after_restore_runtime");
    return ESP_OK;
}

bool app_inventory_has_addr(const uint8_t addr[6])
{
    return find_slot_by_addr(addr) >= 0;
}

bool app_inventory_esl_id_in_use(uint8_t esl_id, uint8_t group_id)
{
    return find_slot_by_esl(esl_id, group_id) >= 0;
}

esp_err_t app_inventory_allocate_esl_id(uint8_t *esl_id, uint8_t *group_id)
{
    if (esl_id == NULL || group_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* *group_id is the preferred group (caller sets default when omitted). */
    for (uint16_t candidate = 0; candidate <= 0xFE; candidate++) {
        if (!app_inventory_esl_id_in_use((uint8_t)candidate, *group_id)) {
            *esl_id = (uint8_t)candidate;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t app_inventory_remove(uint8_t esl_id, uint8_t group_id)
{
    int slot = find_slot_by_esl(esl_id, group_id);
    if (slot < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Fixed slots: invalidate only — do not compact. */
    memset(&s_inventory.blob.tags[slot], 0, sizeof(s_inventory.blob.tags[slot]));
    memset(&s_inventory.runtime[slot], 0, sizeof(s_inventory.runtime[slot]));
    s_inventory.runtime[slot].conn_handle = INVENTORY_CONN_INVALID;

    if (s_inventory.selected_index == (uint8_t)slot) {
        s_inventory.selected_index = 0;
        for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
            if (s_inventory.blob.tags[i].valid == 1) {
                s_inventory.selected_index = i;
                break;
            }
        }
    }

    refresh_count();
    ESP_LOGI(TAG, "Removed tag esl_id=%u group_id=%u (slot=%d kept)", esl_id, group_id, slot);
    return app_inventory_save();
}

static esp_err_t peer_has_bond(const uint8_t addr[6], uint8_t addr_type, bool *has_bond)
{
    if (has_bond == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *has_bond = false;

    ble_addr_t peers[ESL_AP_MAX_ORPHAN_BONDS];
    int num_peers = 0;
    int rc = ble_store_util_bonded_peers(peers, &num_peers, ESL_AP_MAX_ORPHAN_BONDS);
    if (rc != 0) {
        /* TODO: if ble_store_util_bonded_peers unavailable, fall back to
         * ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, ...). */
        ESP_LOGW(TAG, "bonded_peers query failed rc=%d", rc);
        return ESP_FAIL;
    }
    for (int i = 0; i < num_peers; i++) {
        if (peers[i].type == addr_type &&
                memcmp(peers[i].val, addr, ESL_PRODUCT_ADDR_LEN) == 0) {
            *has_bond = true;
            break;
        }
    }
    return ESP_OK;
}

esp_err_t app_inventory_cancel_provision(uint8_t esl_id, uint8_t group_id)
{
    int slot = find_slot_by_esl(esl_id, group_id);
    if (slot < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    esl_product_tag_wire_t *w = &s_inventory.blob.tags[slot];
    if (!provision_phase_incomplete(w->provision_phase)) {
        return ESP_ERR_INVALID_STATE;
    }

    bool has_bond = false;
    esp_err_t bond_ret = peer_has_bond(w->addr, w->addr_type, &has_bond);
    if (bond_ret != ESP_OK) {
        return bond_ret;
    }
    if (has_bond) {
        ble_addr_t peer = {
            .type = w->addr_type,
        };
        memcpy(peer.val, w->addr, sizeof(peer.val));
        int rc = ble_store_util_delete_peer(&peer);
        if (rc != 0) {
            ESP_LOGW(TAG, "Cancel provision: bond delete failed rc=%d", rc);
            return ESP_FAIL;
        }
    }

    return app_inventory_remove(esl_id, group_id);
}

esp_err_t app_inventory_reconcile_bonds(void)
{
    s_orphan_count = 0;
    bool wire_changed = false;

    ble_addr_t peers[ESL_AP_MAX_ORPHAN_BONDS];
    int num_peers = 0;
    int rc = ble_store_util_bonded_peers(peers, &num_peers, ESL_AP_MAX_ORPHAN_BONDS);
    if (rc != 0) {
        /*
         * TODO(bond-reconcile): ble_store_util_bonded_peers failed.
         * Preferred fallback shape:
         *   ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, iter_cb, &ctx);
         * where iter_cb reads val->sec.peer_addr / peer_addr_type into
         * esl_ap_orphan_bond_t / match against inventory wire.addr.
         */
        ESP_LOGW(TAG, "Bond reconcile skipped: bonded_peers rc=%d", rc);
        return ESP_ERR_INVALID_STATE;
    }

    bool matched[ESL_AP_MAX_ORPHAN_BONDS] = { 0 };

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_wire_t *w = &s_inventory.blob.tags[i];
        if (w->valid != 1) {
            continue;
        }

        bool found = false;
        for (int p = 0; p < num_peers; p++) {
            if (peers[p].type == w->addr_type &&
                    memcmp(peers[p].val, w->addr, ESL_PRODUCT_ADDR_LEN) == 0) {
                found = true;
                matched[p] = true;
                break;
            }
        }

        if (!found &&
                (w->provision_phase == ESL_PRODUCT_PHASE_ACTIVE ||
                 w->provision_phase == ESL_PRODUCT_PHASE_CONFIGURED ||
                 w->provision_phase == ESL_PRODUCT_PHASE_BONDED)) {
            ESP_LOGW(TAG, "Bond missing for slot=%u phase=%s -> BOND_MISMATCH",
                     i, phase_str(w->provision_phase));
            w->quarantine_reason = ESL_PRODUCT_QUARANTINE_BOND_MISMATCH;
            w->last_failure_reason = ESL_AP_FAIL_BOND_MISMATCH;
            wire_changed = true;
        }
    }

    int64_t now = esp_timer_get_time();
    for (int p = 0; p < num_peers && s_orphan_count < ESL_AP_MAX_ORPHAN_BONDS; p++) {
        if (matched[p]) {
            continue;
        }
        esl_ap_orphan_bond_t *ob = &s_orphan_bonds[s_orphan_count++];
        memcpy(ob->peer_id_addr, peers[p].val, ESL_PRODUCT_ADDR_LEN);
        ob->addr_type = peers[p].type;
        ob->first_seen_uptime_us = now;
    }

    if (wire_changed) {
        return app_inventory_save();
    }
    return ESP_OK;
}

const esl_ap_orphan_bond_t *app_inventory_orphan_bonds(uint8_t *out_count)
{
    if (out_count != NULL) {
        *out_count = s_orphan_count;
    }
    return s_orphan_bonds;
}
