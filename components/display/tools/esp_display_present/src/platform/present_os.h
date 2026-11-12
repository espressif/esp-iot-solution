/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef PRESENT_HOST
#include <pthread.h>

typedef pthread_mutex_t present_lock_t;
typedef pthread_t present_task_t;
#define PRESENT_ISR_ATTR
#else
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

typedef portMUX_TYPE present_lock_t;
typedef TaskHandle_t present_task_t;
#define PRESENT_ISR_ATTR IRAM_ATTR
#endif

bool present_lock_init(present_lock_t *lock);
void present_lock_destroy(present_lock_t *lock);
/* Definitions carry IRAM_ATTR; repeating it on declarations creates different
 * section names with ESP-IDF's __COUNTER__-based attribute. */
void present_lock_enter(present_lock_t *lock, bool isr);
void present_lock_exit(present_lock_t *lock, bool isr);
void *present_alloc(size_t size);
void *present_alloc_pixels(size_t size);
void *present_alloc_pixels_in(size_t size, bool external);
void present_free(void *memory);
uint64_t present_now_us(void);
uint64_t present_now_ms(void);
void present_pause(void);
bool present_expired(uint64_t start, uint32_t timeout_ms);
/* Process-wide tokens never wrap/reuse; exhaustion returns zero. */
uint64_t present_new_token(void);
present_task_t present_current_task(void);
bool present_same_task(present_task_t a, present_task_t b);

/* Latched binary events. Notification domains use separate waiter events.
 * Signal-before-wait is retained;
 * coalescing is intentional: callers always recheck authoritative state.
 * Destroy requires all signalers/waiters to have exited. */
typedef struct present_os_event present_os_event_t;
present_os_event_t *present_event_create(void);
bool present_event_signal(present_os_event_t *event, bool from_isr);
bool present_event_wait(present_os_event_t *event, uint32_t timeout_ms);
void present_event_destroy(present_os_event_t *event);
/* Task-only nonrecursive serialization, never held while waiting for hardware. */
typedef struct present_mutex present_mutex_t;
present_mutex_t *present_mutex_create(void);
bool present_mutex_take(present_mutex_t *mutex, uint32_t timeout_ms);
void present_mutex_give(present_mutex_t *mutex);
void present_mutex_destroy(present_mutex_t *mutex);
typedef struct present_worker present_worker_t;
typedef void (*present_worker_entry_fn)(void *user);
present_worker_t *present_worker_create(const char *name, present_worker_entry_fn entry, void *user, uint32_t stack_bytes, unsigned priority);
/* Host leaves scheduling unchanged. On IDF these apply to the calling task. */
unsigned present_task_priority(void);
void present_task_set_priority(unsigned priority);
/* Entry must be requested to exit by caller before join. Timeout preserves
 * object/thread. Success joins its final exit and allows destroy. */
bool present_worker_join(present_worker_t *worker, uint32_t timeout_ms);
void present_worker_destroy(present_worker_t *worker);

/* Task-only join for driver ISRs after callback unregister. No argument is
 * retained. IDF IPC calls can block independently of the poll budget; timeout
 * is checked before/after each native join, as for native panel deletion. */
esp_err_t present_isr_barrier(uint32_t timeout_ms);
