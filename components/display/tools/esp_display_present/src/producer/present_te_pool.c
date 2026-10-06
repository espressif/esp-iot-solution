/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GRAM ownership for TE compose buffers:
 *   commit(A, B): A → free, B inflight (DMA reads B)
 *   DMA-done ISR: B becomes baseline; B is not freed until the next submit
 */

#include "present_te_pool.h"

#include <stdlib.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_display_present.h"
#include "esp_log.h"

static const char *TAG = "present_te_pool";
static const uint32_t TE_POOL_ACQUIRE_POLL_MS = 10;

static bool buffer_on_queues(present_te_pool_t *pool, void *buffer)
{
    present_te_buf_t *elem;

    if (pool == NULL || buffer == NULL) {
        return false;
    }
    STAILQ_FOREACH(elem, &pool->inflight_queue, entry) {
        if (elem->buffer == buffer) {
            return true;
        }
    }
    STAILQ_FOREACH(elem, &pool->free_queue, entry) {
        if (elem->buffer == buffer) {
            return true;
        }
    }
    return false;
}

esp_err_t present_te_pool_init(
    present_te_pool_t *pool,
    void **buffers,
    uint8_t buffer_count,
    uint32_t acquire_timeout_ms)
{
    if (pool == NULL || buffers == NULL || buffer_count < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(pool, 0, sizeof(*pool));
    pool->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    pool->acquire_timeout_ms =
        acquire_timeout_ms != 0
        ? acquire_timeout_ms
        : ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS;
    STAILQ_INIT(&pool->inflight_queue);
    STAILQ_INIT(&pool->free_queue);

    pool->avail_sem = xSemaphoreCreateCounting(buffer_count, 0);
    if (pool->avail_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    pool->elem_count = buffer_count;
    pool->elems = calloc(buffer_count, sizeof(*pool->elems));
    if (pool->elems == NULL) {
        vSemaphoreDelete(pool->avail_sem);
        pool->avail_sem = NULL;
        return ESP_ERR_NO_MEM;
    }

    for (uint8_t i = 0; i < buffer_count; ++i) {
        pool->elems[i].buffer = buffers[i];
    }
    /* buffers[0] is draw, buffers[1] is display; extras start free. */
    for (uint8_t i = 2; i < buffer_count; ++i) {
        STAILQ_INSERT_TAIL(&pool->free_queue, &pool->elems[i], entry);
        (void)xSemaphoreGive(pool->avail_sem);
    }
    return ESP_OK;
}

void present_te_pool_deinit(present_te_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }
    free(pool->elems);
    pool->elems = NULL;
    pool->elem_count = 0;
    if (pool->avail_sem != NULL) {
        vSemaphoreDelete(pool->avail_sem);
        pool->avail_sem = NULL;
    }
    STAILQ_INIT(&pool->inflight_queue);
    STAILQ_INIT(&pool->free_queue);
}

bool present_te_pool_enabled(const present_te_pool_t *pool)
{
    return pool != NULL && pool->elems != NULL;
}

bool present_te_pool_commit(
    present_te_pool_t *pool,
    void *old_display,
    void *new_display,
    uint64_t ticket)
{
    if (!present_te_pool_enabled(pool) || old_display == NULL ||
            new_display == NULL || ticket == 0 ||
            old_display == new_display) {
        return false;
    }

    portENTER_CRITICAL(&pool->lock);
    if (buffer_on_queues(pool, old_display) ||
            buffer_on_queues(pool, new_display)) {
        portEXIT_CRITICAL(&pool->lock);
        return false;
    }

    present_te_buf_t *retired = NULL;
    present_te_buf_t *submitted = NULL;
    for (uint8_t i = 0; i < pool->elem_count; ++i) {
        if (pool->elems[i].buffer == old_display) {
            retired = &pool->elems[i];
        } else if (pool->elems[i].buffer == new_display) {
            submitted = &pool->elems[i];
        }
    }
    if (retired == NULL || submitted == NULL) {
        portEXIT_CRITICAL(&pool->lock);
        return false;
    }
    submitted->new_display_buffer = new_display;
    submitted->ticket = ticket;
    STAILQ_INSERT_TAIL(&pool->inflight_queue, submitted, entry);
    STAILQ_INSERT_TAIL(&pool->free_queue, retired, entry);
    SemaphoreHandle_t sem = pool->avail_sem;
    portEXIT_CRITICAL(&pool->lock);

    if (sem != NULL) {
        (void)xSemaphoreGive(sem);
    }
    return true;
}

bool present_te_pool_cancel_commit(
    present_te_pool_t *pool,
    void *old_display,
    void *new_display)
{
    if (!present_te_pool_enabled(pool) || old_display == NULL ||
            new_display == NULL || pool->avail_sem == NULL) {
        return false;
    }
    present_te_buf_t *retired = NULL;
    present_te_buf_t *submitted = NULL;
    portENTER_CRITICAL(&pool->lock);
    STAILQ_FOREACH(submitted, &pool->inflight_queue, entry) {
        if (submitted->buffer == new_display) {
            break;
        }
    }
    STAILQ_FOREACH(retired, &pool->free_queue, entry) {
        if (retired->buffer == old_display) {
            break;
        }
    }
    if (retired == NULL || submitted == NULL) {
        portEXIT_CRITICAL(&pool->lock);
        return false;
    }
    STAILQ_REMOVE(&pool->inflight_queue, submitted, present_te_buf, entry);
    STAILQ_REMOVE(&pool->free_queue, retired, present_te_buf, entry);
    submitted->new_display_buffer = NULL;
    submitted->ticket = 0;
    portEXIT_CRITICAL(&pool->lock);

    return xSemaphoreTake(pool->avail_sem, 0) == pdTRUE;
}

static present_te_buf_t *take_free(present_te_pool_t *pool)
{
    present_te_buf_t *next = NULL;
    portENTER_CRITICAL(&pool->lock);
    next = STAILQ_FIRST(&pool->free_queue);
    if (next != NULL) {
        STAILQ_REMOVE_HEAD(&pool->free_queue, entry);
    }
    portEXIT_CRITICAL(&pool->lock);
    return next;
}

esp_err_t present_te_pool_acquire_next(
    present_te_pool_t *pool,
    const bool *closing,
    void **out_buffer)
{
    if (pool == NULL || out_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_buffer = NULL;
    if (pool->avail_sem == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t waited = 0;
    const TickType_t poll_ticks = pdMS_TO_TICKS(TE_POOL_ACQUIRE_POLL_MS);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(pool->acquire_timeout_ms);
    while (*out_buffer == NULL) {
        if (closing != NULL && *closing) {
            return ESP_ERR_INVALID_STATE;
        }
        if (waited >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        TickType_t wait_ticks = poll_ticks;
        if (timeout_ticks - waited < wait_ticks) {
            wait_ticks = timeout_ticks - waited;
        }
        if (xSemaphoreTake(pool->avail_sem, wait_ticks) != pdTRUE) {
            waited += wait_ticks;
            continue;
        }
        present_te_buf_t *next = take_free(pool);
        if (next == NULL) {
            ESP_LOGW(TAG, "TE pool semaphore signaled without free buffer");
            waited += wait_ticks;
            continue;
        }
        *out_buffer = next->buffer;
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

bool IRAM_ATTR present_te_pool_retire_isr(
    present_te_pool_t *pool,
    void **out_new_display,
    uint64_t *out_ticket)
{
    present_te_buf_t *elem = NULL;

    if (!present_te_pool_enabled(pool)) {
        return false;
    }

    portENTER_CRITICAL_ISR(&pool->lock);
    elem = STAILQ_FIRST(&pool->inflight_queue);
    if (elem != NULL) {
        STAILQ_REMOVE_HEAD(&pool->inflight_queue, entry);
        if (out_new_display != NULL) {
            *out_new_display = elem->new_display_buffer;
        }
        if (out_ticket != NULL) {
            *out_ticket = elem->ticket;
        }
        elem->new_display_buffer = NULL;
        elem->ticket = 0;
        /* B is now the software/display baseline. It stays off both queues
         * until the next submit frees it. */
    }
    portEXIT_CRITICAL_ISR(&pool->lock);
    return elem != NULL;
}

bool present_te_pool_has_inflight(const present_te_pool_t *pool)
{
    if (!present_te_pool_enabled(pool)) {
        return false;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&pool->lock);
    bool pending = STAILQ_FIRST(&pool->inflight_queue) != NULL;
    portEXIT_CRITICAL((portMUX_TYPE *)&pool->lock);
    return pending;
}
