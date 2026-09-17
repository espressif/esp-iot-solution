/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Pure-C recovery scan / ONLINE advertising / Ping-retry policy (host-testable).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RECOVERY_SCAN_IGNORE = 0,
    APP_RECOVERY_SCAN_EXISTING_RECOVERY,
    APP_RECOVERY_SCAN_SUSPECT_UNSYNCHRONIZED,
    APP_RECOVERY_SCAN_SUSPECT_UNASSOCIATED,
} app_recovery_scan_decision_t;

typedef enum {
    APP_RECOVERY_SUSPECT_NONE = 0,
    APP_RECOVERY_SUSPECT_UNSYNCHRONIZED,
    APP_RECOVERY_SUSPECT_UNASSOCIATED,
} app_recovery_suspect_kind_t;

typedef enum {
    APP_RECOVERY_CONFIRM_IGNORE = 0,
    APP_RECOVERY_CONFIRM_CLEAR_SUSPECT,
    APP_RECOVERY_CONFIRM_COUNT_MISS,
    APP_RECOVERY_CONFIRM_CONFIRMED,
} app_recovery_confirm_decision_t;

/** Snapshot of inventory + scan flags for pure policy evaluation. */
typedef struct {
    bool record_valid;
    bool provision_active;
    bool desired_active;
    bool quarantined;
    bool peer_addr_set;
    bool record_generation_valid;
    uint8_t product_state; /* esl_product_state_t numeric */
    bool scan_data_complete;
    bool scan_is_connectable;
    bool scan_advertises_esl_service;
} app_recovery_scan_input_t;

/**
 * Evaluate a scan hit against an inventory identity.
 *
 * product_state values used:
 *   1=RECOVERING, 2=ONLINE, 3=RETRY_WAIT, 4=OFFLINE, 5=QUARANTINED
 */
static inline app_recovery_scan_decision_t
app_recovery_policy_on_scan(const app_recovery_scan_input_t *in)
{
    if (in == NULL || !in->record_valid) {
        return APP_RECOVERY_SCAN_IGNORE;
    }
    if (!in->provision_active || !in->desired_active || in->quarantined ||
            !in->peer_addr_set || !in->record_generation_valid) {
        return APP_RECOVERY_SCAN_IGNORE;
    }
    if (!in->scan_data_complete || !in->scan_is_connectable) {
        return APP_RECOVERY_SCAN_IGNORE;
    }

    /* Existing recovery-eligible states: RECOVERING / RETRY_WAIT / OFFLINE */
    if (in->product_state == 1 || in->product_state == 3 || in->product_state == 4) {
        /* Unassociated advertising must not enter Absolute-Time/PAST fast path. */
        if (in->scan_advertises_esl_service) {
            return APP_RECOVERY_SCAN_SUSPECT_UNASSOCIATED;
        }
        return APP_RECOVERY_SCAN_EXISTING_RECOVERY;
    }

    /* ONLINE only: require cross-check before product-state change. */
    if (in->product_state == 2) {
        if (in->scan_advertises_esl_service) {
            return APP_RECOVERY_SCAN_SUSPECT_UNASSOCIATED;
        }
        return APP_RECOVERY_SCAN_SUSPECT_UNSYNCHRONIZED;
    }

    return APP_RECOVERY_SCAN_IGNORE;
}

typedef struct {
    bool advertising_suspect;
    app_recovery_suspect_kind_t kind;
    uint8_t adv_observations;
    bool ping_pending;
    uint8_t confirm_misses;
    uint32_t suspect_token;
    int64_t suspect_deadline_us;
} app_recovery_suspect_state_t;

#define APP_RECOVERY_CONFIRM_WINDOW_US   (30000000LL) /* 30 s */
#define APP_RECOVERY_CONFIRM_MISS_NEED   2
#define APP_RECOVERY_CONFIRM_ADV_NEED    2

/* After PAST + ACL drop: wait one PAwR interval, then up to 3 Ping attempts. */
#define APP_RECOVERY_PING_SETTLE_MS      400
#define APP_RECOVERY_PING_MAX_TRIES      3

typedef enum {
    APP_RECOVERY_PING_ACT_ONLINE = 0,
    APP_RECOVERY_PING_ACT_RETRY,
    APP_RECOVERY_PING_ACT_GIVE_UP,
} app_recovery_ping_action_t;

/**
 * Classify a recovery Ping completion.
 * @param success    true if Basic State / expected response arrived
 * @param tries_done number of Pings already submitted this attempt (1 after first)
 */
static inline app_recovery_ping_action_t
app_recovery_policy_on_ping_result(bool success, uint8_t tries_done)
{
    if (success) {
        return APP_RECOVERY_PING_ACT_ONLINE;
    }
    if (tries_done < APP_RECOVERY_PING_MAX_TRIES) {
        return APP_RECOVERY_PING_ACT_RETRY;
    }
    return APP_RECOVERY_PING_ACT_GIVE_UP;
}

static inline void app_recovery_suspect_clear(app_recovery_suspect_state_t *s)
{
    if (s == NULL) {
        return;
    }
    s->advertising_suspect = false;
    s->kind = APP_RECOVERY_SUSPECT_NONE;
    s->adv_observations = 0;
    s->ping_pending = false;
    s->confirm_misses = 0;
    s->suspect_token = 0;
    s->suspect_deadline_us = 0;
}

/**
 * Observe a new suspect advertising hit. Returns true if a verify Ping should
 * be submitted now (first observation or after a completed miss cycle).
 */
static inline bool
app_recovery_suspect_on_adv(app_recovery_suspect_state_t *s,
                            app_recovery_suspect_kind_t kind,
                            int64_t now_us,
                            uint32_t next_token)
{
    if (s == NULL) {
        return false;
    }
    if (s->advertising_suspect && s->kind != kind) {
        app_recovery_suspect_clear(s);
    }
    if (s->advertising_suspect && s->suspect_deadline_us > 0 &&
            now_us >= s->suspect_deadline_us) {
        app_recovery_suspect_clear(s);
    }

    if (!s->advertising_suspect) {
        s->advertising_suspect = true;
        s->kind = kind;
        s->adv_observations = 1;
        s->ping_pending = false;
        s->confirm_misses = 0;
        s->suspect_token = next_token == 0 ? 1u : next_token;
        s->suspect_deadline_us = now_us + APP_RECOVERY_CONFIRM_WINDOW_US;
        return true; /* submit first verify ping */
    }

    if (s->adv_observations < 255) {
        s->adv_observations++;
    }
    /* Do not re-submit while a ping is outstanding. */
    if (s->ping_pending) {
        return false;
    }
    /* After a counted miss, allow another verify ping within the window. */
    return s->confirm_misses < APP_RECOVERY_CONFIRM_MISS_NEED;
}

/**
 * Classify a verify-ping completion.
 * @param radio_timeout true only when the ping was successfully sent but no
 *        valid Response-Key/EAD response arrived (not queue busy/cancel).
 */
static inline app_recovery_confirm_decision_t
app_recovery_suspect_on_ping_result(app_recovery_suspect_state_t *s,
                                    uint32_t token,
                                    bool still_online,
                                    bool radio_timeout,
                                    bool success,
                                    int64_t now_us)
{
    if (s == NULL || !s->advertising_suspect) {
        return APP_RECOVERY_CONFIRM_IGNORE;
    }
    if (token != s->suspect_token || !still_online) {
        return APP_RECOVERY_CONFIRM_IGNORE;
    }
    if (s->suspect_deadline_us > 0 && now_us >= s->suspect_deadline_us) {
        app_recovery_suspect_clear(s);
        return APP_RECOVERY_CONFIRM_CLEAR_SUSPECT;
    }

    s->ping_pending = false;

    if (success) {
        app_recovery_suspect_clear(s);
        return APP_RECOVERY_CONFIRM_CLEAR_SUSPECT;
    }
    if (!radio_timeout) {
        /* queue busy / cancel / internal — do not count as radio miss */
        return APP_RECOVERY_CONFIRM_IGNORE;
    }

    if (s->confirm_misses < 255) {
        s->confirm_misses++;
    }
    if (s->adv_observations >= APP_RECOVERY_CONFIRM_ADV_NEED &&
            s->confirm_misses >= APP_RECOVERY_CONFIRM_MISS_NEED) {
        return APP_RECOVERY_CONFIRM_CONFIRMED;
    }
    return APP_RECOVERY_CONFIRM_COUNT_MISS;
}

#ifdef __cplusplus
}
#endif
