/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_eaf_status.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_check.h"
#else
#include <stdio.h>
#define ESP_LOGE(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_GOTO_ON_FALSE(test, code, label, tag, ...) do { \
    (void)(tag); if (!(test)) { ret = (code); goto label; } } while (0)
#endif
void *eaf_alloc(size_t size);
void *eaf_alloc_aligned(size_t size, size_t alignment, size_t *allocated_size);
void *eaf_port_alloc_internal(size_t size);
int64_t eaf_port_now_us(void);
size_t eaf_port_inflate_raw(void *dst, size_t dst_size, const void *src, size_t src_size);
void eaf_port_sync(void *data, size_t size);

void eaf_port_release_buffer(void *data, size_t size);

#include <stdatomic.h>
#ifdef ESP_PLATFORM
#include <sys/lock.h>
typedef struct {
    _lock_t lock;
    atomic_bool done;
} eaf_once_t;
#define EAF_ONCE_INIT {0}
#else
#include <pthread.h>
typedef struct {
    pthread_mutex_t lock;
    atomic_bool done;
} eaf_once_t;
#define EAF_ONCE_INIT {PTHREAD_MUTEX_INITIALIZER, ATOMIC_VAR_INIT(false)}
#endif
void eaf_port_call_once(eaf_once_t *once, void (*initialize)(void));
void eaf_jpeg_backend_retain(void);
void eaf_jpeg_backend_release(void);
