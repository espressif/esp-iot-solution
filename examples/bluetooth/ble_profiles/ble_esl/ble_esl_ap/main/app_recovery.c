/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Hard-reboot recovery coordinator:
 * scan → ACL + bond encrypt → Absolute Time → Update Complete + PAST →
 * ACL drop → settle → arbiter recovery Ping (up to 3 tries) →
 * product ONLINE only after Ping.
 */

#include "app_recovery.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "host/ble_gap.h"
#include "ble_esl_common.h"
#include "app_inventory.h"
#include "app_esl_ap.h"
#include "esl_ap_arbiter.h"
#include "app_recovery_policy.h"
#include "app_gateway.h"
#include "sdkconfig.h"

#if CONFIG_ESL_AP_MODE_WITH_APP
#define APP_RECOVERY_HAS_GATEWAY 1
#else
#define APP_RECOVERY_HAS_GATEWAY 0
#endif

static const char *TAG = "app_recovery";

#define APP_RECOVERY_FIRST_ROUND_MS     480000u
#define APP_RECOVERY_CONNECT_TIMEOUT_MS 10000u
#define APP_RECOVERY_SYNC_TIMEOUT_MS    15000u
#define APP_RECOVERY_PING_GUARD_MS      3000u
/* After reboot the Tag is still on the old PAwR train and does not advertise
 * until its sync timeout. Scanning through that window beats a blind connect
 * that cancels discovery for 10 s. */
#define APP_RECOVERY_BOOT_SCAN_MS       12000u
#define APP_RECOVERY_BACKOFF_MIN_MS     5000u
#define APP_RECOVERY_BACKOFF_MAX_MS     300000u  /* 5 min */
#define APP_RECOVERY_TICK_MS            200u
#define APP_RECOVERY_VERIFY_PING_GUARD_MS 3000u
/* Periodic discovery restart so controller scan state cannot permanently hide
 * a known TAG that starts advertising after reboot. */
#define APP_RECOVERY_SCAN_REFRESH_MS    10000u

typedef enum {
    REC_TX_IDLE = 0,
    REC_TX_CONNECTING,
    REC_TX_WRITE_TIME,
    REC_TX_SYNCHRONIZING,
    REC_TX_PING_SETTLE,
    REC_TX_AWAIT_PING,
} recovery_tx_phase_t;

typedef struct {
    uint64_t boot_id;
    esl_ap_lifecycle_status_t lifecycle;
    bool healthy;
    bool started;
    bool paused;
    bool first_round_done;
    int64_t first_round_deadline_us;
    uint8_t rr_cursor;
    uint32_t next_request_id;
    uint32_t next_suspect_token;

    recovery_tx_phase_t tx_phase;
    uint8_t tx_slot;
    uint16_t tx_conn_handle;
    uint8_t tx_peer_addr[6];
    uint32_t tx_recovery_generation;
    uint32_t tx_record_generation;
    bool tx_past_ok;
    bool tx_via_pawr;
    uint8_t tx_ping_tries;
    int64_t tx_ping_due_us;
    bool tx_attempted[ESL_PRODUCT_MAX_TAGS];
    /* Connectable adv seen since boot. Until then the Tag may still be on PAwR
     * and not advertising; ACL connect pauses that train and times out. */
    bool adv_seen[ESL_PRODUCT_MAX_TAGS];
    bool acl_probe_after_pawr_timeout[ESL_PRODUCT_MAX_TAGS];
    int64_t boot_scan_until_us;
    app_recovery_suspect_state_t suspect[ESL_PRODUCT_MAX_TAGS];
    int64_t last_scan_refresh_us;

    esp_timer_handle_t tick_timer;
    esp_timer_handle_t connect_timer;
} app_recovery_ctx_t;

static app_recovery_ctx_t s_rec;

static bool peer_addr_is_set(const uint8_t addr[6])
{
    static const uint8_t zero[6] = { 0 };
    return memcmp(addr, zero, 6) != 0;
}

static int64_t now_us(void)
{
    return esp_timer_get_time();
}

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

/* stop first so scan_suppressed blocks auto-resume between cancel and restart */
static esp_err_t refresh_scan(void)
{
    (void)ble_esl_ap_stop_scan();
    return ble_esl_ap_start_scan();
}

static bool slot_view(uint8_t slot, esl_product_tag_record_t *out)
{
    return app_inventory_tag_view(slot, out);
}

static bool is_quarantined(const esl_product_tag_wire_t *w, const esl_ap_tag_runtime_t *rt)
{
    if (rt->product_state == ESL_PRODUCT_STATE_QUARANTINED) {
        return true;
    }
    return w->quarantine_reason != ESL_PRODUCT_QUARANTINE_NONE &&
           w->quarantine_reason != ESL_PRODUCT_QUARANTINE_INVALID;
}

static bool tag_eligible(const esl_product_tag_record_t *rec)
{
    if (rec == NULL || rec->wire == NULL || rec->rt == NULL) {
        return false;
    }
    if (rec->wire->valid != 1) {
        return false;
    }
    if (rec->wire->provision_phase != ESL_PRODUCT_PHASE_ACTIVE) {
        return false;
    }
    if (rec->wire->desired_state != ESL_PRODUCT_DESIRED_ACTIVE) {
        return false;
    }
    if (is_quarantined(rec->wire, rec->rt)) {
        return false;
    }
    if (!peer_addr_is_set(rec->wire->addr)) {
        return false;
    }
    if (!app_inventory_runtime_generation_valid(rec->slot)) {
        return false;
    }
    return rec->rt->product_state == ESL_PRODUCT_STATE_RECOVERING ||
           rec->rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT ||
           rec->rt->product_state == ESL_PRODUCT_STATE_OFFLINE;
}

static bool tag_needs_attempt_now(const esl_product_tag_record_t *rec)
{
    if (!tag_eligible(rec)) {
        return false;
    }
    if (rec->rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT ||
            rec->rt->product_state == ESL_PRODUCT_STATE_OFFLINE) {
        if (rec->rt->retry_deadline_us > 0 && now_us() < rec->rt->retry_deadline_us) {
            return false;
        }
    }
    return true;
}

static uint32_t backoff_ms_for_retry(uint16_t retry_count)
{
    uint32_t ms = APP_RECOVERY_BACKOFF_MIN_MS;
    for (uint16_t i = 0; i < retry_count && ms < APP_RECOVERY_BACKOFF_MAX_MS; i++) {
        if (ms > APP_RECOVERY_BACKOFF_MAX_MS / 2u) {
            ms = APP_RECOVERY_BACKOFF_MAX_MS;
            break;
        }
        ms *= 2u;
    }
    if (ms > APP_RECOVERY_BACKOFF_MAX_MS) {
        ms = APP_RECOVERY_BACKOFF_MAX_MS;
    }
    return ms;
}

static void emit_summary_changed(void)
{
#if APP_RECOVERY_HAS_GATEWAY
    app_gateway_on_recovery_summary_changed();
#endif
}

static void emit_tag_changed(uint8_t slot)
{
#if APP_RECOVERY_HAS_GATEWAY
    app_gateway_on_tag_recovery_changed(slot);
#else
    (void)slot;
#endif
}

static bool recovery_has_boot_scan_gateway_blocker(void)
{
    if (!s_rec.started || s_rec.paused || now_us() >= s_rec.boot_scan_until_us) {
        return false;
    }
    if (s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_RECOVERING &&
            s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_READY) {
        return false;
    }

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!slot_view(i, &view) || !tag_needs_attempt_now(&view)) {
            continue;
        }
        if (view.rt->product_state == ESL_PRODUCT_STATE_RECOVERING && !s_rec.adv_seen[i]) {
            return true;
        }
    }
    return false;
}

static void maybe_start_gateway(void)
{
#if APP_RECOVERY_HAS_GATEWAY
    if (app_recovery_blocks_gateway()) {
        ESP_LOGD(TAG, "Defer NUS Gateway during recovery radio protection");
        return;
    }
    if (!app_gateway_is_started()) {
        ESP_LOGI(TAG, "Starting deferred NUS Gateway");
        (void)app_gateway_start();
    }
#else
    (void)0;
#endif
}

static void recompute_health(void)
{
    if (s_rec.lifecycle.lifecycle == ESL_AP_LIFECYCLE_FAILED) {
        s_rec.healthy = false;
        return;
    }

    bool degraded = false;
    bool any_online = false;
    bool any_eligible_pending = false;

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!slot_view(i, &view)) {
            continue;
        }
        if (view.wire->provision_phase != ESL_PRODUCT_PHASE_ACTIVE ||
                view.wire->desired_state != ESL_PRODUCT_DESIRED_ACTIVE) {
            continue;
        }
        if (is_quarantined(view.wire, view.rt)) {
            degraded = true;
            continue;
        }
        if (view.rt->product_state == ESL_PRODUCT_STATE_ONLINE && view.rt->time_synced) {
            any_online = true;
            continue;
        }
        if (view.rt->product_state == ESL_PRODUCT_STATE_RECOVERING ||
                view.rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT ||
                view.rt->product_state == ESL_PRODUCT_STATE_OFFLINE) {
            any_eligible_pending = true;
            degraded = true;
        }
    }

    s_rec.healthy = !degraded;
    (void)any_online;
    (void)any_eligible_pending;
}

static void set_lifecycle(esl_ap_lifecycle_t life, bool retryable, esl_ap_error_code_t err)
{
    if (s_rec.lifecycle.lifecycle == life &&
            s_rec.lifecycle.retryable == retryable &&
            s_rec.lifecycle.error_code == err) {
        return;
    }
    s_rec.lifecycle.lifecycle = life;
    s_rec.lifecycle.retryable = retryable;
    s_rec.lifecycle.error_code = err;
    recompute_health();
    emit_summary_changed();
}

static void clear_tx(void)
{
    s_rec.tx_phase = REC_TX_IDLE;
    s_rec.tx_slot = 0;
    s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memset(s_rec.tx_peer_addr, 0, sizeof(s_rec.tx_peer_addr));
    s_rec.tx_recovery_generation = 0;
    s_rec.tx_record_generation = 0;
    s_rec.tx_past_ok = false;
    s_rec.tx_via_pawr = false;
    s_rec.tx_ping_tries = 0;
    s_rec.tx_ping_due_us = 0;
#if APP_RECOVERY_HAS_GATEWAY
    app_gateway_set_pawr_radio_hold(false);
#endif
    if (s_rec.connect_timer != NULL) {
        esp_timer_stop(s_rec.connect_timer);
    }
}

static void bump_generation_and_invalidate(esl_product_tag_record_t *rec)
{
    rec->rt->recovery_generation++;
    if (rec->rt->recovery_generation == 0) {
        rec->rt->recovery_generation = 1;
    }
    uint16_t esl_addr = app_inventory_esl_addr(rec);
    (void)esl_ap_arbiter_invalidate_generation(rec->wire->group_id, esl_addr,
                                               rec->rt->recovery_generation);
}

static void mark_retry_wait(uint8_t slot, esl_ap_failure_reason_t reason)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        return;
    }

    esl_product_state_t old = view.rt->product_state;
    view.rt->failure_reason = reason;
    view.rt->time_synced = false;
    view.rt->esl_runtime_state = BLE_ESL_STATE_UNSYNCHRONIZED;
    view.rt->retry_count++;
    s_rec.tx_attempted[slot] = true;

    uint32_t backoff = backoff_ms_for_retry(view.rt->retry_count);
    view.rt->retry_deadline_us = now_us() + (int64_t)backoff * 1000LL;

    if (s_rec.first_round_done) {
        view.rt->product_state = ESL_PRODUCT_STATE_OFFLINE;
    } else {
        view.rt->product_state = ESL_PRODUCT_STATE_RETRY_WAIT;
    }

    ESP_LOGW(TAG, "Tag slot=%u -> %s reason=%u backoff=%u ms gen=%" PRIu32,
             slot,
             view.rt->product_state == ESL_PRODUCT_STATE_OFFLINE ? "OFFLINE" : "RETRY_WAIT",
             (unsigned)reason, backoff, view.rt->recovery_generation);
    (void)old;
    recompute_health();
    emit_tag_changed(slot);
}

static void mark_online(uint8_t slot)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        return;
    }

    view.rt->product_state = ESL_PRODUCT_STATE_ONLINE;
    view.rt->time_synced = true;
    view.rt->failure_reason = ESL_AP_FAIL_NONE;
    view.rt->miss_count = 0;
    view.rt->retry_count = 0;
    view.rt->retry_deadline_us = 0;
    view.rt->esl_runtime_state = BLE_ESL_STATE_SYNCHRONIZED;
    view.rt->last_seen_uptime_us = now_us();
    s_rec.tx_attempted[slot] = true;
    app_recovery_suspect_clear(&s_rec.suspect[slot]);

    ESP_LOGI(TAG, "Tag slot=%u ONLINE time_synced gen=%" PRIu32,
             slot, view.rt->recovery_generation);
    recompute_health();
    emit_tag_changed(slot);
}

static void mark_recovering_from_advertising(uint8_t slot)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        return;
    }
    if (view.rt->product_state != ESL_PRODUCT_STATE_ONLINE) {
        return;
    }

    view.rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
    view.rt->time_synced = false;
    view.rt->failure_reason = ESL_AP_FAIL_ADVERTISING_DETECTED;
    view.rt->esl_runtime_state = BLE_ESL_STATE_UNSYNCHRONIZED;
    app_recovery_suspect_clear(&s_rec.suspect[slot]);

    ESP_LOGW(TAG, "Tag slot=%u ONLINE->RECOVERING (advertising detected)", slot);
    recompute_health();
    emit_tag_changed(slot);
}

static void mark_quarantine_unassociated(uint8_t slot)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        return;
    }

    /* Quarantine owns generation bump because begin_attempt() is not called. */
    bump_generation_and_invalidate(&view);
    view.wire->quarantine_reason = ESL_PRODUCT_QUARANTINE_TAG_UNASSOCIATED;
    view.rt->product_state = ESL_PRODUCT_STATE_QUARANTINED;
    view.rt->time_synced = false;
    view.rt->failure_reason = ESL_AP_FAIL_ADVERTISING_DETECTED;
    view.rt->esl_runtime_state = BLE_ESL_STATE_UNASSOCIATED;
    app_recovery_suspect_clear(&s_rec.suspect[slot]);
    (void)app_inventory_save();

    ESP_LOGW(TAG, "Tag slot=%u QUARANTINED tag_unassociated (requires reprovision)", slot);
    recompute_health();
    emit_tag_changed(slot);
}

static esp_err_t submit_verify_ping(uint8_t slot, uint32_t suspect_token)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        return ESP_ERR_INVALID_STATE;
    }

    esl_ap_arbiter_request_t req;
    memset(&req, 0, sizeof(req));
    req.request_id = suspect_token;
    req.boot_id = s_rec.boot_id;
    req.recovery_generation = 0; /* verify ping marker while still ONLINE */
    req.record_generation = esl_product_rd_le32(view.wire->record_generation_le);
    req.record_slot = slot;
    req.esl_addr = app_inventory_esl_addr(&view);
    req.group_id = view.wire->group_id;
    req.command_opcode = BLE_ESL_CMD_PING;
    req.expected_response = ESL_AP_EXPECT_BASIC_STATE;
    req.priority = ESL_AP_ARBITER_PRIO_RECOVERY;
    req.guard_timeout_ms = APP_RECOVERY_VERIFY_PING_GUARD_MS;
    req.params_len = 1;
    req.params[0] = view.wire->esl_id;

    esp_err_t ret = esl_ap_arbiter_submit(&req);
    if (ret == ESP_OK) {
        s_rec.suspect[slot].ping_pending = true;
        ESP_LOGI(TAG, "Verify Ping queued slot=%u token=%" PRIu32, slot, suspect_token);
    } else {
        ESP_LOGD(TAG, "Verify Ping submit deferred slot=%u: %s",
                 slot, esp_err_to_name(ret));
    }
    return ret;
}

static void handle_online_suspect(uint8_t slot, app_recovery_suspect_kind_t kind)
{
    uint32_t token = ++s_rec.next_suspect_token;
    if (token == 0) {
        token = ++s_rec.next_suspect_token;
    }
    bool submit = app_recovery_suspect_on_adv(&s_rec.suspect[slot], kind, now_us(), token);
    if (submit) {
        (void)submit_verify_ping(slot, s_rec.suspect[slot].suspect_token);
    }
}

static void maybe_finish_first_round(void)
{
    if (s_rec.tx_phase != REC_TX_IDLE) {
        return;
    }
    if (s_rec.first_round_done ||
            s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_RECOVERING) {
        return;
    }

    bool budget_expired = now_us() >= s_rec.first_round_deadline_us;
    bool all_attempted = true;
    bool any_eligible = false;

    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!slot_view(i, &view)) {
            continue;
        }
        if (view.wire->provision_phase != ESL_PRODUCT_PHASE_ACTIVE ||
                view.wire->desired_state != ESL_PRODUCT_DESIRED_ACTIVE ||
                is_quarantined(view.wire, view.rt)) {
            continue;
        }
        any_eligible = true;
        if (view.rt->product_state == ESL_PRODUCT_STATE_ONLINE && view.rt->time_synced) {
            continue;
        }
        /* AUX_CONNECT miss is not a finished attempt; keep first round open. */
        if (view.rt->failure_reason == ESL_AP_FAIL_ACL_TIMEOUT) {
            all_attempted = false;
            continue;
        }
        if (!s_rec.tx_attempted[i] && view.rt->retry_count == 0) {
            all_attempted = false;
        }
    }

    if (!any_eligible) {
        s_rec.first_round_done = true;
        set_lifecycle(ESL_AP_LIFECYCLE_READY, false, ESL_AP_ERROR_NONE);
        ESP_LOGI(TAG, "First round: no eligible tags -> READY");
        return;
    }

    if (!budget_expired && !all_attempted) {
        return;
    }

    s_rec.first_round_done = true;
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!slot_view(i, &view)) {
            continue;
        }
        if (!tag_eligible(&view)) {
            continue;
        }
        if (view.rt->product_state == ESL_PRODUCT_STATE_ONLINE) {
            continue;
        }
        if (view.rt->product_state == ESL_PRODUCT_STATE_RECOVERING ||
                view.rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT) {
            view.rt->product_state = ESL_PRODUCT_STATE_OFFLINE;
            if (view.rt->failure_reason == ESL_AP_FAIL_NONE) {
                view.rt->failure_reason = ESL_AP_FAIL_UNREACHABLE;
            }
            if (view.rt->retry_deadline_us == 0) {
                view.rt->retry_deadline_us =
                    now_us() + (int64_t)APP_RECOVERY_BACKOFF_MIN_MS * 1000LL;
            }
            emit_tag_changed(i);
        }
    }

    set_lifecycle(ESL_AP_LIFECYCLE_READY, false, ESL_AP_ERROR_NONE);
    ESP_LOGI(TAG, "First round done (budget=%d all_attempted=%d) -> READY degraded=%d",
             budget_expired, all_attempted, app_recovery_is_degraded());
}

static void arm_connect_timeout(void)
{
    if (s_rec.connect_timer == NULL) {
        return;
    }
    esp_timer_stop(s_rec.connect_timer);
    esp_timer_start_once(s_rec.connect_timer,
                         (uint64_t)APP_RECOVERY_CONNECT_TIMEOUT_MS * 1000ULL);
}

static void arm_sync_timeout(void)
{
    if (s_rec.connect_timer == NULL) {
        return;
    }
    esp_timer_stop(s_rec.connect_timer);
    esp_timer_start_once(s_rec.connect_timer,
                         (uint64_t)APP_RECOVERY_SYNC_TIMEOUT_MS * 1000ULL);
}

static void begin_attempt(uint8_t slot)
{
    if (s_rec.tx_phase != REC_TX_IDLE) {
        return;
    }
    if (s_rec.paused || app_esl_ap_pipeline_busy()) {
        return;
    }

    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !tag_needs_attempt_now(&view)) {
        return;
    }

    if (ble_gap_disc_active()) {
        int rc = ble_gap_disc_cancel();
        if (rc != 0 && rc != BLE_HS_EALREADY) {
            ESP_LOGW(TAG, "disc_cancel before connect failed: %d", rc);
        }
    }

    /* Re-check while still IDLE so pipeline_busy() cannot see our own TX. */
    if (s_rec.paused || app_esl_ap_pipeline_busy()) {
        return;
    }

#if APP_RECOVERY_HAS_GATEWAY
    app_gateway_set_pawr_radio_hold(true);
#endif

    bump_generation_and_invalidate(&view);
    view.rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
    view.rt->time_synced = false;
    view.rt->failure_reason = ESL_AP_FAIL_NONE;

    s_rec.tx_phase = REC_TX_CONNECTING;
    s_rec.tx_slot = slot;
    s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    memcpy(s_rec.tx_peer_addr, view.wire->addr, 6);
    s_rec.tx_recovery_generation = view.rt->recovery_generation;
    s_rec.tx_record_generation = esl_product_rd_le32(view.wire->record_generation_le);
    s_rec.tx_past_ok = false;
    s_rec.tx_ping_tries = 0;
    s_rec.tx_ping_due_us = 0;
    s_rec.tx_attempted[slot] = true;
    s_rec.rr_cursor = (uint8_t)((slot + 1) % ESL_PRODUCT_MAX_TAGS);

    uint16_t esl_addr = app_inventory_esl_addr(&view);
    esp_err_t ret;
    /* ACL pauses PAwR. A real connectable advertisement proves the Tag is no
     * longer following PAwR. A previous PAwR-connect timeout only permits one
     * ACL probe; it is tracked separately from real advertising evidence. */
    bool use_acl = s_rec.adv_seen[slot] || s_rec.acl_probe_after_pawr_timeout[slot];
    if (use_acl && !s_rec.adv_seen[slot]) {
        s_rec.acl_probe_after_pawr_timeout[slot] = false;
    }
    s_rec.tx_via_pawr = false;
    if (!use_acl) {
        ESP_LOGI(TAG, "Recovery PAwR connect slot=%u esl_addr=0x%04x gen=%" PRIu32
                 " adv_seen=%d",
                 slot, esl_addr, view.rt->recovery_generation, (int)s_rec.adv_seen[slot]);
        ble_esl_address_t profile_addr =
            esl_product_pack_esl_addr(view.wire->esl_id, view.wire->group_id);
        ret = ble_esl_ap_connect_synced(profile_addr);
        if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "Recovery PAwR connect: no facade tag, fallback ACL");
            ret = ble_esl_ap_connect(view.wire->addr, view.wire->addr_type);
        } else {
            s_rec.tx_via_pawr = (ret == ESP_OK);
        }
    } else {
        ESP_LOGI(TAG, "Recovery ACL connect slot=%u esl_addr=0x%04x gen=%" PRIu32,
                 slot, esl_addr, view.rt->recovery_generation);
        ret = ble_esl_ap_connect(view.wire->addr, view.wire->addr_type);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Recovery connect failed: %s", esp_err_to_name(ret));
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_UNREACHABLE);
        maybe_start_gateway();
        maybe_finish_first_round();
        return;
    }
    arm_connect_timeout();
    emit_tag_changed(slot);
}

static void try_schedule_next(void)
{
    if (s_rec.paused || s_rec.tx_phase != REC_TX_IDLE) {
        return;
    }
    if (app_esl_ap_pipeline_busy()) {
        return;
    }
    if (s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_RECOVERING &&
            s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_READY) {
        return;
    }

    /* Promote due RETRY_WAIT/OFFLINE back to RECOVERING for scheduling. */
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        esl_product_tag_record_t view;
        if (!slot_view(i, &view) || !tag_eligible(&view)) {
            continue;
        }
        if ((view.rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT ||
                view.rt->product_state == ESL_PRODUCT_STATE_OFFLINE) &&
                view.rt->retry_deadline_us > 0 &&
                now_us() >= view.rt->retry_deadline_us) {
            view.rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
            view.rt->retry_deadline_us = 0;
            emit_tag_changed(i);
        }
    }

    uint8_t start = s_rec.rr_cursor % ESL_PRODUCT_MAX_TAGS;
    for (uint8_t step = 0; step < ESL_PRODUCT_MAX_TAGS; step++) {
        uint8_t i = (uint8_t)((start + step) % ESL_PRODUCT_MAX_TAGS);
        esl_product_tag_record_t view;
        if (!slot_view(i, &view) || !tag_needs_attempt_now(&view)) {
            continue;
        }
        if (view.rt->product_state != ESL_PRODUCT_STATE_RECOVERING) {
            continue;
        }
        /* No advertisement yet: the Tag may still be synced to the previous
         * train. A connect here cancels scan and misses the adv that follows
         * sync loss. */
        if (!s_rec.adv_seen[i] && now_us() < s_rec.boot_scan_until_us) {
            continue;
        }
        begin_attempt(i);
        return;
    }

    maybe_finish_first_round();

    /* Keep scanning for unsynchronized / reboot advertising. Gateway scan_result
     * is already rate-limited, so NUS is not flooded. */
    if (s_rec.paused) {
        return;
    }
    /* Restarting discovery while PAwR connect / OTS / provision owns the
     * radio races instance 0 and starves AUX_CONNECT_REQ. */
    if (app_esl_ap_pipeline_busy()) {
        return;
    }
    /* Restarting discovery while a PAwR command is in flight starves the
     * response slot (phone Ping times out even though the Tag answered). */
    for (uint8_t g = 0; g < ESL_AP_ARBITER_MAX_GROUPS; g++) {
        if (esl_ap_arbiter_group_busy(g, NULL)) {
            return;
        }
    }
    int64_t now = now_us();
    bool due = (s_rec.last_scan_refresh_us == 0) ||
               ((now - s_rec.last_scan_refresh_us) >=
                (int64_t)APP_RECOVERY_SCAN_REFRESH_MS * 1000LL);
    if (!ble_gap_disc_active() || due) {
        if (refresh_scan() == ESP_OK) {
            s_rec.last_scan_refresh_us = now;
        }
    }
    maybe_start_gateway();
}

void app_recovery_handle_connect_timeout(void)
{
    if (s_rec.tx_phase == REC_TX_CONNECTING) {
        uint8_t slot = s_rec.tx_slot;
        /* A miss means this Tag is not on the current PAwR train (stale sync
         * after AP reboot, or already advertising). Next try must be ACL. */
        if (s_rec.tx_via_pawr && slot < ESL_PRODUCT_MAX_TAGS) {
            s_rec.acl_probe_after_pawr_timeout[slot] = true;
            ESP_LOGW(TAG, "Recovery PAwR connect timeout slot=%u; allow one ACL probe", slot);
        } else {
            ESP_LOGW(TAG, "Recovery connect timeout slot=%u", slot);
        }
        ble_esl_ap_abort_tag_connections();
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_ACL_TIMEOUT);
        maybe_start_gateway();
        maybe_finish_first_round();
        try_schedule_next();
        return;
    }
    if (s_rec.tx_phase == REC_TX_WRITE_TIME) {
        uint8_t slot = s_rec.tx_slot;
        ESP_LOGW(TAG, "Recovery Absolute Time timeout slot=%u", slot);
        if (s_rec.tx_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ble_esl_ap_disconnect(s_rec.tx_conn_handle);
        }
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_TIME_SYNC_FAILED);
        maybe_finish_first_round();
        try_schedule_next();
        return;
    }
    if (s_rec.tx_phase == REC_TX_SYNCHRONIZING) {
        uint8_t slot = s_rec.tx_slot;
        ESP_LOGW(TAG, "Recovery sync timeout slot=%u", slot);
        if (s_rec.tx_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ble_esl_ap_disconnect(s_rec.tx_conn_handle);
        }
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_PAST_FAILED);
        maybe_finish_first_round();
        try_schedule_next();
    }
}

static void connect_timeout_cb(void *arg)
{
    (void)arg;
    if (!app_esl_ap_post_recovery_connect_timeout()) {
        ESP_LOGW(TAG, "Recovery connect timeout event dropped (queue full)");
    }
}

bool app_recovery_on_abs_time_written(const ble_esl_ap_abs_time_written_t *info)
{
    if (info == NULL) {
        return false;
    }
    uint16_t conn_handle = info->conn_handle;
    esp_err_t status = info->status;
    uint8_t slot = s_rec.tx_slot;

    if (s_rec.tx_phase != REC_TX_WRITE_TIME || s_rec.tx_conn_handle != conn_handle) {
        ESP_LOGD(TAG, "Stale abs-time event conn=%u", conn_handle);
        return false;
    }

    if (s_rec.connect_timer != NULL) {
        esp_timer_stop(s_rec.connect_timer);
    }

    if (status != ESP_OK) {
        ESP_LOGW(TAG, "Absolute Time write failed: %s", esp_err_to_name(status));
        esl_ap_failure_reason_t reason = ESL_AP_FAIL_TIME_SYNC_FAILED;
        if (status == ESP_ERR_NOT_ALLOWED || status == ESP_ERR_INVALID_STATE) {
            reason = ESL_AP_FAIL_WRITE_NOT_PERMITTED;
        }
        ble_esl_ap_disconnect(conn_handle);
        clear_tx();
        mark_retry_wait(slot, reason);
        maybe_finish_first_round();
        try_schedule_next();
        return true;
    }

    ESP_LOGI(TAG, "Absolute Time OK slot=%u; synchronize", slot);
    s_rec.tx_phase = REC_TX_SYNCHRONIZING;
    arm_sync_timeout();
    esp_err_t ret = ble_esl_ap_synchronize(conn_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "synchronize failed: %s", esp_err_to_name(ret));
        ble_esl_ap_disconnect(conn_handle);
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_PAST_FAILED);
        maybe_finish_first_round();
        try_schedule_next();
    }
    return true;
}

static void arm_ping_settle(uint8_t slot)
{
    if (s_rec.tx_phase == REC_TX_PING_SETTLE || s_rec.tx_phase == REC_TX_AWAIT_PING) {
        return;
    }
    s_rec.tx_phase = REC_TX_PING_SETTLE;
    s_rec.tx_slot = slot;
    s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_rec.tx_ping_tries = 0;
    s_rec.tx_ping_due_us = now_us() + (int64_t)APP_RECOVERY_PING_SETTLE_MS * 1000LL;
    if (s_rec.connect_timer != NULL) {
        esp_timer_stop(s_rec.connect_timer);
    }
    ESP_LOGI(TAG, "Recovery Ping settle %u ms slot=%u",
             (unsigned)APP_RECOVERY_PING_SETTLE_MS, slot);
#if APP_RECOVERY_HAS_GATEWAY
    /* Phone ACL (and NUS adv) shares the radio with the PAwR response slot.
     * The Tag still decrypts Ping; the AP never sees PER_SUBEV_RESP. */
    app_gateway_set_pawr_radio_hold(true);
#endif
}

static void submit_recovery_ping(uint8_t slot)
{
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        clear_tx();
        return;
    }

    esl_ap_arbiter_request_t req;
    memset(&req, 0, sizeof(req));
    req.request_id = ++s_rec.next_request_id;
    req.boot_id = s_rec.boot_id;
    req.recovery_generation = view.rt->recovery_generation;
    req.record_generation = esl_product_rd_le32(view.wire->record_generation_le);
    req.record_slot = slot;
    req.esl_addr = app_inventory_esl_addr(&view);
    req.group_id = view.wire->group_id;
    req.command_opcode = BLE_ESL_CMD_PING;
    req.expected_response = ESL_AP_EXPECT_BASIC_STATE;
    req.priority = ESL_AP_ARBITER_PRIO_RECOVERY;
    req.guard_timeout_ms = APP_RECOVERY_PING_GUARD_MS;
    req.params_len = 1;
    req.params[0] = view.wire->esl_id;

    s_rec.tx_phase = REC_TX_AWAIT_PING;
    /* ACL transaction complete after PAST; Ping is arbiter-owned. */
    s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    if (s_rec.tx_ping_tries < 255) {
        s_rec.tx_ping_tries++;
    }
    if (s_rec.connect_timer != NULL) {
        esp_timer_stop(s_rec.connect_timer);
    }

    esp_err_t ret = esl_ap_arbiter_submit(&req);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Recovery Ping submit failed: %s", esp_err_to_name(ret));
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_PING_TIMEOUT);
        maybe_finish_first_round();
        try_schedule_next();
        return;
    }
    ESP_LOGI(TAG, "Recovery Ping queued slot=%u esl_addr=0x%04x gen=%" PRIu32
             " try=%u/%u",
             slot, req.esl_addr, req.recovery_generation,
             (unsigned)s_rec.tx_ping_tries, (unsigned)APP_RECOVERY_PING_MAX_TRIES);
}

static void arbiter_send_cb(const esl_ap_arbiter_request_t *request)
{
    if (request == NULL) {
        return;
    }
    uint8_t esl_id = (uint8_t)(request->esl_addr & 0xFF);
    uint8_t group_id = request->group_id;
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    /* Broadcast and unicast both use the atomic PAwR-only entry. */
    const uint8_t *params = request->params;
    ret = ble_esl_ap_pawr_command(esl_id, group_id, request->command_opcode,
                                  params, request->params_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "arbiter send failed opcode=0x%02x esl=0x%04x: %s",
                 request->command_opcode, request->esl_addr, esp_err_to_name(ret));
        (void)esl_ap_arbiter_complete(group_id, request->esl_addr,
                                      request->command_opcode, request->request_id, ret);
        return;
    }
    if (esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        (void)esl_ap_arbiter_complete(group_id, request->esl_addr,
                                      request->command_opcode, request->request_id, ret);
    }
}

static void arbiter_completion_cb(const esl_ap_arbiter_request_t *request,
                                  const uint8_t *data, uint8_t len, esp_err_t status)
{
    (void)data;
    (void)len;
    if (request == NULL) {
        return;
    }

    if (request->boot_id != 0 && request->boot_id != s_rec.boot_id) {
        ESP_LOGD(TAG, "Ignore arbiter completion from other boot");
        return;
    }

    /* ONLINE advertising verify Ping: RECOVERY prio + generation 0. */
    if (request->priority == ESL_AP_ARBITER_PRIO_RECOVERY &&
            request->recovery_generation == 0) {
        uint8_t slot = request->record_slot;
        if (slot >= ESL_PRODUCT_MAX_TAGS) {
            return;
        }
        esl_product_tag_record_t view;
        bool still_online = false;
        if (slot_view(slot, &view) &&
                app_inventory_runtime_generation_valid(slot) &&
                esl_product_rd_le32(view.wire->record_generation_le) ==
                request->record_generation &&
                view.rt->product_state == ESL_PRODUCT_STATE_ONLINE) {
            still_online = true;
        }

        bool radio_timeout = (status == ESP_ERR_TIMEOUT ||
                              status == ESP_ERR_INVALID_RESPONSE ||
                              (status != ESP_OK && status != ESP_ERR_INVALID_STATE &&
                               status != ESP_ERR_NO_MEM));
        if (status == ESP_ERR_NO_MEM || status == ESP_ERR_INVALID_STATE) {
            radio_timeout = false; /* queue/cancel */
        }

        app_recovery_confirm_decision_t dec =
            app_recovery_suspect_on_ping_result(&s_rec.suspect[slot],
                                                request->request_id,
                                                still_online,
                                                radio_timeout && status != ESP_OK,
                                                status == ESP_OK,
                                                now_us());
        if (dec == APP_RECOVERY_CONFIRM_CLEAR_SUSPECT) {
            ESP_LOGI(TAG, "Verify Ping OK slot=%u — keep ONLINE", slot);
            return;
        }
        if (dec == APP_RECOVERY_CONFIRM_COUNT_MISS) {
            ESP_LOGD(TAG, "Verify Ping miss slot=%u count=%u",
                     slot, s_rec.suspect[slot].confirm_misses);
            return;
        }
        if (dec == APP_RECOVERY_CONFIRM_CONFIRMED) {
            app_recovery_suspect_kind_t kind = s_rec.suspect[slot].kind;
            if (kind == APP_RECOVERY_SUSPECT_UNASSOCIATED) {
                mark_quarantine_unassociated(slot);
            } else {
                mark_recovering_from_advertising(slot);
                if (!app_recovery_tx_busy() && !s_rec.paused) {
                    begin_attempt(slot);
                }
            }
        }
        return;
    }

    if (request->priority == ESL_AP_ARBITER_PRIO_APP) {
#if APP_RECOVERY_HAS_GATEWAY
        app_gateway_on_arbiter_complete(request, status);
#endif
        if (status == ESP_OK && request->record_slot < ESL_PRODUCT_MAX_TAGS) {
            esl_product_tag_record_t view;
            if (slot_view(request->record_slot, &view) &&
                    app_inventory_runtime_generation_valid(request->record_slot) &&
                    esl_product_rd_le32(view.wire->record_generation_le) ==
                    request->record_generation) {
                view.rt->last_seen_uptime_us = now_us();
                view.rt->miss_count = 0;
            }
        }
        return;
    }

    if (request->priority != ESL_AP_ARBITER_PRIO_RECOVERY &&
            request->recovery_generation == 0) {
        /* Health completions: refresh last_seen on success. */
        if (status == ESP_OK && request->record_slot < ESL_PRODUCT_MAX_TAGS) {
            esl_product_tag_record_t view;
            if (slot_view(request->record_slot, &view) &&
                    app_inventory_runtime_generation_valid(request->record_slot) &&
                    esl_product_rd_le32(view.wire->record_generation_le) ==
                    request->record_generation) {
                view.rt->last_seen_uptime_us = now_us();
                view.rt->miss_count = 0;
            }
        }
        return;
    }

    uint8_t slot = request->record_slot;
    esl_product_tag_record_t view;
    if (!slot_view(slot, &view) || !app_inventory_runtime_generation_valid(slot)) {
        if (s_rec.tx_phase == REC_TX_AWAIT_PING && s_rec.tx_slot == slot) {
            clear_tx();
        }
        return;
    }
    if (esl_product_rd_le32(view.wire->record_generation_le) != request->record_generation ||
            view.rt->recovery_generation != request->recovery_generation) {
        ESP_LOGD(TAG, "Stale recovery Ping completion slot=%u", slot);
        if (s_rec.tx_phase == REC_TX_AWAIT_PING && s_rec.tx_slot == slot &&
                s_rec.tx_recovery_generation == request->recovery_generation) {
            clear_tx();
            try_schedule_next();
        }
        return;
    }

    if (status == ESP_OK) {
        mark_online(slot);
    } else if (status == ESP_ERR_INVALID_STATE) {
        /* Cancelled / stale — ignore for product state. */
        ESP_LOGD(TAG, "Recovery Ping cancelled slot=%u", slot);
    } else {
        app_recovery_ping_action_t act =
            app_recovery_policy_on_ping_result(false, s_rec.tx_ping_tries);
        if (act == APP_RECOVERY_PING_ACT_RETRY &&
                s_rec.tx_phase == REC_TX_AWAIT_PING && s_rec.tx_slot == slot) {
            ESP_LOGW(TAG, "Recovery Ping timeout slot=%u try=%u/%u; retry",
                     slot, (unsigned)s_rec.tx_ping_tries,
                     (unsigned)APP_RECOVERY_PING_MAX_TRIES);
            submit_recovery_ping(slot);
            return;
        }
        mark_retry_wait(slot, ESL_AP_FAIL_PING_TIMEOUT);
    }

    if (s_rec.tx_phase == REC_TX_AWAIT_PING && s_rec.tx_slot == slot) {
        clear_tx();
    }
    maybe_finish_first_round();
    try_schedule_next();
}

void app_recovery_handle_tick(void)
{
    esl_ap_arbiter_tick(now_ms());

    int64_t now = now_us();
    if (s_rec.tx_phase == REC_TX_PING_SETTLE &&
            !s_rec.paused &&
            now >= s_rec.tx_ping_due_us) {
        submit_recovery_ping(s_rec.tx_slot);
    }
    for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
        app_recovery_suspect_state_t *s = &s_rec.suspect[i];
        if (!s->advertising_suspect) {
            continue;
        }
        if (s->suspect_deadline_us > 0 && now >= s->suspect_deadline_us) {
            app_recovery_suspect_clear(s);
            continue;
        }
        if (!s->ping_pending &&
                s->confirm_misses < APP_RECOVERY_CONFIRM_MISS_NEED &&
                !s_rec.paused && !app_recovery_tx_busy()) {
            (void)submit_verify_ping(i, s->suspect_token);
        }
    }

    maybe_finish_first_round();
    try_schedule_next();
}

static void tick_cb(void *arg)
{
    (void)arg;
    /* Drop quietly if queue is saturated; next periodic tick retries. */
    (void)app_esl_ap_post_recovery_tick();
}

static uint64_t generate_boot_id(void)
{
    uint32_t hi = esp_random();
    uint32_t lo = esp_random();
    uint64_t id = ((uint64_t)hi << 32) | (uint64_t)lo;
    if (id == 0) {
        id = 1;
    }
    return id;
}

esp_err_t app_recovery_init(void)
{
    memset(&s_rec, 0, sizeof(s_rec));
    s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_rec.lifecycle.lifecycle = ESL_AP_LIFECYCLE_BOOTING;
    s_rec.lifecycle.error_code = ESL_AP_ERROR_NONE;
    s_rec.healthy = true;

    esp_err_t ret = esl_ap_arbiter_init(arbiter_completion_cb);
    if (ret != ESP_OK) {
        return ret;
    }
    esl_ap_arbiter_set_send_cb(arbiter_send_cb);

    esp_timer_create_args_t tick_args = {
        .callback = tick_cb,
        .name = "esl_rec_tick",
    };
    ret = esp_timer_create(&tick_args, &s_rec.tick_timer);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_timer_create_args_t conn_args = {
        .callback = connect_timeout_cb,
        .name = "esl_rec_conn",
    };
    ret = esp_timer_create(&conn_args, &s_rec.connect_timer);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t app_recovery_start(void)
{
    if (s_rec.lifecycle.lifecycle == ESL_AP_LIFECYCLE_FAILED &&
            !s_rec.lifecycle.retryable) {
        ESP_LOGW(TAG, "Recovery start skipped: lifecycle FAILED");
        return ESP_ERR_INVALID_STATE;
    }

    s_rec.boot_id = generate_boot_id();
    s_rec.started = true;
    s_rec.first_round_done = false;
    s_rec.first_round_deadline_us =
        now_us() + (int64_t)APP_RECOVERY_FIRST_ROUND_MS * 1000LL;
    memset(s_rec.tx_attempted, 0, sizeof(s_rec.tx_attempted));
    memset(s_rec.adv_seen, 0, sizeof(s_rec.adv_seen));
    memset(s_rec.acl_probe_after_pawr_timeout, 0, sizeof(s_rec.acl_probe_after_pawr_timeout));
    clear_tx();

    set_lifecycle(ESL_AP_LIFECYCLE_RECOVERING, false, ESL_AP_ERROR_NONE);
    ESP_LOGI(TAG, "Recovery start boot_id=%016" PRIx64, s_rec.boot_id);

    if (s_rec.tick_timer != NULL) {
        esp_timer_stop(s_rec.tick_timer);
        esp_timer_start_periodic(s_rec.tick_timer,
                                 (uint64_t)APP_RECOVERY_TICK_MS * 1000ULL);
    }

    s_rec.boot_scan_until_us =
        now_us() + (int64_t)APP_RECOVERY_BOOT_SCAN_MS * 1000LL;
    ESP_LOGI(TAG, "Recovery scanning %u ms for tag advertising before connect",
             (unsigned)APP_RECOVERY_BOOT_SCAN_MS);
    s_rec.last_scan_refresh_us = 0;
    (void)refresh_scan();
    s_rec.last_scan_refresh_us = now_us();
    try_schedule_next();
    maybe_finish_first_round();
    return ESP_OK;
}

void app_recovery_set_inventory_unreadable(void)
{
    s_rec.lifecycle.lifecycle = ESL_AP_LIFECYCLE_FAILED;
    s_rec.lifecycle.retryable = false;
    s_rec.lifecycle.error_code = ESL_AP_ERROR_INVENTORY_UNREADABLE;
    s_rec.healthy = false;
    ESP_LOGE(TAG, "Lifecycle FAILED: INVENTORY_UNREADABLE");
    emit_summary_changed();
}

uint64_t app_recovery_boot_id(void)
{
    return s_rec.boot_id;
}

void app_recovery_get_lifecycle_status(esl_ap_lifecycle_status_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = s_rec.lifecycle;
}

bool app_recovery_is_healthy(void)
{
    recompute_health();
    return s_rec.healthy;
}

bool app_recovery_is_degraded(void)
{
    return !app_recovery_is_healthy();
}

bool app_recovery_tx_busy(void)
{
    return s_rec.tx_phase == REC_TX_CONNECTING ||
           s_rec.tx_phase == REC_TX_WRITE_TIME ||
           s_rec.tx_phase == REC_TX_SYNCHRONIZING ||
           s_rec.tx_phase == REC_TX_PING_SETTLE ||
           s_rec.tx_phase == REC_TX_AWAIT_PING;
}

bool app_recovery_blocks_gateway(void)
{
    if (s_rec.paused) {
        return false;
    }
    return app_recovery_tx_busy() || recovery_has_boot_scan_gateway_blocker();
}

void app_recovery_set_paused(bool paused)
{
    s_rec.paused = paused;
    if (paused) {
        for (uint8_t i = 0; i < ESL_PRODUCT_MAX_TAGS; i++) {
            app_recovery_suspect_clear(&s_rec.suspect[i]);
        }
        /* Drop an in-flight GAP connect so provision/OTS can use the radio. */
        if (s_rec.tx_phase == REC_TX_CONNECTING) {
            ble_esl_ap_abort_tag_connections();
            clear_tx();
        }
    }
    if (!paused) {
        /* Provision / OTS cancelled discovery; resume so reboot ads are seen. */
        if (refresh_scan() == ESP_OK) {
            s_rec.last_scan_refresh_us = now_us();
        }
        try_schedule_next();
    }
}

bool app_recovery_on_scan(const ble_esl_ap_scan_result_t *scan)
{
    if (scan == NULL || !s_rec.started) {
        return false;
    }
    if (s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_RECOVERING &&
            s_rec.lifecycle.lifecycle != ESL_AP_LIFECYCLE_READY) {
        return false;
    }

    esl_product_tag_record_t *rec = app_inventory_find_by_addr(scan->addr);
    if (rec == NULL) {
        return false;
    }
    if (rec->wire->provision_phase != ESL_PRODUCT_PHASE_ACTIVE) {
        return true; /* inventory identity, but first-config path owns it */
    }

    app_recovery_scan_input_t in = {
        .record_valid = (rec->wire->valid == 1),
        .provision_active = true,
        .desired_active = (rec->wire->desired_state == ESL_PRODUCT_DESIRED_ACTIVE),
        .quarantined = is_quarantined(rec->wire, rec->rt),
        .peer_addr_set = peer_addr_is_set(rec->wire->addr),
        .record_generation_valid = app_inventory_runtime_generation_valid(rec->slot),
        .product_state = (uint8_t)rec->rt->product_state,
        .scan_data_complete = scan->data_complete,
        .scan_is_connectable = scan->is_connectable,
        .scan_advertises_esl_service = scan->advertises_esl_service,
    };

    if (scan->is_connectable && rec->slot < ESL_PRODUCT_MAX_TAGS &&
            !s_rec.adv_seen[rec->slot]) {
        s_rec.adv_seen[rec->slot] = true;
        s_rec.acl_probe_after_pawr_timeout[rec->slot] = false;
        if (rec->rt->esl_runtime_state == BLE_ESL_STATE_SYNCHRONIZED) {
            rec->rt->esl_runtime_state = BLE_ESL_STATE_UNSYNCHRONIZED;
            rec->rt->time_synced = false;
            emit_tag_changed(rec->slot);
        }
        ESP_LOGI(TAG, "Scan saw inventory tag slot=%u; next recovery connect is ACL",
                 rec->slot);
    }

    app_recovery_scan_decision_t decision = app_recovery_policy_on_scan(&in);
    if (decision == APP_RECOVERY_SCAN_IGNORE) {
        /* Still consume known inventory identities that are not actionable. */
        return true;
    }

    if (decision == APP_RECOVERY_SCAN_SUSPECT_UNSYNCHRONIZED) {
        /* Synchronized Tags do not advertise. Connectable ADV from an ONLINE
         * inventory identity is a reboot / lost-sync (no ESL UUID). Skip the
         * two verify-Pings (~6s+) and recover over ACL immediately. Do this
         * even if a verify Ping from an earlier ESL-UUID packet is in flight. */
        ESP_LOGI(TAG, "Scan hit ONLINE tag advertising unsync slot=%u — ACL recover",
                 rec->slot);
        mark_recovering_from_advertising(rec->slot);
        if (tag_eligible(rec) && tag_needs_attempt_now(rec) &&
                scan->is_connectable &&
                !app_recovery_tx_busy() && !s_rec.paused) {
            begin_attempt(rec->slot);
        }
        return true;
    }

    if (app_recovery_tx_busy() || s_rec.paused || app_esl_ap_pipeline_busy()) {
        return true;
    }
    if (decision == APP_RECOVERY_SCAN_SUSPECT_UNASSOCIATED) {
        /* ONLINE: multi-confirm via verify Ping. RECOVERING/OFFLINE: no PAwR
         * sync for Ping — count ESL-UUID observations and prefer ACL recovery;
         * quarantine only after repeated observations (TODO: tune thresholds). */
        if (rec->rt->product_state == ESL_PRODUCT_STATE_ONLINE) {
            handle_online_suspect(rec->slot, APP_RECOVERY_SUSPECT_UNASSOCIATED);
        } else {
            uint8_t slot = rec->slot;
            uint32_t token = ++s_rec.next_suspect_token;
            if (token == 0) {
                token = ++s_rec.next_suspect_token;
            }
            (void)app_recovery_suspect_on_adv(&s_rec.suspect[slot],
                                              APP_RECOVERY_SUSPECT_UNASSOCIATED,
                                              now_us(), token);
            /* Do not submit verify Ping (no sync). Prefer ACL attempt. */
            s_rec.suspect[slot].ping_pending = false;
            if (tag_eligible(rec) && tag_needs_attempt_now(rec) &&
                    scan->is_connectable &&
                    !app_recovery_tx_busy() && !s_rec.paused &&
                    !app_esl_ap_pipeline_busy()) {
                begin_attempt(slot);
            } else if (s_rec.suspect[slot].adv_observations >=
                       APP_RECOVERY_CONFIRM_ADV_NEED) {
                mark_quarantine_unassociated(slot);
            }
        }
        return true;
    }

    /* EXISTING_RECOVERY */
    if (!tag_eligible(rec)) {
        return true;
    }
    /* Backoff is for a Tag we cannot see. A connectable adv means it is here. */
    if (scan->is_connectable &&
            (rec->rt->product_state == ESL_PRODUCT_STATE_RETRY_WAIT ||
             rec->rt->product_state == ESL_PRODUCT_STATE_OFFLINE) &&
            rec->rt->retry_deadline_us > now_us()) {
        rec->rt->retry_deadline_us = 0;
        rec->rt->product_state = ESL_PRODUCT_STATE_RECOVERING;
        ESP_LOGI(TAG, "Scan hit during backoff slot=%u; connect now", rec->slot);
    }
    if (!tag_needs_attempt_now(rec)) {
        return true;
    }
    if (!scan->is_connectable) {
        return true;
    }
    if (app_recovery_tx_busy() || s_rec.paused || app_esl_ap_pipeline_busy()) {
        return true;
    }

    ESP_LOGI(TAG, "Scan hit recovery tag slot=%u", rec->slot);
    begin_attempt(rec->slot);
    return true;
}

bool app_recovery_on_connected(const ble_esl_ap_conn_info_t *info)
{
    if (info == NULL || s_rec.tx_phase != REC_TX_CONNECTING) {
        return false;
    }
    if (memcmp(info->addr, s_rec.tx_peer_addr, 6) != 0) {
        return false;
    }

    if (s_rec.connect_timer != NULL) {
        esp_timer_stop(s_rec.connect_timer);
    }

    s_rec.tx_conn_handle = info->conn_handle;
    s_rec.tx_phase = REC_TX_WRITE_TIME;
    arm_connect_timeout();
    maybe_start_gateway();

    esl_product_tag_record_t view;
    if (slot_view(s_rec.tx_slot, &view)) {
        view.rt->conn_handle = info->conn_handle;
        view.rt->esl_runtime_state = BLE_ESL_STATE_UPDATING;
    }

    ESP_LOGI(TAG, "Recovery connected conn=%u slot=%u; write Absolute Time",
             info->conn_handle, s_rec.tx_slot);

    esp_err_t ret = ble_esl_ap_write_absolute_time(info->conn_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "write_absolute_time failed: %s", esp_err_to_name(ret));
        uint8_t slot = s_rec.tx_slot;
        ble_esl_ap_disconnect(info->conn_handle);
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_TIME_SYNC_FAILED);
        maybe_finish_first_round();
        try_schedule_next();
    }
    return true;
}

bool app_recovery_on_disconnected(const ble_esl_ap_disconn_info_t *info)
{
    if (info == NULL) {
        return false;
    }

    if (s_rec.tx_phase == REC_TX_IDLE) {
        return false;
    }
    if (s_rec.tx_conn_handle != BLE_HS_CONN_HANDLE_NONE &&
            info->conn_handle != s_rec.tx_conn_handle) {
        return false;
    }
    if (s_rec.tx_conn_handle == BLE_HS_CONN_HANDLE_NONE &&
            s_rec.tx_phase != REC_TX_SYNCHRONIZING) {
        return false;
    }

    uint8_t slot = s_rec.tx_slot;

    if (s_rec.tx_phase == REC_TX_SYNCHRONIZING) {
        /* PAST is applied on ACL; Tag only follows PAwR after this drop. */
        s_rec.tx_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        if (s_rec.tx_past_ok) {
            ESP_LOGI(TAG, "Recovery ACL dropped after PAST slot=%u; settle then Ping", slot);
            arm_ping_settle(slot);
        } else {
            ESP_LOGD(TAG, "Recovery disconnect during synchronize (wait PAST) slot=%u", slot);
        }
        return true;
    }

    if (s_rec.tx_phase == REC_TX_AWAIT_PING || s_rec.tx_phase == REC_TX_PING_SETTLE) {
        return true;
    }

    ESP_LOGW(TAG, "Recovery disconnect mid-ACL phase=%u slot=%u reason=0x%02x",
             (unsigned)s_rec.tx_phase, slot, info->reason);
    clear_tx();
    mark_retry_wait(slot, ESL_AP_FAIL_UNREACHABLE);
    maybe_finish_first_round();
    try_schedule_next();
    return true;
}

bool app_recovery_on_synchronized(const ble_esl_ap_synchronized_t *info)
{
    if (info == NULL) {
        return false;
    }
    if (s_rec.tx_phase != REC_TX_SYNCHRONIZING &&
            s_rec.tx_phase != REC_TX_PING_SETTLE &&
            s_rec.tx_phase != REC_TX_AWAIT_PING) {
        return false;
    }

    esl_product_tag_record_t view;
    if (!slot_view(s_rec.tx_slot, &view)) {
        return false;
    }
    if (app_inventory_esl_addr(&view) != esl_product_unpack_esl_addr(info->esl_addr)) {
        return false;
    }
    if (!app_inventory_runtime_generation_valid(s_rec.tx_slot) ||
            view.rt->recovery_generation != s_rec.tx_recovery_generation) {
        return false;
    }

    if (s_rec.tx_phase == REC_TX_AWAIT_PING || s_rec.tx_phase == REC_TX_PING_SETTLE) {
        return true;
    }

    if (info->status != ESP_OK) {
        uint8_t slot = s_rec.tx_slot;
        ESP_LOGW(TAG, "Recovery PAST/sync failed slot=%u", slot);
        clear_tx();
        mark_retry_wait(slot, ESL_AP_FAIL_PAST_FAILED);
        maybe_finish_first_round();
        try_schedule_next();
        return true;
    }

    view.rt->esl_runtime_state = BLE_ESL_STATE_SYNCHRONIZED;
    s_rec.tx_past_ok = true;
    /* Do NOT promote product ONLINE here — wait for arbiter Ping after ACL drop.
     * A PAwR Ping while the recovery ACL is still up times out: Tag radio is
     * on the ACL (UPDATING), then product OFFLINE + long backoff hides a
     * SYNCHRONIZED Tag from the phone. */
    if (s_rec.tx_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "Recovery PAST ok after ACL drop slot=%u; settle then Ping",
                 s_rec.tx_slot);
        arm_ping_settle(s_rec.tx_slot);
    } else {
        ESP_LOGI(TAG, "Recovery PAST ok slot=%u; defer Ping until ACL drop", s_rec.tx_slot);
    }
    return true;
}

void app_recovery_on_pawr_response(const esl_ap_response_t *resp)
{
    if (resp == NULL) {
        return;
    }
    uint16_t esl_addr = (uint16_t)(resp->esl_id | ((uint16_t)resp->group_id << 8));

    esl_ap_arbiter_request_t active;
    if (!esl_ap_arbiter_peek_active(resp->group_id, &active)) {
        return;
    }
    if (active.esl_addr != esl_addr) {
        return;
    }

    bool match = false;
    switch (active.expected_response) {
    case ESL_AP_EXPECT_BASIC_STATE:
        match = (resp->type == ESL_AP_RESP_BASIC_STATE);
        break;
    case ESL_AP_EXPECT_ERROR_OR_BASIC_STATE:
        match = (resp->type == ESL_AP_RESP_BASIC_STATE ||
                 resp->type == ESL_AP_RESP_ERROR);
        break;
    case ESL_AP_EXPECT_COMMAND_SPECIFIC:
        match = (resp->type == ESL_AP_RESP_LED_STATE ||
                 resp->type == ESL_AP_RESP_DISPLAY_STATE ||
                 resp->type == ESL_AP_RESP_BASIC_STATE ||
                 resp->type == ESL_AP_RESP_ERROR);
        break;
    default:
        break;
    }
    if (!match) {
        return;
    }

    esp_err_t st = (resp->type == ESL_AP_RESP_ERROR) ? ESP_FAIL : ESP_OK;
    (void)esl_ap_arbiter_complete(resp->group_id, esl_addr, active.command_opcode,
                                  active.request_id, st);
}

esp_err_t app_recovery_submit_health_ping(uint8_t esl_id, uint8_t group_id,
                                          uint8_t record_slot, uint32_t record_generation)
{
    esl_ap_arbiter_request_t req;
    memset(&req, 0, sizeof(req));
    req.request_id = ++s_rec.next_request_id;
    req.boot_id = s_rec.boot_id;
    req.recovery_generation = 0;
    req.record_generation = record_generation;
    req.record_slot = record_slot;
    req.esl_addr = (uint16_t)(esl_id | ((uint16_t)group_id << 8));
    req.group_id = group_id;
    req.command_opcode = BLE_ESL_CMD_PING;
    req.expected_response = ESL_AP_EXPECT_BASIC_STATE;
    req.priority = ESL_AP_ARBITER_PRIO_HEALTH;
    req.guard_timeout_ms = APP_RECOVERY_PING_GUARD_MS;
    req.params_len = 1;
    req.params[0] = esl_id;
    return esl_ap_arbiter_submit(&req);
}
