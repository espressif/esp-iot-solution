/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boundary_trace.h"
#include <stdio.h>
#include <inttypes.h>
#ifdef BOUNDARY_HOST
#include <pthread.h>
#include <time.h>
static pthread_mutex_t trace_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&trace_lock)
#define UNLOCK() pthread_mutex_unlock(&trace_lock)
static uint64_t trace_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
static DRAM_ATTR portMUX_TYPE trace_lock = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() portENTER_CRITICAL_SAFE(&trace_lock)
#define UNLOCK() portEXIT_CRITICAL_SAFE(&trace_lock)
#define trace_time() ((uint64_t)esp_timer_get_time())
#endif
#define TRACE_CAPACITY BOUNDARY_TRACE_CAPACITY
#ifdef BOUNDARY_HOST
static boundary_record_t records[TRACE_CAPACITY];
#else
static DRAM_ATTR boundary_record_t records[TRACE_CAPACITY];
#endif
static size_t record_count;
static uint32_t dropped_count;
static bool enabled;

void boundary_reset(void)
{
    LOCK();
    record_count = 0;
    dropped_count = 0;
    enabled = true;
    UNLOCK();
}

void boundary_emit(unsigned kind, uintptr_t buffer, uint32_t detail)
{
    LOCK();
    if (enabled) {
        if (record_count == TRACE_CAPACITY) {
            ++dropped_count; /* Never overwrite evidence silently. */
        } else {
            boundary_record_t *record = &records[record_count];
            *record = (boundary_record_t) {
                .time_us = trace_time(), .buffer = buffer,
                .sequence = (uint32_t)record_count, .detail = detail, .kind = kind,
#ifndef BOUNDARY_HOST
                .isr = xPortInIsrContext(),
#endif
            };
            ++record_count;
        }
    }
    UNLOCK();
}

void boundary_freeze(void)
{
    LOCK();
    enabled = false;
    UNLOCK();
}

const boundary_record_t *boundary_records(size_t *count, uint32_t *dropped)
{
    /* Freeze has joined any emitter holding the lock. Only frozen reads allowed. */
    *count = record_count;
    *dropped = dropped_count;
    return records;
}

void boundary_dump(const char *name, const char *scope)
{
    size_t count;
    uint32_t dropped;
    const boundary_record_t *data = boundary_records(&count, &dropped);
    printf("BOUNDARY_BEGIN,%s,%s,%u,%u\n", name, scope, (unsigned)count, (unsigned)dropped);
    for (size_t i = 0; i < count; ++i) {
        const boundary_record_t *record = &data[i];
        printf("BT,%u,%" PRIu64 ",%u,0x%" PRIxPTR ",%u,%u\n",
               (unsigned)record->sequence, record->time_us, record->kind,
               record->buffer, (unsigned)record->detail, record->isr);
#ifndef BOUNDARY_HOST
        if ((i & 7u) == 7u) {
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(2));
        }
#endif
    }
    printf("BOUNDARY_END,%s\n", name);
}
