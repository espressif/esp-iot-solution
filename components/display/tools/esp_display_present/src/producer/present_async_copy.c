/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Tile → framebuffer copy worker. The worker runs the same synchronous blit
 * (DMA2D / PPA / memcpy + cache maintenance) as the inline path; overlap
 * comes from the producer rendering into the other drawbuf. Frame-level
 * stages (repair, commit/push, cancel) drain the worker first, so the
 * destination framebuffer is never touched by two agents at once.
 */

#include "present_async_copy.h"

#include <string.h>

#include "esp_log.h"
#include "present_rgb565_swap.h"
#include "present_target_internal.h"

#define PRESENT_ASYNC_COPY_STACK_BYTES 4096

static const char *TAG = "present_copy";

typedef struct {
    esp_display_present_target_t *target;
    esp_display_present_blit_fb_t dst;
    esp_display_presenter_region_t region;
    uint8_t slot;
    bool swap_rgb565;
} present_async_copy_job_t;

static void async_copy_task(void *arg)
{
    present_async_copy_t *copy = arg;
    present_async_copy_job_t job;
    for (;;) {
        if (xQueueReceive(copy->jobs, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (job.swap_rgb565) {
            present_rgb565_swap_in_place(
                job.region.surface.pixels,
                (size_t)job.region.surface.width * job.region.surface.height);
        }
        esp_err_t ret = present_target_blit_region(job.target, &job.dst,
                                                   &job.region);
        if (ret != ESP_OK && copy->error == ESP_OK) {
            copy->error = ret;
        }
        (void)xSemaphoreGive(copy->idle[job.slot]);
    }
}

static int slot_of(const present_async_copy_t *copy, const void *pixels)
{
    for (uint8_t index = 0; index < copy->pool->count; ++index) {
        if (copy->pool->buffers[index] == pixels) {
            return index;
        }
    }
    return -1;
}

esp_err_t present_async_copy_init(present_async_copy_t *copy,
                                  const esp_display_present_drawbuf_pool_t *pool,
                                  bool allow)
{
    if (copy == NULL || pool == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(copy, 0, sizeof(*copy));
    copy->pool = pool;
    if (!allow || portNUM_PROCESSORS < 2 || pool->count < 2 ||
            pool->count > ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS) {
        return ESP_OK;
    }
    copy->jobs = xQueueCreate(pool->count, sizeof(present_async_copy_job_t));
    if (copy->jobs == NULL) {
        /* The worker is an optional overlap optimization. Keep the normal
         * synchronous blit path available when its queue cannot be created. */
        ESP_LOGW(TAG, "no async-copy queue; tile copies stay inline");
        return ESP_OK;
    }
    for (uint8_t index = 0; index < pool->count; ++index) {
        copy->idle[index] =
            xSemaphoreCreateBinaryStatic(&copy->idle_storage[index]);
    }
    copy->enabled = true;
    return ESP_OK;
}

void present_async_copy_deinit(present_async_copy_t *copy)
{
    if (copy == NULL) {
        return;
    }
    (void)present_async_copy_drain(copy);
    if (copy->task != NULL) {
        vTaskDelete(copy->task);
    }
    if (copy->jobs != NULL) {
        vQueueDelete(copy->jobs);
    }
    for (uint8_t index = 0; index < ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS;
            ++index) {
        if (copy->idle[index] != NULL) {
            vSemaphoreDelete(copy->idle[index]);
        }
    }
    memset(copy, 0, sizeof(*copy));
}

void present_async_copy_wait_slot(present_async_copy_t *copy, uint8_t slot)
{
    if (copy == NULL || slot >= ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS ||
            !copy->busy[slot]) {
        return;
    }
    /* The blit bounds its own hardware waits, so the worker always gives. */
    (void)xSemaphoreTake(copy->idle[slot], portMAX_DELAY);
    copy->busy[slot] = false;
}

static esp_err_t ensure_task(present_async_copy_t *copy)
{
    if (copy->task != NULL) {
        return ESP_OK;
    }
    /* Created from the first producer submit so the worker inherits the
     * render task priority instead of the presenter creator's. A failure
     * (tight internal RAM) keeps every later copy inline. */
    if (xTaskCreate(async_copy_task, "present_copy",
                    PRESENT_ASYNC_COPY_STACK_BYTES, copy,
                    uxTaskPriorityGet(NULL), &copy->task) != pdPASS) {
        copy->task = NULL;
        copy->enabled = false;
        ESP_LOGW(TAG, "no worker task; tile copies stay inline");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t present_async_copy_submit(present_async_copy_t *copy,
                                    esp_display_present_target_t *target,
                                    const esp_display_present_blit_fb_t *dst,
                                    const esp_display_presenter_region_t *region,
                                    bool swap_rgb565)
{
    if (copy == NULL || !copy->enabled) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    int slot = slot_of(copy, region->surface.pixels);
    if (slot < 0 || copy->busy[slot] || ensure_task(copy) != ESP_OK) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const present_async_copy_job_t job = {
        .target = target,
        .dst = *dst,
        .region = *region,
        .slot = (uint8_t)slot,
        .swap_rgb565 = swap_rgb565,
    };
    copy->busy[slot] = true;
    if (xQueueSend(copy->jobs, &job, 0) != pdTRUE) {
        copy->busy[slot] = false;
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

esp_err_t present_async_copy_drain(present_async_copy_t *copy)
{
    if (copy == NULL || copy->task == NULL) {
        return ESP_OK;
    }
    for (uint8_t index = 0; index < ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS;
            ++index) {
        present_async_copy_wait_slot(copy, index);
    }
    esp_err_t ret = copy->error;
    copy->error = ESP_OK;
    return ret;
}
