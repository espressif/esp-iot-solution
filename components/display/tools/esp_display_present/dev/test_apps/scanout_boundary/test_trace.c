/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "main/boundary_trace.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
static void *producer(void *count)
{
    for (uintptr_t i = 0; i < (uintptr_t)count; ++i) {
        boundary_emit(BT_BOUNCE_EOF, 0, i);
    }
    return NULL;
}
static void run(unsigned per_thread, unsigned expected, unsigned dropped)
{
    boundary_reset();
    pthread_t threads[3];
    for (unsigned i = 0; i < 3; ++i) {
        assert(pthread_create(&threads[i], NULL, producer, (void *)(uintptr_t)per_thread) == 0);
    }
    for (unsigned i = 0; i < 3; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
    }
    boundary_freeze();
    boundary_emit(BT_UNDERRUN, 0, 0); /* Frozen producers cannot alter evidence. */
    size_t count;
    uint32_t lost;
    const boundary_record_t *records = boundary_records(&count, &lost);
    assert(count == expected && lost == dropped);
    for (size_t i = 0; i < count; ++i) {
        assert(records[i].sequence == i && records[i].kind == BT_BOUNCE_EOF);
        assert(i == 0 || records[i].time_us >= records[i - 1].time_us);
    }
}
int main(void)
{
    run(128, 384, 0);
    run(4000, BOUNDARY_TRACE_CAPACITY, 12000 - BOUNDARY_TRACE_CAPACITY);
    run(1, 3, 0);
    puts("trace: concurrent producers, ordering, overflow, freeze and reset passed");
}
