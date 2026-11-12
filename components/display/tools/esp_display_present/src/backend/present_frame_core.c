/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_types.h"
#include "backend/present_frame_core.h"
#include <assert.h>

esp_err_t present_frame_core_init(present_frame_core_t *core, size_t count)
{
    if (!core || !count || count > PRESENT_MAX_FRAMES) {
        return ESP_ERR_INVALID_ARG;
    }
    *core = (present_frame_core_t) {
        .count = count,
        .active = SIZE_MAX
    };
    core->frames = present_alloc(count * sizeof(*core->frames));
    return core->frames ? ESP_OK : ESP_ERR_NO_MEM;
}
void present_frame_core_deinit(present_frame_core_t *core)
{
    present_free(core->frames);
    core->frames = NULL;
}
present_core_frame_t *present_frame_core_find(present_frame_core_t *core, uint64_t token)
{
    return present_record_find(core->frames, core->count, sizeof(*core->frames), token, false);
}
present_core_frame_t *present_frame_core_find_receipt(present_frame_core_t *core, esp_display_present_receipt_t receipt)
{
    return present_record_find(core->frames, core->count, sizeof(*core->frames), receipt.value, true);
}
esp_err_t present_frame_core_admit(present_frame_core_t *core, present_lock_t *lock,
                                   uint32_t width, uint32_t height,
                                   const esp_display_present_frame_request_t *request, esp_display_present_frame_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->value = 0;
    if (!core || !lock || !request) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = present_validate_damage(width, height, request->damage, request->damage_count);
    if (err != ESP_OK) {
        return err;
    }
    present_lock_enter(lock, false);
    bool ok = core->life == PRESENT_CORE_RUNNING && core->fault.result == ESP_OK;
    present_lock_exit(lock, false);
    return ok ? ESP_OK : ESP_ERR_INVALID_STATE;
}
esp_err_t present_frame_core_begin(present_frame_core_t *core, esp_display_present_frame_t *out)
{
    out->value = 0;
    if (core->life != PRESENT_CORE_RUNNING || core->fault.result != ESP_OK || core->active != SIZE_MAX) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < core->count; ++i) {
        if (core->frames[i].common.token) {
            continue;
        }
        uint64_t token = present_new_token();
        if (!token) {
            break;
        }
        core->frames[i] = (present_core_frame_t) {
            .common = {.token = token}
        };
        core->active = i;
        out->value = token;
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}
void PRESENT_ISR_ATTR present_frame_core_reap(present_frame_core_t *core, present_core_frame_t *record)
{
    if (!record->common.token || !record->sealed || record->pending || record->leased || record->common.external) {
        return;
    }
    if (core->active == (size_t)(record - core->frames)) {
        core->active = SIZE_MAX;
    }
    if (record->canceled) {
        present_cancel_publish(&core->canceled, record->common.token);
    }
    record->common.token = 0;
}
void PRESENT_ISR_ATTR present_frame_core_complete(present_frame_core_t *core, present_core_frame_t *record)
{
    assert(record->pending);
    --record->pending;
    present_frame_core_reap(core, record);
}
void PRESENT_ISR_ATTR present_frame_core_fail(present_frame_core_t *core, present_core_frame_t *record,
                                              uint64_t cookie, esp_err_t error)
{
    present_record_fail(&record->common, error);
    present_fault_first(&core->fault, record->common.token, cookie, error);
}
esp_err_t present_frame_core_seal(present_frame_core_t *core, present_core_frame_t *record, esp_display_present_receipt_t *out)
{
    if (out) {
        out->value = 0;
    }
    if (core->life != PRESENT_CORE_RUNNING || !record || record->sealed || record->leased) {
        return ESP_ERR_INVALID_STATE;
    }
    record->sealed = true;
    core->active = SIZE_MAX;
    if (out) {
        record->common.external = true;
        out->value = record->common.token;
    }
    present_frame_core_reap(core, record);
    return ESP_OK;
}
esp_err_t present_frame_core_release(present_core_frame_t *record)
{
    if (!record || !record->common.external || !record->sealed || record->pending || record->leased) {
        return ESP_ERR_INVALID_STATE;
    }
    record->common.token = 0;
    return ESP_OK;
}
esp_err_t present_frame_core_query(present_frame_core_t *core, present_lock_t *lock, esp_display_present_receipt_t receipt,
                                   bool supports_present, esp_display_present_frame_status_t *out)
{
    if (!core || !lock || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(lock, false);
    present_core_frame_t *record = present_frame_core_find_receipt(core, receipt);
    if (!record) {
        present_lock_exit(lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    bool done = record->common.terminal || (record->sealed && record->pending == 0);
    /* Presentation capability is not evidence that asynchronous source access
     * ended. Keep the existing conservative completion point: finish the frame
     * before reporting source release; retained scanout still requires a lease. */
    bool released = done && record->pending == 0 && record->leased == 0;
    *out = present_record_status(&record->common, record->sealed || record->common.terminal, released, done,
                                 supports_present && done && record->common.result == ESP_OK);
    present_lock_exit(lock, false);
    return ESP_OK;
}
esp_err_t present_frame_core_get_fault(present_frame_core_t *core, present_lock_t *lock, esp_display_present_backend_fault_t *out)
{
    if (!core || !lock || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(lock, false);
    *out = core->fault;
    present_lock_exit(lock, false);
    return ESP_OK;
}
esp_err_t present_frame_core_wait(present_backend_t *backend,
                                  esp_err_t (*query)(present_backend_t *, esp_display_present_receipt_t, esp_display_present_frame_status_t *),
                                  esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point,
                                  bool supports_present, uint32_t timeout, esp_display_present_frame_status_t *out)
{
    if (!backend || !query || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (point == ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED && !supports_present) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (point != ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED && point != ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    for (;;) {
        esp_err_t err = query(backend, receipt, out);
        if (err != ESP_OK || out->terminal) {
            return err;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}
esp_err_t present_frame_core_release_receipt(present_frame_core_t *core, present_lock_t *lock, esp_display_present_receipt_t receipt)
{
    if (!core || !lock) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(lock, false);
    present_core_frame_t *record = present_frame_core_find_receipt(core, receipt);
    esp_err_t err = present_frame_core_release(record);
    present_lock_exit(lock, false);
    return err;
}
esp_err_t present_frame_core_can_destroy(const present_frame_core_t *core)
{
    if (!core) {
        return ESP_ERR_INVALID_ARG;
    }
    if (core->life != PRESENT_CORE_QUIESCENT) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < core->count; ++i) {
        if (core->frames[i].common.token) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    return ESP_OK;
}
esp_err_t present_frame_core_quiescent(present_frame_core_t *core)
{
    if (core->life == PRESENT_CORE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < core->count; ++i)
        if (core->frames[i].common.token && (core->frames[i].pending || core->frames[i].leased)) {
            return ESP_ERR_TIMEOUT;
        }
    for (size_t i = 0; i < core->count; ++i) {
        present_frame_core_reap(core, &core->frames[i]);
    }
    core->active = SIZE_MAX;
    core->life = PRESENT_CORE_QUIESCENT;
    return ESP_OK;
}

void present_frame_core_terminal(present_frame_core_t *core, present_core_frame_t *record)
{
    record->common.terminal = true;
    record->sealed = true;
    record->pending = 0;
    present_frame_core_reap(core, record);
}
