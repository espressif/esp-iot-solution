/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "resources/present_memory.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"
#include <stdlib.h>
#ifdef PRESENT_HOST
#include <errno.h>
#include <time.h>
#else
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#if CONFIG_ESP_IPC_ENABLE
#include "esp_ipc.h"
#endif
#endif

#ifdef PRESENT_HOST
static present_lock_t token_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static present_lock_t token_lock = portMUX_INITIALIZER_UNLOCKED;
#endif
static uint64_t next_token = 1;

uint64_t present_new_token(void)
{
    /* Leaf lock: never calls into an engine/backend while holding it. */
    present_lock_enter(&token_lock, false);
    uint64_t value = 0;
    if (next_token != UINT64_MAX) {
        value = next_token++;
    }
    present_lock_exit(&token_lock, false);
    return value;
}

bool present_lock_init(present_lock_t *lock)
{
#ifdef PRESENT_HOST
    return pthread_mutex_init(lock, NULL) == 0;
#else
    *lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    return true;
#endif
}

void present_lock_destroy(present_lock_t *lock)
{
#ifdef PRESENT_HOST
    pthread_mutex_destroy(lock);
#else
    (void)lock;
#endif
}

void PRESENT_ISR_ATTR present_lock_enter(present_lock_t *lock, bool isr)
{
#ifdef PRESENT_HOST
    (void)isr;
    pthread_mutex_lock(lock);
#else
    if (isr) {
        portENTER_CRITICAL_ISR(lock);
    } else {
        portENTER_CRITICAL(lock);
    }
#endif
}

void PRESENT_ISR_ATTR present_lock_exit(present_lock_t *lock, bool isr)
{
#ifdef PRESENT_HOST
    (void)isr;
    pthread_mutex_unlock(lock);
#else
    if (isr) {
        portEXIT_CRITICAL_ISR(lock);
    } else {
        portEXIT_CRITICAL(lock);
    }
#endif
}

void *present_alloc(size_t size)
{
#ifdef PRESENT_HOST
    return calloc(1, size);
#else
    return heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

void *present_alloc_pixels(size_t size)
{
    if (size > SIZE_MAX - (PRESENT_BUFFER_ALIGNMENT - 1)) {
        return NULL;
    }
#ifdef PRESENT_HOST
    return aligned_alloc(PRESENT_BUFFER_ALIGNMENT, (size + PRESENT_BUFFER_ALIGNMENT - 1) & ~(size_t)(PRESENT_BUFFER_ALIGNMENT - 1));
#else
    return heap_caps_aligned_alloc(PRESENT_BUFFER_ALIGNMENT, (size + PRESENT_BUFFER_ALIGNMENT - 1) & ~(size_t)(PRESENT_BUFFER_ALIGNMENT - 1),
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
#endif
}

void *present_alloc_pixels_in(size_t size, bool external)
{
#ifdef PRESENT_HOST
    (void)external;
    return present_alloc_pixels(size);
#else
    if (!external) {
        return present_alloc_pixels(size);
    }
    if (size > SIZE_MAX - (PRESENT_BUFFER_ALIGNMENT - 1)) {
        return NULL;
    }
    return heap_caps_aligned_alloc(PRESENT_BUFFER_ALIGNMENT,
                                   (size + PRESENT_BUFFER_ALIGNMENT - 1) & ~(size_t)(PRESENT_BUFFER_ALIGNMENT - 1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
}

void present_free(void *memory)
{
    free(memory);
}

uint64_t PRESENT_ISR_ATTR present_now_us(void)
{
#ifdef PRESENT_HOST
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_nsec / 1000;
#else
    return (uint64_t)esp_timer_get_time();
#endif
}

uint64_t present_now_ms(void)
{
    return present_now_us() / 1000;
}

void present_pause(void)
{
#ifdef PRESENT_HOST
    const struct timespec delay = { .tv_nsec = 1000000 };
    nanosleep(&delay, NULL);
#else
    vTaskDelay(1);
#endif
}

bool present_expired(uint64_t start, uint32_t timeout_ms)
{
    return timeout_ms != UINT32_MAX && present_now_ms() - start >= timeout_ms;
}

present_task_t present_current_task(void)
{
#ifdef PRESENT_HOST
    return pthread_self();
#else
    return xTaskGetCurrentTaskHandle();
#endif
}

bool present_same_task(present_task_t a, present_task_t b)
{
#ifdef PRESENT_HOST
    return pthread_equal(a, b) != 0;
#else
    return a == b;
#endif
}

#if !defined(PRESENT_HOST) && CONFIG_ESP_IPC_ENABLE
static void isr_barrier(void *unused)
{
    (void)unused;
}
#endif
esp_err_t present_isr_barrier(uint32_t timeout)
{
#ifdef PRESENT_HOST
    (void)timeout;
    /* Host event producers have no preempted ISR; their callback joins remain
     * the mock driver's responsibility. Active calls are counted by adapter. */
    return ESP_OK;
#else
#if CONFIG_ESP_IPC_ENABLE
    uint64_t start = present_now_ms();
    for (unsigned core = 0; core < portNUM_PROCESSORS; ++core) {
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        /* Include this CPU too: callers may migrate between iterations. A task
         * on that CPU cannot run until an ISR holding an old user_ctx exits. */
        esp_err_t error = esp_ipc_call_blocking(core, isr_barrier, NULL);
        if (error != ESP_OK) {
            return error;
        }
    }
    return present_expired(start, timeout) ? ESP_ERR_TIMEOUT : ESP_OK;
#else
#if CONFIG_FREERTOS_UNICORE
    (void)timeout;
    return ESP_OK;
#else
    (void)timeout;
    return ESP_ERR_NOT_SUPPORTED;
#endif
#endif
#endif
}

struct present_os_event {
#ifdef PRESENT_HOST
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool signaled;
#else
    StaticSemaphore_t storage;
    SemaphoreHandle_t semaphore;
#endif
};
struct present_mutex {
#ifdef PRESENT_HOST
    present_os_event_t *available;
#else
    StaticSemaphore_t storage;
    SemaphoreHandle_t semaphore;
#endif
};
struct present_worker {
    present_worker_entry_fn entry;
    void *user;
    present_os_event_t *exited;
    bool joined;
#ifdef PRESENT_HOST
    pthread_t thread;
#else
    TaskHandle_t task;
#endif
};
#ifndef PRESENT_HOST
static TickType_t timeout_ticks(uint32_t timeout)
{
    if (timeout == UINT32_MAX) {
        return portMAX_DELAY;
    }
    if (!timeout) {
        return 0;
    }
    uint64_t ticks = ((uint64_t)timeout * configTICK_RATE_HZ + 999) / 1000;
    return ticks >= portMAX_DELAY ? portMAX_DELAY - 1 : (TickType_t)ticks;
}
#endif
present_os_event_t *present_event_create(void)
{
    present_os_event_t *event = present_alloc(sizeof(*event));
    if (!event) {
        return NULL;
    }
#ifdef PRESENT_HOST
    if (pthread_mutex_init(&event->mutex, NULL) != 0) {
        present_free(event);
        return NULL;
    }
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr) != 0) {
        pthread_mutex_destroy(&event->mutex);
        present_free(event);
        return NULL;
    }
    int result = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!result) {
        result = pthread_cond_init(&event->condition, &attr);
    }
    pthread_condattr_destroy(&attr);
    if (result) {
        pthread_mutex_destroy(&event->mutex);
        present_free(event);
        return NULL;
    }
#else
    event->semaphore = xSemaphoreCreateBinaryStatic(&event->storage);
    if (!event->semaphore) {
        present_free(event);
        return NULL;
    }
#endif
    return event;
}

bool PRESENT_ISR_ATTR present_event_signal(present_os_event_t *event, bool isr)
{
    if (!event) {
        return false;
    }
#ifdef PRESENT_HOST
    (void)isr;
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    pthread_cond_signal(&event->condition);
    pthread_mutex_unlock(&event->mutex);
    return false;
#else
    if (isr) {
        BaseType_t yield = pdFALSE;
        xSemaphoreGiveFromISR(event->semaphore, &yield);
        return yield == pdTRUE;
    }
    xSemaphoreGive(event->semaphore);
    return false;
#endif
}

bool present_event_wait(present_os_event_t *event, uint32_t timeout)
{
    if (!event) {
        return false;
    }
#ifdef PRESENT_HOST
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout / 1000;
    deadline.tv_nsec += (long)(timeout % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }
    pthread_mutex_lock(&event->mutex);
    int result = 0;
    while (!event->signaled && !result && timeout) {
        result = timeout == UINT32_MAX ? pthread_cond_wait(&event->condition, &event->mutex) :
                 pthread_cond_timedwait(&event->condition, &event->mutex, &deadline);
    }
    bool signaled = event->signaled;
    if (signaled) {
        event->signaled = false;
    }
    pthread_mutex_unlock(&event->mutex);
    return signaled;
#else
    return xSemaphoreTake(event->semaphore, timeout_ticks(timeout)) == pdTRUE;
#endif
}

void present_event_destroy(present_os_event_t *event)
{
    if (!event) {
        return;
    }
#ifdef PRESENT_HOST
    pthread_cond_destroy(&event->condition);
    pthread_mutex_destroy(&event->mutex);
#else
    vSemaphoreDelete(event->semaphore);
#endif
    present_free(event);
}

present_mutex_t *present_mutex_create(void)
{
    present_mutex_t *mutex = present_alloc(sizeof(*mutex));
    if (!mutex) {
        return NULL;
    }
#ifdef PRESENT_HOST
    mutex->available = present_event_create();
    if (!mutex->available) {
        present_free(mutex);
        return NULL;
    }
    (void)present_event_signal(mutex->available, false);
#else
    mutex->semaphore = xSemaphoreCreateMutexStatic(&mutex->storage);
    if (!mutex->semaphore) {
        present_free(mutex);
        return NULL;
    }
#endif
    return mutex;
}

bool present_mutex_take(present_mutex_t *mutex, uint32_t timeout)
{
#ifdef PRESENT_HOST
    return present_event_wait(mutex->available, timeout);
#else
    return xSemaphoreTake(mutex->semaphore, timeout_ticks(timeout)) == pdTRUE;
#endif
}

void present_mutex_give(present_mutex_t *mutex)
{
#ifdef PRESENT_HOST
    (void)present_event_signal(mutex->available, false);
#else
    xSemaphoreGive(mutex->semaphore);
#endif
}

void present_mutex_destroy(present_mutex_t *mutex)
{
    if (!mutex) {
        return;
    }
#ifdef PRESENT_HOST
    present_event_destroy(mutex->available);
#else
    vSemaphoreDelete(mutex->semaphore);
#endif
    present_free(mutex);
}
#ifdef PRESENT_HOST
static void *worker_entry(void *context)
#else
static void worker_entry(void *context)
#endif
{
    present_worker_t *worker = context;
    worker->entry(worker->user);
    /* No access to worker or user after publishing exit. Host join additionally
     * waits for pthread return; FreeRTOS self-delete only touches its own TCB. */
    (void)present_event_signal(worker->exited, false);
#ifdef PRESENT_HOST
    return NULL;
#else
    vTaskDelete(NULL);
#endif
}

unsigned present_task_priority(void)
{
#ifdef PRESENT_HOST
    return 0;
#else
    return uxTaskPriorityGet(NULL);
#endif
}

void present_task_set_priority(unsigned priority)
{
#ifdef PRESENT_HOST
    (void)priority;
#else
    vTaskPrioritySet(NULL, priority);
#endif
}

present_worker_t *present_worker_create(const char *name, present_worker_entry_fn entry, void *user, uint32_t stack_bytes, unsigned priority)
{
    if (!name || !*name || !entry) {
        return NULL;
    }
    present_worker_t *worker = present_alloc(sizeof(*worker));
    if (!worker) {
        return NULL;
    }
    worker->entry = entry;
    worker->user = user;
    worker->exited = present_event_create();
    if (!worker->exited) {
        present_free(worker);
        return NULL;
    }
#ifdef PRESENT_HOST
    (void)name;
    (void)stack_bytes;
    (void)priority;
    bool created = pthread_create(&worker->thread, NULL, worker_entry, worker) == 0;
#else
    bool created = priority < configMAX_PRIORITIES &&
                   xTaskCreate(worker_entry, name, stack_bytes, worker, priority, &worker->task) == pdPASS;
#endif
    if (!created) {
        present_event_destroy(worker->exited);
        present_free(worker);
        return NULL;
    }
    return worker;
}

bool present_worker_join(present_worker_t *worker, uint32_t timeout)
{
    if (worker->joined) {
        return true;
    }
    if (!present_event_wait(worker->exited, timeout)) {
        return false;
    }
#ifdef PRESENT_HOST
    if (pthread_join(worker->thread, NULL) != 0) {
        return false;
    }
#endif
    worker->joined = true;
    return true;
}

void present_worker_destroy(present_worker_t *worker)
{
    if (!worker || !worker->joined) {
        return;
    }
    present_event_destroy(worker->exited);
    present_free(worker);
}

size_t present_event_memory(void)
{
    return sizeof(present_os_event_t);
}
size_t present_mutex_memory(void)
{
    size_t bytes = sizeof(present_mutex_t);
#ifdef PRESENT_HOST
    bytes += sizeof(present_os_event_t);
#endif
    return bytes;
}
size_t present_worker_memory(void)
{
    size_t bytes = sizeof(present_worker_t) + sizeof(present_os_event_t);
#ifndef PRESENT_HOST
    bytes += sizeof(StaticTask_t);
#endif
    return bytes;
}
