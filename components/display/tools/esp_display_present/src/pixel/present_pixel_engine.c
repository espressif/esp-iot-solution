/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "resources/present_memory.h"
#include "pixel/present_pixel_engine.h"
#include "pixel/present_pixel_ops.h"
#include "pixel/present_pixel_ops_internal.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"
#include <string.h>

typedef struct {
    uint64_t token;
    esp_display_present_copy_status_t status;
    esp_display_present_copy_request_t request;
    present_os_event_t *changed;
    bool waiting;
    bool canceled;
} copy_record_t;

/* One lock still protects all mutable groups below. Nesting expresses
 * responsibility; it does not introduce independent locks or owners. */
typedef struct {
    present_lock_t lock;
    present_os_event_t *idle;
} pixel_sync_t;
typedef struct {
    bool closing, stopped;
    bool executing; /* Serializes overlapping writes, including sync CPU calls. */
} pixel_lifecycle_t;
typedef struct {
    esp_display_present_pixel_event_fn on_event;
    void *user;
    bool active;
    present_task_t task; /* Callback reentry detection, not a worker handle. */
} pixel_notification_t;
typedef struct {
    esp_display_present_pixel_execute_fn execute;
    void *user;
} pixel_executor_binding_t;
typedef struct {
    bool initialized, applied;
    unsigned value;
} pixel_worker_priority_t;
typedef struct {
    present_os_event_t *work;
    present_worker_t *task;
    pixel_worker_priority_t priority;
    bool asynchronous;
} pixel_worker_t;

struct esp_display_present_pixel_engine {
    pixel_sync_t sync;
    pixel_lifecycle_t lifecycle;
    pixel_notification_t notification;
    pixel_executor_binding_t executor;
    pixel_worker_t worker;
    size_t count;
    copy_record_t records[];
};

/* The record may be released by the callback; no access to it after cb. */
static void finish_copy(esp_display_present_pixel_engine_t *engine, copy_record_t *record, esp_err_t result)
{
    present_lock_enter(&engine->sync.lock, false);
    record->status.state = result == ESP_OK ? ESP_DISPLAY_PRESENT_COPY_SUCCEEDED : ESP_DISPLAY_PRESENT_COPY_FAILED;
    record->status.result = result;
    esp_display_present_copy_status_t status = record->status;
    esp_display_present_copy_ticket_t ticket = {record->token};
    esp_display_present_pixel_event_fn cb = engine->notification.on_event;
    engine->notification.active = cb != NULL;
    if (cb) {
        engine->notification.task = present_current_task();
    }
    /* Publish while record ownership is stable, before callback can release it. */
    present_os_event_t *changed = record->changed;
    present_lock_exit(&engine->sync.lock, false);
    (void)present_event_signal(changed, false);
    if (cb) {
        (void)cb(engine->notification.user, ticket, &status, false);
    }
    present_lock_enter(&engine->sync.lock, false);
    engine->notification.active = false;
    engine->lifecycle.executing = false;
    present_lock_exit(&engine->sync.lock, false);
    (void)present_event_signal(engine->sync.idle, false);
}

static void pixel_worker(void *context)
{
    esp_display_present_pixel_engine_t *engine = context;
    for (;;) {
        present_lock_enter(&engine->sync.lock, false);
        copy_record_t *next = NULL;
        for (size_t i = 0; i < engine->count; i++) {
            copy_record_t *record = &engine->records[i];
            if (record->token && record->status.state == ESP_DISPLAY_PRESENT_COPY_QUEUED &&
                    (!next || record->token < next->token)) {
                next = record;
            }
        }
        if (!next) {
            bool exit = engine->lifecycle.closing;
            present_lock_exit(&engine->sync.lock, false);
            if (exit) {
                return;
            }
            (void)present_event_wait(engine->worker.work, UINT32_MAX);
            continue;
        }
        bool canceled = next->canceled;
        bool set_priority = !engine->worker.priority.applied;
        unsigned priority = engine->worker.priority.value;
        engine->worker.priority.applied = true;
        next->status.state = ESP_DISPLAY_PRESENT_COPY_RUNNING;
        engine->lifecycle.executing = true;
        esp_display_present_copy_request_t request = next->request;
        present_lock_exit(&engine->sync.lock, false);
        /* Apply from the live worker itself. A concurrent stop cannot leave a
         * producer changing the priority of an already-deleted task handle. */
        if (set_priority) {
            present_task_set_priority(priority);
        }
        esp_err_t result = ESP_ERR_INVALID_STATE;
        if (!canceled) {
            if (engine->executor.execute) {
                result = engine->executor.execute(engine->executor.user, &request);
            } else {
                present_pixel_copy_cpu_validated(&request);
                result = ESP_OK;
            }
        }
        finish_copy(engine, next, result);
    }
}

static void free_engine(esp_display_present_pixel_engine_t *engine)
{
    for (size_t i = 0; i < engine->count; i++) {
        present_event_destroy(engine->records[i].changed);
    }
    present_worker_destroy(engine->worker.task);
    present_event_destroy(engine->worker.work);
    present_event_destroy(engine->sync.idle);
    present_lock_destroy(&engine->sync.lock);
    present_free(engine);
}

esp_err_t esp_display_present_pixel_create(const esp_display_present_pixel_config_t *config, esp_display_present_pixel_engine_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!config || !config->max_jobs || config->max_jobs > PRESENT_MAX_COPY_JOBS) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_pixel_engine_t *engine = present_alloc(sizeof(*engine) + config->max_jobs * sizeof(copy_record_t));
    if (!engine) {
        return ESP_ERR_NO_MEM;
    }
    if (!present_lock_init(&engine->sync.lock)) {
        present_free(engine);
        return ESP_ERR_NO_MEM;
    }
    engine->count = config->max_jobs;
    engine->notification.on_event = config->on_event;
    engine->notification.user = config->user;
    engine->worker.asynchronous = config->asynchronous;
    engine->executor.execute = config->execute;
    engine->executor.user = config->execute_user;
    engine->worker.work = present_event_create();
    engine->sync.idle = present_event_create();
    if (!engine->worker.work || !engine->sync.idle) {
        free_engine(engine);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < engine->count; i++) {
        engine->records[i].changed = present_event_create();
        if (!engine->records[i].changed) {
            free_engine(engine);
            return ESP_ERR_NO_MEM;
        }
    }
    if (engine->worker.asynchronous) {
        engine->worker.task = present_worker_create("present_pixel", pixel_worker, engine, 4096, 5);
        if (!engine->worker.task) {
            free_engine(engine);
            return ESP_ERR_NO_MEM;
        }
    }
    *out = engine;
    return ESP_OK;
}

esp_err_t esp_display_present_pixel_submit(esp_display_present_pixel_engine_t *engine, const esp_display_present_copy_request_t *request, esp_display_present_copy_ticket_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->value = 0;
    if (!engine) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = esp_display_present_pixel_validate_request(request);
    if (result != ESP_OK) {
        return result;
    }
    unsigned priority = engine->worker.asynchronous ? present_task_priority() : 0;
    present_lock_enter(&engine->sync.lock, false);
    if (engine->lifecycle.closing) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    if ((!engine->worker.asynchronous && engine->lifecycle.executing) ||
            (engine->notification.active && present_same_task(engine->notification.task, present_current_task()))) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_NO_MEM;
    }
    copy_record_t *record = NULL;
    for (size_t i = 0; i < engine->count; i++) {
        if (!engine->records[i].token) {
            record = &engine->records[i];
            break;
        }
    }
    uint64_t token = record ? present_new_token() : 0;
    if (!token) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_NO_MEM;
    }
    bool inherit_priority = engine->worker.asynchronous && !engine->worker.priority.initialized;
    if (inherit_priority) {
        engine->worker.priority.value = priority;
        engine->worker.priority.initialized = true;
    }
    record->token = token;
    record->status = (esp_display_present_copy_status_t) {
        .state = engine->worker.asynchronous ? ESP_DISPLAY_PRESENT_COPY_QUEUED : ESP_DISPLAY_PRESENT_COPY_RUNNING
    };
    record->request = *request;
    record->canceled = false;
    out->value = token;
    if (!engine->worker.asynchronous) {
        engine->lifecycle.executing = true;
    }
    present_lock_exit(&engine->sync.lock, false);
    if (engine->worker.asynchronous) {
        (void)present_event_signal(engine->worker.work, false);
    } else {
        esp_err_t exec_res = engine->executor.execute ? engine->executor.execute(engine->executor.user, request) : (present_pixel_copy_cpu_validated(request), ESP_OK);
        finish_copy(engine, record, exec_res);
    }
    return ESP_OK;
}

static copy_record_t *find_record(esp_display_present_pixel_engine_t *engine, esp_display_present_copy_ticket_t ticket)
{
    for (size_t i = 0; i < engine->count; i++) {
        if (engine->records[i].token == ticket.value) {
            return &engine->records[i];
        }
    }
    return NULL;
}

esp_err_t esp_display_present_pixel_query(esp_display_present_pixel_engine_t *engine, esp_display_present_copy_ticket_t ticket, esp_display_present_copy_status_t *out)
{
    if (!engine || !out || !ticket.value) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&engine->sync.lock, false);
    copy_record_t *record = find_record(engine, ticket);
    if (record) {
        *out = record->status;
    }
    present_lock_exit(&engine->sync.lock, false);
    return record ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static bool terminal(esp_display_present_copy_status_t status)
{
    return status.state == ESP_DISPLAY_PRESENT_COPY_SUCCEEDED || status.state == ESP_DISPLAY_PRESENT_COPY_FAILED;
}

esp_err_t esp_display_present_pixel_wait(esp_display_present_pixel_engine_t *engine, esp_display_present_copy_ticket_t ticket, uint32_t timeout, esp_display_present_copy_status_t *out)
{
    if (!engine || !out || !ticket.value) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    present_lock_enter(&engine->sync.lock, false);
    copy_record_t *record = find_record(engine, ticket);
    if (!record || record->waiting) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    if (engine->notification.active && present_same_task(engine->notification.task, present_current_task()) && !terminal(record->status)) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    record->waiting = true;
    for (;;) {
        *out = record->status;
        if (terminal(*out) || present_expired(start, timeout)) {
            record->waiting = false;
            present_lock_exit(&engine->sync.lock, false);
            return terminal(*out) ? ESP_OK : ESP_ERR_TIMEOUT;
        }
        present_lock_exit(&engine->sync.lock, false);
        uint64_t elapsed = present_now_ms() - start;
        uint32_t left = timeout == UINT32_MAX ? UINT32_MAX : elapsed >= timeout ? 0 : timeout - elapsed;
        (void)present_event_wait(record->changed, left);
        present_lock_enter(&engine->sync.lock, false);
    }
}

esp_err_t esp_display_present_pixel_cancel(esp_display_present_pixel_engine_t *engine, esp_display_present_copy_ticket_t ticket, uint32_t timeout, esp_display_present_copy_status_t *out)
{
    if (!engine || !ticket.value || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&engine->sync.lock, false);
    copy_record_t *record = find_record(engine, ticket);
    if (!record) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    if (record->status.state == ESP_DISPLAY_PRESENT_COPY_QUEUED) {
        record->canceled = true;
    }
    present_lock_exit(&engine->sync.lock, false);
    (void)present_event_signal(engine->worker.work, false);
    return esp_display_present_pixel_wait(engine, ticket, timeout, out);
}

esp_err_t esp_display_present_pixel_release(esp_display_present_pixel_engine_t *engine, esp_display_present_copy_ticket_t ticket)
{
    if (!engine || !ticket.value) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&engine->sync.lock, false);
    copy_record_t *record = find_record(engine, ticket);
    if (!record || record->waiting || !terminal(record->status)) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    record->token = 0;
    present_lock_exit(&engine->sync.lock, false);
    return ESP_OK;
}

esp_err_t esp_display_present_pixel_stop(esp_display_present_pixel_engine_t *engine, uint32_t timeout)
{
    if (!engine) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    present_lock_enter(&engine->sync.lock, false);
    if (engine->notification.active && present_same_task(engine->notification.task, present_current_task())) {
        present_lock_exit(&engine->sync.lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    engine->lifecycle.closing = true;
    present_lock_exit(&engine->sync.lock, false);
    (void)present_event_signal(engine->worker.work, false);
    if (engine->worker.asynchronous) {
        if (!present_worker_join(engine->worker.task, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
    } else {
        for (;;) {
            present_lock_enter(&engine->sync.lock, false);
            bool busy = engine->lifecycle.executing;
            present_lock_exit(&engine->sync.lock, false);
            if (!busy) {
                break;
            }
            uint64_t elapsed = present_now_ms() - start;
            if (timeout != UINT32_MAX && elapsed >= timeout) {
                return ESP_ERR_TIMEOUT;
            }
            (void)present_event_wait(engine->sync.idle, timeout == UINT32_MAX ? UINT32_MAX : timeout - elapsed);
        }
    }
    engine->lifecycle.stopped = true;
    return ESP_OK;
}

esp_err_t esp_display_present_pixel_destroy(esp_display_present_pixel_engine_t *engine)
{
    if (!engine) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!engine->lifecycle.stopped) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < engine->count; i++) {
        if (engine->records[i].token || engine->records[i].waiting) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    free_engine(engine);
    return ESP_OK;
}

size_t present_pixel_memory(size_t jobs, bool async)
{
    return sizeof(esp_display_present_pixel_engine_t) + jobs * sizeof(copy_record_t) + (jobs + 2) * present_event_memory() +
           (async ? present_worker_memory() : 0);
}
