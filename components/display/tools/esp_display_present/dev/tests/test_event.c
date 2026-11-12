/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../src/platform/present_os.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    present_os_event_t *ready;
    present_os_event_t *go;
    unsigned count;
} handshake_t;
static void *producer(void *context)
{
    handshake_t *h = context;
    for (unsigned i = 0; i < h->count; i++) {
        assert(present_event_wait(h->go, 1000));
        (void)present_event_signal(h->ready, true);
    }
    return NULL;
}
int main(void)
{
    handshake_t h = {.ready = present_event_create(), .go = present_event_create(), .count = 1000};
    assert(h.ready && h.go);
    assert(!present_event_wait(h.ready, 0));
    (void)present_event_signal(h.ready, false);
    (void)present_event_signal(h.ready, false);
    assert(present_event_wait(h.ready, 0)); /* Signal before wait, coalesced. */
    assert(!present_event_wait(h.ready, 0));
    uint64_t start = present_now_ms();
    assert(!present_event_wait(h.ready, 5));
    assert(present_now_ms() - start >= 4 && present_now_ms() - start < 500);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, producer, &h) == 0);
    for (unsigned i = 0; i < h.count; i++) {
        (void)present_event_signal(h.go, false);
        assert(present_event_wait(h.ready, 1000));
    }
    assert(pthread_join(thread, NULL) == 0);
    present_event_destroy(h.ready);
    present_event_destroy(h.go);
    puts("Latched events: pre-signal, coalescing, deadline and 1000 handshakes passed");
    return 0;
}
