/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <esp_timer.h>

#include <esp_check.h>
#include <esp_log.h>
#include <sys/queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esp_mcp_task_priv.h"

static const char *TAG = "esp_mcp_task";

struct esp_mcp_task_s {
    char *task_id;
    char origin_session_id[32];
    esp_mcp_task_status_t status;
    char *status_message;
    char *result_json;
    bool result_is_error_response;
    bool cancel_requested;
    int64_t created_at;
    int64_t last_updated_at;
    int64_t created_monotonic_ms;
    bool has_ttl;
    int64_t ttl;
    struct esp_mcp_task_store_s *store;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t worker_done;
    bool worker_running;
    SLIST_ENTRY(esp_mcp_task_s) next;
};

struct esp_mcp_task_store_s {
    SLIST_HEAD(task_table_t, esp_mcp_task_s) tasks;
    SemaphoreHandle_t mutex;
    esp_mcp_task_status_cb_t status_cb;
    void *status_cb_ctx;
};

#ifndef CONFIG_MCP_TASKS_LIST_MAX_COUNT
#define CONFIG_MCP_TASKS_LIST_MAX_COUNT 100
#endif

static esp_err_t task_destroy(esp_mcp_task_t *task);

static bool esp_mcp_task_is_terminal(esp_mcp_task_status_t status)
{
    return status == ESP_MCP_TASK_COMPLETED ||
           status == ESP_MCP_TASK_FAILED ||
           status == ESP_MCP_TASK_CANCELLED;
}

static char *dup_or_null(const char *src)
{
    if (!src) {
        return NULL;
    }
    return strdup(src);
}

static void task_lock(const esp_mcp_task_t *task)
{
    if (task && task->mutex) {
        (void)xSemaphoreTake(task->mutex, portMAX_DELAY);
    }
}

static void task_unlock(const esp_mcp_task_t *task)
{
    if (task && task->mutex) {
        (void)xSemaphoreGive(task->mutex);
    }
}

static bool esp_mcp_task_store_prune_one_terminal_locked(esp_mcp_task_store_t *store)
{
    esp_mcp_task_t *it = NULL;
    esp_mcp_task_t *prev = NULL;
    esp_mcp_task_t *victim = NULL;
    SLIST_FOREACH(it, &store->tasks, next) {
        if (!esp_mcp_task_is_terminal(it->status)) {
            prev = it;
            continue;
        }
        task_lock(it);
        bool safe = !it->worker_running;
        task_unlock(it);
        if (!safe) {
            prev = it;
            continue;
        }
        if (prev) {
            SLIST_NEXT(prev, next) = SLIST_NEXT(it, next);
        } else {
            SLIST_REMOVE_HEAD(&store->tasks, next);
        }
        victim = it;
        break;
    }
    if (victim) {
        (void)task_destroy(victim);
        return true;
    }
    return false;
}

static bool esp_mcp_task_status_can_transit(esp_mcp_task_status_t from, esp_mcp_task_status_t to)
{
    if (from == to) {
        return true;
    }
    switch (from) {
    case ESP_MCP_TASK_WORKING:
        return to == ESP_MCP_TASK_COMPLETED ||
               to == ESP_MCP_TASK_FAILED ||
               to == ESP_MCP_TASK_CANCELLED ||
               to == ESP_MCP_TASK_INPUT_REQUIRED ||
               to == ESP_MCP_TASK_CANCELLING;
    case ESP_MCP_TASK_CANCELLING:
        return to == ESP_MCP_TASK_COMPLETED ||
               to == ESP_MCP_TASK_FAILED ||
               to == ESP_MCP_TASK_CANCELLED;
    case ESP_MCP_TASK_INPUT_REQUIRED:
        return to == ESP_MCP_TASK_WORKING ||
               to == ESP_MCP_TASK_COMPLETED ||
               to == ESP_MCP_TASK_FAILED ||
               to == ESP_MCP_TASK_CANCELLED;
    case ESP_MCP_TASK_COMPLETED:
    case ESP_MCP_TASK_FAILED:
    case ESP_MCP_TASK_CANCELLED:
    default:
        return false;
    }
}

static esp_err_t task_destroy(esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");

    SemaphoreHandle_t worker_done = NULL;
    bool wait_for_worker = false;
    task_lock(task);
    if (task->worker_running) {
        __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
        worker_done = task->worker_done;
        wait_for_worker = true;
    }
    task_unlock(task);

    if (wait_for_worker && worker_done) {
        (void)xSemaphoreTake(worker_done, portMAX_DELAY);
    }

    SemaphoreHandle_t mutex = task->mutex;
    worker_done = task->worker_done;
    if (mutex) {
        (void)xSemaphoreTake(mutex, portMAX_DELAY);
    }
    free(task->task_id);
    free(task->status_message);
    free(task->result_json);
    if (mutex) {
        (void)xSemaphoreGive(mutex);
        vSemaphoreDelete(mutex);
    }
    if (worker_done) {
        vSemaphoreDelete(worker_done);
    }
    free(task);
    return ESP_OK;
}

esp_mcp_task_store_t *esp_mcp_task_store_create(void)
{
    esp_mcp_task_store_t *store = calloc(1, sizeof(*store));
    ESP_RETURN_ON_FALSE(store, NULL, TAG, "No mem");
    SLIST_INIT(&store->tasks);
    store->mutex = xSemaphoreCreateMutex();
    store->status_cb = NULL;
    store->status_cb_ctx = NULL;
    if (!store->mutex) {
        free(store);
        return NULL;
    }
    return store;
}

void esp_mcp_task_store_set_status_cb(esp_mcp_task_store_t *store,
                                      esp_mcp_task_status_cb_t cb,
                                      void *user_ctx)
{
    if (!store) {
        return;
    }
    if (store->mutex && xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
        store->status_cb = cb;
        store->status_cb_ctx = user_ctx;
        xSemaphoreGive(store->mutex);
    }
}

esp_err_t esp_mcp_task_store_clear(esp_mcp_task_store_t *store)
{
    ESP_RETURN_ON_FALSE(store, ESP_ERR_INVALID_ARG, TAG, "Invalid store");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_mcp_task_t *task = NULL;
    SLIST_FOREACH(task, &store->tasks, next) {
        if (!esp_mcp_task_is_terminal(task->status)) {
            __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
        }
    }
    xSemaphoreGive(store->mutex);

    for (int i = 0; i < 100; i++) {
        bool all_done = true;
        if (xSemaphoreTake(store->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            SLIST_FOREACH(task, &store->tasks, next) {
                if (task->status == ESP_MCP_TASK_WORKING || task->status == ESP_MCP_TASK_CANCELLING) {
                    all_done = false;
                    break;
                }
            }
            xSemaphoreGive(store->mutex);
        } else {
            all_done = false;
        }
        if (all_done) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (;;) {
        if (xSemaphoreTake(store->mutex, portMAX_DELAY) != pdTRUE) {
            return ESP_ERR_INVALID_STATE;
        }
        esp_mcp_task_t *t = SLIST_FIRST(&store->tasks);
        if (!t) {
            xSemaphoreGive(store->mutex);
            return ESP_OK;
        }
        SLIST_REMOVE_HEAD(&store->tasks, next);
        xSemaphoreGive(store->mutex);
        (void)task_destroy(t);
    }
}

esp_err_t esp_mcp_task_store_destroy(esp_mcp_task_store_t *store)
{
    ESP_RETURN_ON_FALSE(store, ESP_ERR_INVALID_ARG, TAG, "Invalid store");

    if (store->mutex && xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
        /* Set cancel under store lock only; avoid task mutex while holding store mutex. */
        esp_mcp_task_t *task = NULL;
        SLIST_FOREACH(task, &store->tasks, next) {
            if (!esp_mcp_task_is_terminal(task->status)) {
                __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
            }
        }
        xSemaphoreGive(store->mutex);

        // Wait briefly for workers to observe cancellation before freeing tasks.
        for (int i = 0; i < 100; i++) {
            bool all_done = true;
            if (xSemaphoreTake(store->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                SLIST_FOREACH(task, &store->tasks, next) {
                    if (task->status == ESP_MCP_TASK_WORKING || task->status == ESP_MCP_TASK_CANCELLING) {
                        all_done = false;
                        break;
                    }
                }
                xSemaphoreGive(store->mutex);
            } else {
                all_done = false;
            }
            if (all_done) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        while (xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
            esp_mcp_task_t *t = SLIST_FIRST(&store->tasks);
            if (!t) {
                vSemaphoreDelete(store->mutex);
                store->mutex = NULL;
                break;
            }
            SLIST_REMOVE_HEAD(&store->tasks, next);
            xSemaphoreGive(store->mutex);
            (void)task_destroy(t);
        }
    }
    free(store);
    return ESP_OK;
}

esp_mcp_task_t *esp_mcp_task_create(esp_mcp_task_store_t *store, const char *task_id)
{
    ESP_RETURN_ON_FALSE(store && task_id, NULL, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE, NULL, TAG, "Mutex take failed");

    esp_mcp_task_t *it = NULL;
    size_t task_count = 0;
    SLIST_FOREACH(it, &store->tasks, next) {
        task_count++;
        if (it->task_id && !strcmp(it->task_id, task_id)) {
            xSemaphoreGive(store->mutex);
            return NULL;
        }
    }

    if (task_count >= (size_t)CONFIG_MCP_TASKS_LIST_MAX_COUNT) {
        bool pruned = esp_mcp_task_store_prune_one_terminal_locked(store);
        if (!pruned) {
            ESP_LOGW(TAG, "Task store full (%d) and no terminal task to prune", CONFIG_MCP_TASKS_LIST_MAX_COUNT);
            xSemaphoreGive(store->mutex);
            return NULL;
        }
    }

    esp_mcp_task_t *task = calloc(1, sizeof(*task));
    if (!task) {
        xSemaphoreGive(store->mutex);
        return NULL;
    }
    task->task_id = strdup(task_id);
    task->status = ESP_MCP_TASK_WORKING;
    __atomic_store_n(&task->cancel_requested, false, __ATOMIC_RELEASE);
    task->result_is_error_response = false;
    task->has_ttl = false;
    task->ttl = 0;
    task->store = store;
    task->mutex = xSemaphoreCreateMutex();
    task->worker_done = xSemaphoreCreateBinary();
    task->worker_running = false;
    if (!task->task_id || !task->mutex || !task->worker_done) {
        (void)task_destroy(task);
        xSemaphoreGive(store->mutex);
        return NULL;
    }

    time_t now = time(NULL);
    if (now >= 1577836800) {
        task->created_at = (int64_t)now;
        task->last_updated_at = (int64_t)now;
    } else {
        task->created_at = 0;
        task->last_updated_at = 0;
    }
    task->created_monotonic_ms = esp_timer_get_time() / 1000;

    SLIST_INSERT_HEAD(&store->tasks, task, next);
    xSemaphoreGive(store->mutex);
    return task;
}

esp_err_t esp_mcp_task_mark_worker_starting(esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");
    task_lock(task);
    task->worker_running = true;
    while (task->worker_done && xSemaphoreTake(task->worker_done, 0) == pdTRUE) {
    }
    task_unlock(task);
    return ESP_OK;
}

void esp_mcp_task_mark_worker_finished(esp_mcp_task_t *task)
{
    if (!task) {
        return;
    }

    bool should_signal = false;
    SemaphoreHandle_t worker_done = NULL;
    task_lock(task);
    worker_done = task->worker_done;
    should_signal = task->worker_running;
    task->worker_running = false;
    task_unlock(task);

    if (should_signal && worker_done) {
        (void)xSemaphoreGive(worker_done);
    }
}

esp_mcp_task_t *esp_mcp_task_find(const esp_mcp_task_store_t *store, const char *task_id)
{
    ESP_RETURN_ON_FALSE(store && task_id, NULL, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE, NULL, TAG, "Mutex take failed");

    esp_mcp_task_t *found = NULL;
    esp_mcp_task_t *it = NULL;
    SLIST_FOREACH(it, &store->tasks, next) {
        if (it->task_id && !strcmp(it->task_id, task_id)) {
            found = it;
            break;
        }
    }
    xSemaphoreGive(store->mutex);
    return found;
}

esp_err_t esp_mcp_task_set_origin_session(esp_mcp_task_t *task, const char *session_id)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");
    task_lock(task);
    if (session_id && session_id[0]) {
        snprintf(task->origin_session_id, sizeof(task->origin_session_id), "%s", session_id);
    } else {
        task->origin_session_id[0] = '\0';
    }
    task_unlock(task);
    return ESP_OK;
}

const char *esp_mcp_task_origin_session(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, NULL, TAG, "Invalid task");
    return task->origin_session_id[0] ? task->origin_session_id : NULL;
}

esp_err_t esp_mcp_task_store_foreach(const esp_mcp_task_store_t *store,
                                     esp_err_t (*callback)(esp_mcp_task_t *task, void *arg),
                                     void *arg)
{
    ESP_RETURN_ON_FALSE(store && callback, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_INVALID_STATE, TAG, "Mutex take failed");

    esp_err_t ret = ESP_OK;
    esp_mcp_task_t *it = NULL;
    SLIST_FOREACH(it, &store->tasks, next) {
        ret = callback(it, arg);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(store->mutex);
    return ret;
}

const char *esp_mcp_task_id(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, NULL, TAG, "Invalid task");
    return task->task_id;
}

esp_mcp_task_status_t esp_mcp_task_get_status(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, ESP_MCP_TASK_FAILED, TAG, "Invalid task");
    task_lock(task);
    esp_mcp_task_status_t status = task->status;
    task_unlock(task);
    return status;
}

char *esp_mcp_task_status_message(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, NULL, TAG, "Invalid task");
    task_lock(task);
    char *msg = task->status_message ? strdup(task->status_message) : NULL;
    task_unlock(task);
    return msg;
}

char *esp_mcp_task_result_json(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, NULL, TAG, "Invalid task");
    task_lock(task);
    char *result = task->result_json ? strdup(task->result_json) : NULL;
    task_unlock(task);
    return result;
}

bool esp_mcp_task_get_cancel_requested(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, false, TAG, "Invalid task");
    return __atomic_load_n(&task->cancel_requested, __ATOMIC_ACQUIRE);
}

esp_err_t esp_mcp_task_set_status(esp_mcp_task_t *task, esp_mcp_task_status_t status, const char *status_message)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");
    task_lock(task);
    // Cancellation has higher priority than success/failure when observed before state commit.
    if (__atomic_load_n(&task->cancel_requested, __ATOMIC_ACQUIRE) &&
            (status == ESP_MCP_TASK_COMPLETED || status == ESP_MCP_TASK_FAILED) &&
            (task->status == ESP_MCP_TASK_WORKING || task->status == ESP_MCP_TASK_CANCELLING)) {
        status = ESP_MCP_TASK_CANCELLED;
        status_message = "Cancelled";
    }
    if (!esp_mcp_task_status_can_transit(task->status, status)) {
        ESP_LOGW(TAG, "Invalid task state transition: %d -> %d", task->status, status);
        task_unlock(task);
        return ESP_ERR_INVALID_STATE;
    }
    char *new_msg = dup_or_null(status_message);
    if (status_message && !new_msg) {
        task_unlock(task);
        return ESP_ERR_NO_MEM;
    }
    free(task->status_message);
    task->status_message = new_msg;
    task->status = status;

    time_t now = time(NULL);
    if (now >= 1577836800) {
        task->last_updated_at = (int64_t)now;
    }

    esp_mcp_task_store_t *store = task->store;
    esp_mcp_task_status_cb_t cb = NULL;
    void *cb_ctx = NULL;
    char *task_id_copy = strdup(task->task_id);
    char *msg_copy = task->status_message ? strdup(task->status_message) : NULL;
    char *session_copy = task->origin_session_id[0] ? strdup(task->origin_session_id) : NULL;
    esp_mcp_task_status_t status_copy = status;
    task_unlock(task);

    if (store && store->mutex &&
            xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
        cb = store->status_cb;
        cb_ctx = store->status_cb_ctx;
        xSemaphoreGive(store->mutex);
    }

    if (cb && task_id_copy) {
        cb(task_id_copy, status_copy, msg_copy, session_copy, cb_ctx);
    }
    free(task_id_copy);
    free(msg_copy);
    free(session_copy);

    return ESP_OK;
}

esp_err_t esp_mcp_task_set_result_json(esp_mcp_task_t *task, const char *result_json, bool is_error_response)
{
    ESP_RETURN_ON_FALSE(task && result_json, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    task_lock(task);
    char *new_result = strdup(result_json);
    if (!new_result) {
        task_unlock(task);
        return ESP_ERR_NO_MEM;
    }
    free(task->result_json);
    task->result_json = new_result;
    task->result_is_error_response = is_error_response;
    task_unlock(task);
    return ESP_OK;
}

bool esp_mcp_task_result_is_error_response(const esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, false, TAG, "Invalid task");
    task_lock(task);
    bool is_error_response = task->result_is_error_response;
    task_unlock(task);
    return is_error_response;
}

esp_err_t esp_mcp_task_request_cancel(esp_mcp_task_t *task)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");
    task_lock(task);
    if (task->status == ESP_MCP_TASK_COMPLETED ||
            task->status == ESP_MCP_TASK_FAILED ||
            task->status == ESP_MCP_TASK_CANCELLED) {
        task_unlock(task);
        return ESP_ERR_INVALID_STATE;
    }

    if (task->status == ESP_MCP_TASK_WORKING) {
        __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
        task_unlock(task);
        return esp_mcp_task_set_status(task, ESP_MCP_TASK_CANCELLING, "Cancelling");
    }

    if (task->status == ESP_MCP_TASK_CANCELLING) {
        __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
        task_unlock(task);
        return ESP_OK;
    }

    __atomic_store_n(&task->cancel_requested, true, __ATOMIC_RELEASE);
    task_unlock(task);
    return esp_mcp_task_set_status(task, ESP_MCP_TASK_CANCELLED, "Cancelled");
}

bool esp_mcp_task_checkpoint_cancelled(esp_mcp_task_t *task, const char *checkpoint)
{
    ESP_RETURN_ON_FALSE(task, true, TAG, "Invalid task");
    if (!esp_mcp_task_get_cancel_requested(task)) {
        return false;
    }
    (void)checkpoint;
    esp_err_t ret = esp_mcp_task_set_status(task, ESP_MCP_TASK_CANCELLED, "Cancelled");
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to apply cancelled checkpoint status: %s", esp_err_to_name(ret));
    }
    return true;
}

esp_err_t esp_mcp_task_set_ttl(esp_mcp_task_t *task, int64_t ttl_ms)
{
    ESP_RETURN_ON_FALSE(task, ESP_ERR_INVALID_ARG, TAG, "Invalid task");
    task_lock(task);
    if (ttl_ms >= 0) {
        task->has_ttl = true;
        task->ttl = ttl_ms;
    } else {
        task->has_ttl = false;
        task->ttl = 0;
    }
    task_unlock(task);
    return ESP_OK;
}

int64_t esp_mcp_task_get_created_at(const esp_mcp_task_t *task)
{
    if (!task) {
        return 0;
    }
    task_lock(task);
    int64_t v = task->created_at;
    task_unlock(task);
    return v;
}

int64_t esp_mcp_task_get_last_updated_at(const esp_mcp_task_t *task)
{
    if (!task) {
        return 0;
    }
    task_lock(task);
    int64_t v = task->last_updated_at;
    task_unlock(task);
    return v;
}

bool esp_mcp_task_has_ttl(const esp_mcp_task_t *task)
{
    if (!task) {
        return false;
    }
    task_lock(task);
    bool has = task->has_ttl;
    task_unlock(task);
    return has;
}

int64_t esp_mcp_task_get_ttl(const esp_mcp_task_t *task)
{
    if (!task) {
        return 0;
    }
    task_lock(task);
    int64_t ttl = task->has_ttl ? task->ttl : 0;
    task_unlock(task);
    return ttl;
}

size_t esp_mcp_task_store_cleanup_expired(esp_mcp_task_store_t *store)
{
    if (!store || !store->mutex) {
        return 0;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;

    size_t removed = 0;
    while (xSemaphoreTake(store->mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        esp_mcp_task_t *task = NULL;
        esp_mcp_task_t *next = NULL;
        esp_mcp_task_t *expired = NULL;

        SLIST_FOREACH_SAFE(task, &store->tasks, next, next) {
            bool should_remove = false;

            task_lock(task);
            if (task->has_ttl && task->created_monotonic_ms > 0 && !task->worker_running) {
                int64_t elapsed_ms = now_ms - task->created_monotonic_ms;
                if (elapsed_ms >= task->ttl) {
                    should_remove = true;
                }
            }
            task_unlock(task);

            if (should_remove) {
                SLIST_REMOVE(&store->tasks, task, esp_mcp_task_s, next);
                expired = task;
                break;
            }
        }

        xSemaphoreGive(store->mutex);

        if (!expired) {
            break;
        }

        task_destroy(expired);
        removed++;
    }

    return removed;
}
