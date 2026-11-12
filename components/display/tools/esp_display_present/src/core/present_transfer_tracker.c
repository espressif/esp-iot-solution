/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_transfer_tracker.h"
#include "resources/present_memory.h"
#include <limits.h>

esp_err_t present_transfer_init(present_transfer_tracker_t *tracker, uint32_t timeout_ms)
{
    tracker->timeout_us = (uint64_t)timeout_ms * 1000;
    tracker->entries = tracker->timeout_us ? present_alloc(PRESENT_MAX_FRAMES * sizeof(*tracker->entries)) : NULL;
    return tracker->timeout_us && !tracker->entries ? ESP_ERR_NO_MEM : ESP_OK;
}
void present_transfer_deinit(present_transfer_tracker_t *tracker)
{
    present_free(tracker->entries);
}
size_t present_transfer_memory(uint32_t timeout_ms)
{
    return timeout_ms ? PRESENT_MAX_FRAMES * sizeof(present_transfer_entry_t) : 0;
}
size_t present_transfer_begin(present_transfer_tracker_t *tracker, present_lock_t *control, uint64_t cookie)
{
    size_t index = PRESENT_MAX_FRAMES;
    present_lock_enter(control, false);
    for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
        if (!tracker->entries[i].cookie) {
            index = i;
            tracker->entries[i] = (present_transfer_entry_t) {
                .cookie = cookie,
                .started_us = present_now_us()
            };
            break;
        }
    }
    present_lock_exit(control, false);
    return index;
}
void present_transfer_rejected(present_transfer_tracker_t *tracker, present_lock_t *control, size_t index, uint64_t cookie)
{
    present_lock_enter(control, false);
    /* An early completion may already have consumed this timing record. */
    if (tracker->entries[index].cookie == cookie) {
        tracker->entries[index].cookie = 0;
    }
    present_lock_exit(control, false);
}
void present_transfer_quiesced(present_transfer_tracker_t *tracker, present_lock_t *control)
{
    if (tracker->entries) {
        present_lock_enter(control, false);
        for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
            tracker->entries[i].cookie = 0;
        }
        present_lock_exit(control, false);
    }
}

bool PRESENT_ISR_ATTR present_transfer_finish(present_transfer_tracker_t *tracker, present_lock_t *control, const esp_display_present_backend_event_t *event, bool isr, uint64_t *timestamp_us)
{
    if (!tracker->timeout_us ||
            (event->kind != ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE && event->kind != ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED)) {
        return false;
    }
    uint64_t now = present_now_us();
    bool expired = false;
    present_lock_enter(control, isr);
    for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
        present_transfer_entry_t *watch = &tracker->entries[i];
        if (watch->cookie && watch->cookie == event->transaction_cookie) {
            expired = now - watch->started_us >= tracker->timeout_us;
            watch->cookie = 0;
            break;
        }
    }
    present_lock_exit(control, isr);
    if (!expired) {
        return false;
    }
    *timestamp_us = now;
    return true;
}

size_t present_transfer_expired(present_transfer_tracker_t *tracker, present_lock_t *control, uint64_t now, uint64_t *expired)
{
    size_t count = 0;
    present_lock_enter(control, false);
    for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
        present_transfer_entry_t *watch = &tracker->entries[i];
        if (watch->cookie && !watch->reported && now - watch->started_us >= tracker->timeout_us) {
            watch->reported = true;
            expired[count++] = watch->cookie;
        }
    }
    present_lock_exit(control, false);
    return count;
}

uint32_t present_transfer_wait_ms(present_transfer_tracker_t *tracker, present_lock_t *control)
{
    if (!tracker->timeout_us) {
        return UINT32_MAX;
    }
    uint64_t remaining_us = UINT64_MAX, now = present_now_us();
    present_lock_enter(control, false);
    for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
        present_transfer_entry_t *watch = &tracker->entries[i];
        if (!watch->cookie || watch->reported) {
            continue;
        }
        /* Sender computes its next wait outside gate; a producer may have
         * inserted a newer stamp after this clock snapshot. */
        uint64_t elapsed = now >= watch->started_us ? now - watch->started_us : 0;
        uint64_t left = elapsed >= tracker->timeout_us ? 0 : tracker->timeout_us - elapsed;
        if (left < remaining_us) {
            remaining_us = left;
        }
    }
    present_lock_exit(control, false);
    if (remaining_us == UINT64_MAX) {
        return UINT32_MAX;
    }
    uint64_t ms = (remaining_us + 999) / 1000;
    return ms >= UINT32_MAX ? UINT32_MAX - 1 : (uint32_t)ms;
}
