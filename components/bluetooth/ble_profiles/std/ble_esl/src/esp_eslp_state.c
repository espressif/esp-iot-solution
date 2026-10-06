/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *  @brief Shared ESL Profile lifecycle state machine (Tag + AP)
 */

#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_eslp_state.h"

static const char *TAG = "ble_eslp_sm";

#ifndef CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN
#define CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN 60
#endif

#define ESLP_TIMEOUT_US ((uint64_t)CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN * 60ULL * 1000000ULL)

const char *esp_ble_eslp_state_str(esp_ble_eslp_state_t state)
{
    switch (state) {
    case BLE_ESLP_STATE_UNASSOCIATED:   return "UNASSOCIATED";
    case BLE_ESLP_STATE_CONFIGURING:    return "CONFIGURING";
    case BLE_ESLP_STATE_UPDATING:       return "UPDATING";
    case BLE_ESLP_STATE_SYNCHRONIZED:   return "SYNCHRONIZED";
    case BLE_ESLP_STATE_UNSYNCHRONIZED: return "UNSYNCHRONIZED";
    default:                            return "UNKNOWN";
    }
}

bool esp_ble_eslp_state_is_associated(esp_ble_eslp_state_t state)
{
    return state == BLE_ESLP_STATE_UPDATING ||
           state == BLE_ESLP_STATE_SYNCHRONIZED ||
           state == BLE_ESLP_STATE_UNSYNCHRONIZED;
}

bool esp_ble_eslp_sm_transition_valid(esp_ble_eslp_state_t from, esp_ble_eslp_state_t to)
{
    if (from == to) {
        return true;
    }

    switch (from) {
    case BLE_ESLP_STATE_UNASSOCIATED:
        /* Only to Configuring. */
        return to == BLE_ESLP_STATE_CONFIGURING;
    case BLE_ESLP_STATE_CONFIGURING:
        /* To Unassociated, Unsynchronized, or Synchronized. */
        return to == BLE_ESLP_STATE_UNASSOCIATED ||
               to == BLE_ESLP_STATE_UNSYNCHRONIZED ||
               to == BLE_ESLP_STATE_SYNCHRONIZED;
    case BLE_ESLP_STATE_UPDATING:
        /* To Unsynchronized, Synchronized, or Unassociated. */
        return to == BLE_ESLP_STATE_UNSYNCHRONIZED ||
               to == BLE_ESLP_STATE_SYNCHRONIZED ||
               to == BLE_ESLP_STATE_UNASSOCIATED;
    case BLE_ESLP_STATE_SYNCHRONIZED:
        /* To Unsynchronized, Updating, or Unassociated. */
        return to == BLE_ESLP_STATE_UNSYNCHRONIZED ||
               to == BLE_ESLP_STATE_UPDATING ||
               to == BLE_ESLP_STATE_UNASSOCIATED;
    case BLE_ESLP_STATE_UNSYNCHRONIZED:
        /* To Updating or Unassociated only */
        return to == BLE_ESLP_STATE_UPDATING ||
               to == BLE_ESLP_STATE_UNASSOCIATED;
    default:
        return false;
    }
}

static void eslp_sm_stop_timer(esp_timer_handle_t timer)
{
    if (timer) {
        (void)esp_timer_stop(timer);
    }
}

static esp_err_t eslp_sm_arm_timer(esp_timer_handle_t timer)
{
    if (!timer) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)esp_timer_stop(timer);
    return esp_timer_start_once(timer, ESLP_TIMEOUT_US);
}

static void eslp_sm_sync_timeout_cb(void *arg)
{
    esp_ble_eslp_sm_t *sm = arg;
    if (!sm || !sm->inited) {
        return;
    }
    ESP_LOGW(TAG, "Synchronized timeout (%d min)", CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN);
    if (sm->on_timeout) {
        sm->on_timeout(BLE_ESLP_TIMEOUT_SYNC, sm->ctx);
    }
}

static void eslp_sm_unsync_timeout_cb(void *arg)
{
    esp_ble_eslp_sm_t *sm = arg;
    if (!sm || !sm->inited) {
        return;
    }
    ESP_LOGW(TAG, "Unsynchronized timeout (%d min)", CONFIG_BLE_ESL_PROFILE_TIMEOUT_MIN);
    if (sm->on_timeout) {
        sm->on_timeout(BLE_ESLP_TIMEOUT_UNSYNC, sm->ctx);
    }
}

static void eslp_sm_apply_timers(esp_ble_eslp_sm_t *sm)
{
    switch (sm->state) {
    case BLE_ESLP_STATE_SYNCHRONIZED:
        eslp_sm_stop_timer(sm->unsync_timer);
        (void)eslp_sm_arm_timer(sm->sync_timer);
        break;
    case BLE_ESLP_STATE_UNSYNCHRONIZED:
        eslp_sm_stop_timer(sm->sync_timer);
        (void)eslp_sm_arm_timer(sm->unsync_timer);
        break;
    default:
        eslp_sm_stop_timer(sm->sync_timer);
        eslp_sm_stop_timer(sm->unsync_timer);
        break;
    }
}

esp_err_t esp_ble_eslp_sm_init(esp_ble_eslp_sm_t *sm,
                               esp_ble_eslp_sm_changed_cb_t on_changed,
                               esp_ble_eslp_sm_timeout_cb_t on_timeout,
                               void *ctx)
{
    if (!sm) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sm->inited) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(sm, 0, sizeof(*sm));
    sm->state = BLE_ESLP_STATE_UNASSOCIATED;
    sm->on_changed = on_changed;
    sm->on_timeout = on_timeout;
    sm->ctx = ctx;

    esp_timer_create_args_t sync_args = {
        .callback = eslp_sm_sync_timeout_cb,
        .arg = sm,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eslp_sync",
    };
    esp_err_t ret = esp_timer_create(&sync_args, &sm->sync_timer);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_timer_create_args_t unsync_args = {
        .callback = eslp_sm_unsync_timeout_cb,
        .arg = sm,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eslp_unsync",
    };
    ret = esp_timer_create(&unsync_args, &sm->unsync_timer);
    if (ret != ESP_OK) {
        (void)esp_timer_delete(sm->sync_timer);
        sm->sync_timer = NULL;
        return ret;
    }

    sm->inited = true;
    return ESP_OK;
}

esp_err_t esp_ble_eslp_sm_deinit(esp_ble_eslp_sm_t *sm)
{
    if (!sm || !sm->inited) {
        return ESP_ERR_INVALID_STATE;
    }

    eslp_sm_stop_timer(sm->sync_timer);
    eslp_sm_stop_timer(sm->unsync_timer);
    if (sm->sync_timer) {
        (void)esp_timer_delete(sm->sync_timer);
    }
    if (sm->unsync_timer) {
        (void)esp_timer_delete(sm->unsync_timer);
    }
    memset(sm, 0, sizeof(*sm));
    return ESP_OK;
}

esp_ble_eslp_state_t esp_ble_eslp_sm_get_state(const esp_ble_eslp_sm_t *sm)
{
    if (!sm || !sm->inited) {
        return BLE_ESLP_STATE_UNASSOCIATED;
    }
    return sm->state;
}

esp_err_t esp_ble_eslp_sm_set_state(esp_ble_eslp_sm_t *sm, esp_ble_eslp_state_t new_state)
{
    if (!sm || !sm->inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (sm->state == new_state) {
        eslp_sm_apply_timers(sm);
        return ESP_OK;
    }

    if (!esp_ble_eslp_sm_transition_valid(sm->state, new_state)) {
        ESP_LOGW(TAG, "invalid transition %s -> %s",
                 esp_ble_eslp_state_str(sm->state), esp_ble_eslp_state_str(new_state));
        return ESP_ERR_INVALID_STATE;
    }

    esp_ble_eslp_state_changed_t changed = {
        .prev_state = sm->state,
        .new_state = new_state,
    };
    sm->state = new_state;
    ESP_LOGI(TAG, "state %s -> %s",
             esp_ble_eslp_state_str(changed.prev_state),
             esp_ble_eslp_state_str(changed.new_state));

    eslp_sm_apply_timers(sm);

    if (sm->on_changed) {
        sm->on_changed(&changed, sm->ctx);
    }
    return ESP_OK;
}

esp_err_t esp_ble_eslp_sm_note_sync_activity(esp_ble_eslp_sm_t *sm)
{
    if (!sm || !sm->inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sm->state != BLE_ESLP_STATE_SYNCHRONIZED) {
        return ESP_OK;
    }
    return eslp_sm_arm_timer(sm->sync_timer);
}
