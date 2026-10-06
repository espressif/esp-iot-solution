/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GRAM partition transport: lease DMA draw buffers and commit them straight
 * to panel GRAM. The buffers are released only by transfer-done callbacks, so
 * direct-to-GRAM drawing can pipeline within one frame without reusing an
 * in-flight DMA source.
 */

#include "present_gram_internal.h"
#include "present_mode_internal.h"
#include "present_rgb565_swap.h"
#include "present_transfer_wait.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_display_present_cache.h"
#include "esp_display_present_panel.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "present_gram";

static int buffer_index(const esp_display_present_gram_endpoint_t *ctx,
                        const void *pixels)
{
    for (uint8_t index = 0; index < ctx->buffer_count; ++index) {
        if (ctx->drawbuf_pool.buffers[index] == pixels) {
            return (int)index;
        }
    }
    return -1;
}

static bool buffer_is_inflight(const esp_display_present_gram_endpoint_t *ctx,
                               uint8_t index)
{
    for (uint8_t n = 0; n < ctx->inflight_count; ++n) {
        uint8_t slot = (uint8_t)((ctx->inflight_head + n) %
                                 ctx->buffer_count);
        if (ctx->inflight[slot].buffer_index == index) {
            return true;
        }
    }
    return false;
}

static int find_free_buffer(const esp_display_present_gram_endpoint_t *ctx)
{
    for (uint8_t index = 0; index < ctx->buffer_count; ++index) {
        if (!ctx->held[index]) {
            return (int)index;
        }
    }
    return -1;
}

static void swap_rgb565(const esp_display_presenter_region_t *region)
{
    present_rgb565_swap_in_place(
        region->surface.pixels,
        (size_t)region->surface.width * region->surface.height);
}

esp_err_t esp_display_present_gram_create(
    esp_display_present_target_t *target,
    const esp_display_present_drawbuf_pool_t *drawbuf_pool,
    uint32_t transfer_timeout_ms,
    esp_display_present_gram_endpoint_t **out_endpoint)
{
    if (out_endpoint != NULL) {
        *out_endpoint = NULL;
    }
    const esp_display_present_target_info_t *info =
        esp_display_present_target_get_info(target);
    uint8_t buffer_count = info != NULL && info->drawbuf.lines != 0
                           ? (info->drawbuf.buffers == 1 ? 1 : 2) : 0;
    ESP_RETURN_ON_FALSE(info && out_endpoint && info->hw.panel &&
                        info->drawbuf.lines != 0 &&
                        buffer_count != 0 &&
                        buffer_count <=
                        ESP_DISPLAY_PRESENT_DRAWBUF_MAX_BUFFERS &&
                        drawbuf_pool != NULL &&
                        drawbuf_pool->count == buffer_count &&
                        drawbuf_pool->lines == info->drawbuf.lines &&
                        drawbuf_pool->bytes != 0 &&
                        info->hw.color_bytes != 0 &&
                        (!info->hw.swap_bytes || info->hw.color_bytes == 2),
                        ESP_ERR_INVALID_ARG, TAG, "invalid GRAM target");
    for (uint8_t index = 0; index < buffer_count; ++index) {
        ESP_RETURN_ON_FALSE(drawbuf_pool->buffers[index] != NULL,
                            ESP_ERR_INVALID_ARG, TAG, "invalid drawbuf pool");
    }

    esp_display_present_gram_endpoint_t *ctx = heap_caps_calloc(
                                                   1, sizeof(*ctx), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "no GRAM state");
    ctx->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ctx->panel = info->hw.panel;
    ctx->width = info->hw.width;
    ctx->height = info->hw.height;
    ctx->lines = drawbuf_pool->lines;
    ctx->buffer_count = buffer_count;
    ctx->color_bytes = info->hw.color_bytes;
    ctx->swap_bytes = info->hw.swap_bytes;
    ctx->available = xSemaphoreCreateCountingStatic(
                         ctx->buffer_count, ctx->buffer_count, &ctx->available_storage);
    if (ctx->available == NULL) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    ctx->drawbuf_pool = *drawbuf_pool;
    ctx->drawbuf_bytes = ctx->drawbuf_pool.bytes;
    ctx->transfer_timeout_ms =
        transfer_timeout_ms != 0
        ? transfer_timeout_ms
        : ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS;
    esp_display_present_tracker_init_frame(&ctx->tracker, NULL, true);
    ctx->life = GRAM_RUNNING;
    *out_endpoint = ctx;
    ESP_LOGI(TAG, "ready: %u x %u-line GRAM drawbufs",
             (unsigned)ctx->buffer_count, (unsigned)ctx->lines);
    return ESP_OK;
}

esp_display_present_frame_tracker_t *esp_display_present_gram_get_tracker(
    esp_display_present_gram_endpoint_t *endpoint)
{
    return endpoint != NULL ? &endpoint->tracker : NULL;
}

esp_err_t esp_display_present_gram_stop(
    esp_display_present_gram_endpoint_t *ctx)
{
    if (ctx == NULL || ctx->life == GRAM_STOPPED) {
        return ctx == NULL ? ESP_ERR_INVALID_ARG : ESP_OK;
    }
    portENTER_CRITICAL(&ctx->lock);
    if (ctx->life == GRAM_STOPPED) {
        portEXIT_CRITICAL(&ctx->lock);
        return ESP_OK;
    }
    ctx->life = GRAM_CLOSING;
    bool idle = ctx->inflight_count == 0;
    for (uint8_t index = 0; index < ctx->buffer_count; ++index) {
        idle = idle && !ctx->held[index];
    }
    portEXIT_CRITICAL(&ctx->lock);
    if (!idle) {
        /* Wake a producer blocked in acquire so it can observe CLOSING. */
        (void)xSemaphoreGive(ctx->available);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t session_ret = esp_display_present_tracker_stop(&ctx->tracker);
    if (session_ret == ESP_ERR_INVALID_STATE) {
        /* Hardware is idle, so only failed-frame bookkeeping can remain. */
        session_ret = esp_display_present_tracker_force_stop(&ctx->tracker);
    }
    if (session_ret != ESP_OK) {
        return session_ret;
    }
    portENTER_CRITICAL(&ctx->lock);
    ctx->life = GRAM_STOPPED;
    portEXIT_CRITICAL(&ctx->lock);
    return ESP_OK;
}

esp_err_t esp_display_present_gram_delete(
    esp_display_present_gram_endpoint_t *ctx)
{
    esp_err_t ret = esp_display_present_gram_stop(ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_display_present_gram_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region)
{
    esp_display_present_gram_endpoint_t *gram =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (gram == NULL || area == NULL || out_region == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t row_bytes = (size_t)(area->x2 - area->x1 + 1) * gram->color_bytes;
    uint16_t requested_rows = (uint16_t)(area->y2 - area->y1 + 1);
    if (gram->life == GRAM_STOPPED || row_bytes == 0 ||
            requested_rows == 0 || row_bytes > gram->drawbuf_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t rows = (uint16_t)(gram->drawbuf_bytes / row_bytes);
    if (rows > requested_rows) {
        rows = requested_rows;
    }
    uint16_t row_alignment = ctx->render_alignment.height_pixels != 0
                             ? ctx->render_alignment.height_pixels : 1;
    if (rows < requested_rows && row_alignment > 1) {
        rows = (uint16_t)(rows - rows % row_alignment);
    }
    if (rows == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&gram->lock);
    if (gram->life != GRAM_RUNNING) {
        portEXIT_CRITICAL(&gram->lock);
        return ESP_ERR_INVALID_STATE;
    }
    portEXIT_CRITICAL(&gram->lock);

    if (xSemaphoreTake(gram->available, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    portENTER_CRITICAL(&gram->lock);
    if (gram->life != GRAM_RUNNING) {
        portEXIT_CRITICAL(&gram->lock);
        (void)xSemaphoreGive(gram->available);
        return ESP_ERR_INVALID_STATE;
    }
    int index = find_free_buffer(gram);
    if (index < 0) {
        portEXIT_CRITICAL(&gram->lock);
        (void)xSemaphoreGive(gram->available);
        return ESP_ERR_INVALID_STATE;
    }
    gram->held[index] = true;
    portEXIT_CRITICAL(&gram->lock);
    *out_region = (esp_display_presenter_region_t) {
        .surface = {
            .pixels = gram->drawbuf_pool.buffers[index],
            .stride_bytes = row_bytes,
            .width = (uint16_t)(area->x2 - area->x1 + 1),
            .height = rows,
            .pixel_format = pixel_format,
        },
        .origin_x = (uint16_t)area->x1,
        .origin_y = (uint16_t)area->y1,
    };
    return ESP_OK;
}

esp_err_t esp_display_present_gram_cancel_drawbuf(
    present_frame_ctx_t *ctx,
    void *pixels)
{
    esp_display_present_gram_endpoint_t *gram =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (gram == NULL || gram->life == GRAM_STOPPED || pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int index = buffer_index(gram, pixels);
    if (index < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&gram->lock);
    if (!gram->held[index]) {
        portEXIT_CRITICAL(&gram->lock);
        return ESP_OK;
    }
    if (buffer_is_inflight(gram, (uint8_t)index)) {
        portEXIT_CRITICAL(&gram->lock);
        return ESP_ERR_INVALID_STATE;
    }
    gram->held[index] = false;
    portEXIT_CRITICAL(&gram->lock);
    (void)xSemaphoreGive(gram->available);
    return ESP_OK;
}

static esp_err_t gram_commit_region(
    present_frame_ctx_t *ctx,
    esp_display_present_gram_endpoint_t *gram,
    const esp_display_presenter_region_t *region)
{
    int index = buffer_index(gram, region->surface.pixels);
    if (index < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    const uint64_t frame_id = ctx->frame_id;
    uint64_t previous_frame = 0;
    uint64_t building_frame =
        esp_display_present_tracker_get_building_frame(ctx->tracker);
    bool start_new = building_frame == 0;
    esp_err_t ret = start_new
                    ? esp_display_present_tracker_begin_build(
                        ctx->tracker, frame_id, xTaskGetCurrentTaskHandle(),
                        &previous_frame)
                    : esp_display_present_tracker_continue_build(
                        ctx->tracker, frame_id, xTaskGetCurrentTaskHandle());
    if (ret != ESP_OK) {
        return ret;
    }

    portENTER_CRITICAL(&gram->lock);
    if (gram->life != GRAM_RUNNING || !gram->held[index] ||
            buffer_is_inflight(gram, (uint8_t)index) ||
            gram->inflight_count >= gram->buffer_count) {
        portEXIT_CRITICAL(&gram->lock);
        if (start_new) {
            esp_display_present_tracker_rollback_frame(
                ctx->tracker, frame_id, previous_frame, false);
        }
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t tail = (uint8_t)((gram->inflight_head + gram->inflight_count) %
                             gram->buffer_count);
    gram->inflight[tail] = (gram_transfer_t) {
        .frame_id = frame_id,
        .buffer_index = (uint8_t)index,
    };
    ++gram->inflight_count;
    portEXIT_CRITICAL(&gram->lock);

    if (gram->swap_bytes) {
        swap_rgb565(region);
    }
    esp_display_present_cache_msync_framebuffer(
        region->surface.pixels,
        region->surface.stride_bytes * region->surface.height);
    if (start_new) {
        ret = esp_display_present_tracker_publish_frame(ctx->tracker, frame_id, &previous_frame);
    }
    if (ret == ESP_OK) {
        ret = esp_display_present_tracker_submit_transfer_ticket(ctx->tracker);
    }
    if (ret == ESP_OK) {
        ret = esp_display_present_blit_area(
                  gram->panel,
                  (int)region->origin_x,
                  (int)region->origin_y,
                  (int)region->origin_x + (int)region->surface.width,
                  (int)region->origin_y + (int)region->surface.height,
                  region->surface.pixels);
        if (ret != ESP_OK) {
            esp_display_present_tracker_cancel_transfer_ticket(ctx->tracker);
        }
    }
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    /* Contract: esp_display_present_blit_area() reports an error only before
     * the panel driver accepts the DMA source. Once accepted, completion must
     * arrive through the ISR path and the buffer stays in-flight. */
    esp_display_present_tracker_rollback_frame(
        ctx->tracker, frame_id, previous_frame, !start_new);
    portENTER_CRITICAL(&gram->lock);
    if (gram->inflight_count != 0) {
        uint8_t rollback_tail = (uint8_t)(
                                    (gram->inflight_head + gram->inflight_count - 1U) %
                                    gram->buffer_count);
        if (gram->inflight[rollback_tail].buffer_index == (uint8_t)index) {
            gram->inflight[rollback_tail] = (gram_transfer_t) {
                0
            };
            --gram->inflight_count;
        }
    }
    gram->held[index] = false;
    portEXIT_CRITICAL(&gram->lock);
    (void)xSemaphoreGive(gram->available);
    ESP_LOGE(TAG, "submit failed: ticket=%" PRIu64 " y=%u: %s",
             frame_id, (unsigned)region->origin_y, esp_err_to_name(ret));
    return ret;
}

esp_err_t esp_display_present_gram_submit_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region)
{
    esp_display_present_gram_endpoint_t *gram =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (gram == NULL || gram->life == GRAM_STOPPED || region == NULL ||
            region->surface.pixels == NULL || frame_id == 0 ||
            region->surface.width == 0 || region->surface.height == 0 ||
            (uint32_t)region->origin_x + region->surface.width > gram->width ||
            (uint32_t)region->origin_y + region->surface.height >
            gram->height ||
            region->surface.stride_bytes !=
            (size_t)region->surface.width * gram->color_bytes ||
            region->surface.stride_bytes * region->surface.height >
            gram->drawbuf_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    return gram_commit_region(ctx, gram, region);
}

esp_err_t esp_display_present_gram_commit_frame(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit)
{
    esp_display_present_gram_endpoint_t *gram =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    (void)submit;
    if (gram == NULL || gram->life == GRAM_STOPPED || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_display_present_tracker_get_building_frame(ctx->tracker) !=
            frame_id) {
        return ESP_ERR_INVALID_STATE;
    }
    if (present_transfer_wait_idle(ctx->tracker,
                                   gram->transfer_timeout_ms) != ESP_OK) {
        ESP_LOGE(TAG,
                 "GRAM transfer timed out; retaining in-flight drawbufs");
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t ret = esp_display_present_tracker_end_frame(
                        ctx->tracker, frame_id);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_display_present_tracker_complete_frame(
               ctx->tracker, frame_id, true, true);
}

void esp_display_present_gram_cancel_frame(present_frame_ctx_t *ctx)
{
    esp_display_present_gram_endpoint_t *gram =
        ctx != NULL ? ctx->mode_ctx : NULL;
    uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (gram == NULL || frame_id == 0 ||
            esp_display_present_tracker_get_building_frame(ctx->tracker) !=
            frame_id) {
        return;
    }

    if (present_transfer_wait_idle(ctx->tracker,
                                   gram->transfer_timeout_ms) != ESP_OK) {
        esp_display_present_tracker_rollback_frame(
            ctx->tracker, frame_id, 0, true);
        return;
    }

    if (esp_display_present_tracker_end_frame(ctx->tracker, frame_id) != ESP_OK ||
            esp_display_present_tracker_complete_frame(
                ctx->tracker, frame_id, true, true) != ESP_OK) {
        esp_display_present_tracker_rollback_frame(
            ctx->tracker, frame_id, 0, true);
    }
}

bool IRAM_ATTR esp_display_present_gram_notify_transfer_done_from_isr(
    esp_display_present_gram_endpoint_t *gram)
{
    if (gram == NULL ||
            !esp_display_present_tracker_isr_enter(&gram->tracker)) {
        return false;
    }

    bool has_completed = false;
    uint8_t completed_index = 0;
    portENTER_CRITICAL_ISR(&gram->lock);
    if (gram->inflight_count != 0) {
        gram_transfer_t completed = gram->inflight[gram->inflight_head];
        gram->inflight[gram->inflight_head] = (gram_transfer_t) {
            0
        };
        gram->inflight_head = (uint8_t)(
                                  (gram->inflight_head + 1U) % gram->buffer_count);
        --gram->inflight_count;
        completed_index = completed.buffer_index;
        if (completed_index < gram->buffer_count) {
            gram->held[completed_index] = false;
        }
        has_completed = true;
    }
    portEXIT_CRITICAL_ISR(&gram->lock);

    BaseType_t need_yield = pdFALSE;
    if (has_completed) {
        (void)esp_display_present_tracker_complete_transfer_ticket_isr(
            &gram->tracker);
        xSemaphoreGiveFromISR(gram->available, &need_yield);
        esp_display_present_tracker_signal_completion_isr(
            &gram->tracker, &need_yield);
    }
    esp_display_present_tracker_isr_leave(&gram->tracker);
    return need_yield == pdTRUE;
}

uint64_t esp_display_present_gram_get_completed_frame(
    const esp_display_present_gram_endpoint_t *gram)
{
    return gram != NULL
           ? esp_display_present_tracker_get_completed_present_frame(
               &gram->tracker) : 0;
}

uint64_t esp_display_present_gram_get_last_submitted_frame(
    const esp_display_present_gram_endpoint_t *gram)
{
    return gram != NULL
           ? esp_display_present_tracker_get_last_submitted_frame(
               &gram->tracker) : 0;
}
