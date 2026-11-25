/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <zlib.h>
#include "eaf_port.h"

void *eaf_alloc(size_t size)
{
    return malloc(size);
}
void *eaf_port_alloc_internal(size_t size)
{
    return malloc(size);
}
void *eaf_alloc_aligned(size_t size, size_t alignment, size_t *allocated_size)
{
    if (allocated_size) {
        *allocated_size = 0;
    }
    if (!alignment) {
        alignment = 64;
    }
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }
    if ((alignment & (alignment - 1)) || size > SIZE_MAX - alignment + 1) {
        return NULL;
    }
    size_t rounded = (size + alignment - 1) & ~(alignment - 1);
    void *p = NULL;
    if (posix_memalign(&p, alignment, rounded) != 0) {
        return NULL;
    }
    if (allocated_size) {
        *allocated_size = rounded;
    }
    return p;
}
int64_t eaf_port_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
size_t eaf_port_inflate_raw(void *dst, size_t n, const void *src, size_t len)
{
    if (n > UINT_MAX || len > UINT_MAX) {
        return SIZE_MAX;
    }
    z_stream z = {0};
    if (inflateInit2(&z, -15) != Z_OK) {
        return SIZE_MAX;
    }
    z.next_in = (Bytef *)src; z.avail_in = (uInt)len;
    z.next_out = dst; z.avail_out = (uInt)n;
    int status = inflate(&z, Z_FINISH);
    bool valid = status == Z_STREAM_END && z.total_out == n && z.total_in == len;
    inflateEnd(&z);
    return valid ? n : SIZE_MAX;
}
void eaf_port_sync(void *data, size_t size)
{
    (void)data;
    (void)size;
}

void eaf_port_release_buffer(void *data, size_t size)
{
    (void)size;
    free(data);
}

void eaf_port_call_once(eaf_once_t *once, void (*initialize)(void))
{
    if (atomic_load_explicit(&once->done, memory_order_acquire)) {
        return;
    }
    pthread_mutex_lock(&once->lock);
    if (!atomic_load_explicit(&once->done, memory_order_relaxed)) {
        initialize();
        atomic_store_explicit(&once->done, true, memory_order_release);
    }
    pthread_mutex_unlock(&once->lock);
}
