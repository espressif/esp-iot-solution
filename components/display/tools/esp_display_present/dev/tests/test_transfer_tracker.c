/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#include "core/present_transfer_tracker.h"
#include <assert.h>
#include <limits.h>

static uint64_t now_us;
uint64_t __wrap_present_now_us(void)
{
    return now_us;
}
static bool finish(present_transfer_tracker_t *tracker, present_lock_t *lock, uint64_t cookie)
{
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, .transaction_cookie = cookie};
    uint64_t timestamp = 0;
    bool expired = present_transfer_finish(tracker, lock, &event, true, &timestamp);
    if (expired) {
        assert(timestamp == now_us);
    }
    return expired;
}
int main(void)
{
    present_lock_t lock;
    assert(present_lock_init(&lock));
    present_transfer_tracker_t tracker = {0};
    assert(present_transfer_init(&tracker, 0) == ESP_OK);
    assert(!tracker.entries && present_transfer_wait_ms(&tracker, &lock) == UINT32_MAX);
    assert(!finish(&tracker, &lock, 1));
    present_transfer_deinit(&tracker);
    assert(present_transfer_init(&tracker, 5) == ESP_OK);
    now_us = 10000;
    size_t first = present_transfer_begin(&tracker, &lock, 11);
    assert(first < PRESENT_MAX_FRAMES);
    now_us = 14999;
    assert(present_transfer_wait_ms(&tracker, &lock) == 1);
    assert(!finish(&tracker, &lock, 99)); /* An unrelated completion cannot clear it. */
    now_us = 15000;
    uint64_t cookies[PRESENT_MAX_FRAMES];
    assert(present_transfer_expired(&tracker, &lock, now_us, cookies) == 1 && cookies[0] == 11);
    assert(present_transfer_expired(&tracker, &lock, now_us, cookies) == 0);
    assert(finish(&tracker, &lock, 11)); /* Prior polling must not turn late DONE into success. */
    assert(!finish(&tracker, &lock, 11));
    size_t reused = present_transfer_begin(&tracker, &lock, 12);
    assert(reused == first);
    present_transfer_rejected(&tracker, &lock, first, 11);
    assert(present_transfer_wait_ms(&tracker, &lock) == 5); /* Old cookie cannot clear a reused entry. */
    now_us = 14999; /* Sender clock snapshot may predate producer insertion. */
    assert(present_transfer_wait_ms(&tracker, &lock) == 5);
    now_us = 15001;
    assert(!finish(&tracker, &lock, 12)); /* In-time completion really releases the timing entry. */
    assert(present_transfer_wait_ms(&tracker, &lock) == UINT32_MAX);
    size_t index = present_transfer_begin(&tracker, &lock, 13);
    present_transfer_rejected(&tracker, &lock, index, 13);
    assert(present_transfer_wait_ms(&tracker, &lock) == UINT32_MAX);
    assert(present_transfer_begin(&tracker, &lock, 14) < PRESENT_MAX_FRAMES);
    present_transfer_quiesced(&tracker, &lock);
    assert(present_transfer_wait_ms(&tracker, &lock) == UINT32_MAX);
    present_transfer_deinit(&tracker);
    present_lock_destroy(&lock);
    return 0;
}
