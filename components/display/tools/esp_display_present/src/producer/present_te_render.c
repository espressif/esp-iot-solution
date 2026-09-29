/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GRAM TE draw side: own one persistent full-screen draw buffer and blit
 * rendered tiles into it. The frame commit path (TE wait, one full-frame push
 * and transfer-done ISR) lives in
 * producer/stage/present_te_transport.c.
 */

#include "present_te_internal.h"
#include "present_mode_internal.h"
#include "present_rgb565_swap.h"
#include "present_target_internal.h"

#include <stdlib.h>
#include <string.h>

#include "esp_display_present_blit.h"
#include "esp_display_present_ppa.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "present_te";

static void release_buffers(esp_display_present_te_compose_t *te_compose)
{
    present_async_copy_deinit(&te_compose->async_copy);
    present_te_compose_repair_destroy(te_compose);
    present_te_pool_deinit(&te_compose->pool);
    if (te_compose->te_ctx != NULL) {
        esp_display_present_te_sync_destroy(te_compose->te_ctx);
    }
    esp_display_present_ppa_close_tile_client(&te_compose->ppa_handle);
    for (uint8_t index = 0; index < PRESENT_TE_COMPOSE_MAX_BUFFERS; ++index) {
        free(te_compose->draw.buffers[index]);
    }
}

esp_err_t esp_display_present_te_compose_create(
    esp_display_present_target_t *target,
    size_t max_damage_areas,
    const esp_display_present_drawbuf_pool_t *drawbuf_pool,
    uint32_t transfer_timeout_ms,
    uint32_t acquire_timeout_ms,
    esp_display_present_te_compose_t **out_endpoint)
{
    if (out_endpoint != NULL) {
        *out_endpoint = NULL;
    }
    const esp_display_present_target_info_t *info =
        esp_display_present_target_get_info(target);
    if (info == NULL || out_endpoint == NULL || max_damage_areas == 0 ||
            max_damage_areas > UINT16_MAX || !info->hw.panel_gram ||
            info->hw.color_bytes == 0 ||
            (info->hw.swap_bytes && info->hw.color_bytes != 2) ||
            info->drawbuf.lines == 0 ||
            drawbuf_pool == NULL || drawbuf_pool->count == 0 ||
            drawbuf_pool->bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_display_present_te_compose_t *te_compose = heap_caps_calloc(
                                                       1, sizeof(*te_compose), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (te_compose == NULL) {
        return ESP_ERR_NO_MEM;
    }
    te_compose->panel = info->hw.panel;
    te_compose->target = target;
    te_compose->rotation = info->hw.rotation;
    te_compose->logical_width = info->hw.width;
    te_compose->logical_height = info->hw.height;
    te_compose->max_damage_areas = max_damage_areas;
    te_compose->transfer_timeout_ms =
        transfer_timeout_ms != 0
        ? transfer_timeout_ms
        : ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS;
    te_compose->acquire_timeout_ms =
        acquire_timeout_ms != 0
        ? acquire_timeout_ms
        : ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS;
    te_compose->drawbuf_pool = *drawbuf_pool;
    te_compose->draw.width = info->hw.width;
    te_compose->draw.height = info->hw.height;
    if (info->hw.rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
            info->hw.rotation == ESP_DISPLAY_PRESENT_ROTATE_270) {
        te_compose->draw.width = info->hw.height;
        te_compose->draw.height = info->hw.width;
    }
    te_compose->draw.color_bytes = info->hw.color_bytes;
    te_compose->draw.stride_bytes =
        (size_t)te_compose->draw.width * info->hw.color_bytes;
    te_compose->draw.panel_byte_order = info->hw.swap_bytes;
    size_t draw_bytes = te_compose->draw.stride_bytes * te_compose->draw.height;
    te_compose->compose_buffer_count = info->drawbuf.te_compose_buffers;
    if (te_compose->compose_buffer_count < 1) {
        te_compose->compose_buffer_count = 1;
    }
    if (te_compose->compose_buffer_count > PRESENT_TE_COMPOSE_MAX_BUFFERS) {
        te_compose->compose_buffer_count = PRESENT_TE_COMPOSE_MAX_BUFFERS;
    }
    for (uint8_t index = 0; index < te_compose->compose_buffer_count; ++index) {
        te_compose->draw.buffers[index] = heap_caps_malloc(
                                              draw_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT |
                                              MALLOC_CAP_CACHE_ALIGNED);
        if (te_compose->draw.buffers[index] == NULL) {
            te_compose->draw.buffers[index] = heap_caps_malloc(
                                                  draw_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA |
                                                  MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
        }
        if (te_compose->draw.buffers[index] == NULL) {
            release_buffers(te_compose);
            free(te_compose);
            return ESP_ERR_NO_MEM;
        }
        memset(te_compose->draw.buffers[index], 0, draw_bytes);
    }
    te_compose->draw.pixels = te_compose->draw.buffers[0];
    if (te_compose->compose_buffer_count >= 2) {
        te_compose->display_buffer = te_compose->draw.buffers[1];
    }

    if (info->fb.profile.sync != ESP_DISPLAY_PRESENT_SYNC_TE) {
        release_buffers(te_compose);
        free(te_compose);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_display_present_te_sync_create(
                        &info->hw.te_sync, false, &te_compose->te_ctx);
    if (ret != ESP_OK) {
        release_buffers(te_compose);
        free(te_compose);
        return ret;
    }
    esp_display_present_tracker_init_frame(&te_compose->tracker, NULL, true);
    if (te_compose->compose_buffer_count >= 2) {
        ret = present_te_pool_init(
                  &te_compose->pool, (void **)te_compose->draw.buffers,
                  te_compose->compose_buffer_count,
                  te_compose->acquire_timeout_ms);
        if (ret == ESP_OK) {
            ret = present_te_compose_repair_create(te_compose);
        }
        if (ret != ESP_OK) {
            release_buffers(te_compose);
            free(te_compose);
            return ret;
        }
    }
    (void)esp_display_present_ppa_try_open_tile_client(te_compose->rotation,
                                                       &te_compose->ppa_handle);
    ret = present_async_copy_init(&te_compose->async_copy,
                                  &te_compose->drawbuf_pool, true);
    if (ret != ESP_OK) {
        release_buffers(te_compose);
        free(te_compose);
        return ret;
    }
    ESP_LOGI(TAG, "te_compose: logical=%ux%u physical=%ux%u rotate=%d ppa=%u buffers=%u pipeline=%u async_copy=%u",
             (unsigned)te_compose->logical_width,
             (unsigned)te_compose->logical_height,
             (unsigned)te_compose->draw.width,
             (unsigned)te_compose->draw.height,
             (int)te_compose->rotation,
             te_compose->ppa_handle != NULL ? 1U : 0U,
             (unsigned)te_compose->compose_buffer_count,
             (unsigned)present_te_pool_enabled(&te_compose->pool),
             (unsigned)te_compose->async_copy.enabled);
    *out_endpoint = te_compose;
    return ESP_OK;
}

esp_err_t esp_display_present_te_compose_stop(
    esp_display_present_te_compose_t *te_compose)
{
    if (te_compose == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (te_compose->stopped) {
        return ESP_OK;
    }
    if (esp_display_present_tracker_get_building_frame(
                &te_compose->tracker) != 0) {
        /* A queued tile may still be owned by the copy worker even though no
         * panel transfer ticket exists yet. The producer must cancel/drain
         * that frame before endpoint teardown can release its buffers. */
        return ESP_ERR_INVALID_STATE;
    }
    if (esp_display_present_tracker_get_pending_transfer_tickets(
                &te_compose->tracker) != 0 ||
            present_te_pool_has_inflight(&te_compose->pool)) {
        /* No panel abort primitive exists. Keep the buffers owned until the
         * real transfer-done callback proves that DMA no longer reads them. */
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_display_present_tracker_stop(&te_compose->tracker);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* A failed frame can leave logical fences queued. With no transfer
         * tickets left, hardware has stopped reading our buffers and those
         * bookkeeping-only fences are now safe to discard. */
        ret = esp_display_present_tracker_force_stop(&te_compose->tracker);
    }
    if (ret == ESP_OK) {
        te_compose->stopped = true;
    }
    return ret;
}

esp_err_t esp_display_present_te_compose_delete(
    esp_display_present_te_compose_t *te_compose)
{
    esp_err_t ret = esp_display_present_te_compose_stop(te_compose);
    if (ret != ESP_OK) {
        return ret;
    }
    release_buffers(te_compose);
    free(te_compose);
    return ESP_OK;
}

esp_err_t esp_display_present_te_compose_get_surface(
    const esp_display_present_te_compose_t *te_compose,
    esp_display_present_te_compose_surface_t *out_surface)
{
    if (te_compose == NULL || out_surface == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_surface = (esp_display_present_te_compose_surface_t) {
        .pixels = te_compose->draw.pixels,
        .stride_bytes = te_compose->draw.stride_bytes,
        .width = te_compose->draw.width,
        .height = te_compose->draw.height,
    };
    return ESP_OK;
}

esp_err_t esp_display_present_te_compose_begin(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count)
{
    esp_display_present_te_compose_t *te_compose =
        ctx != NULL ? ctx->mode_ctx : NULL;
    (void)out_render_areas;
    (void)render_area_capacity;
    (void)out_render_area_count;
    (void)request;
    if (te_compose == NULL || ctx->frame_id == 0 || te_compose->stopped ||
            te_compose->has_active_buffer) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_display_present_tracker_validate_frame(
                        &te_compose->tracker, ctx->frame_id,
                        xTaskGetCurrentTaskHandle());
    if (ret != ESP_OK || te_compose->compose_buffer_count == 1) {
        return ret;
    }
    if (te_compose->draw.pixels == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    bool known = false;
    for (uint8_t index = 0; index < te_compose->compose_buffer_count;
            ++index) {
        if (te_compose->draw.buffers[index] == te_compose->draw.pixels) {
            known = true;
            break;
        }
    }
    if (!known) {
        return ESP_ERR_INVALID_STATE;
    }
    te_compose->has_active_buffer = true;
    return ESP_OK;
}

esp_err_t esp_display_present_te_compose_validate_frame(
    esp_display_present_te_compose_t *te_compose,
    uint64_t frame_id)
{
    return te_compose != NULL
           ? esp_display_present_tracker_validate_frame(
               &te_compose->tracker, frame_id, xTaskGetCurrentTaskHandle())
           : ESP_ERR_INVALID_ARG;
}

esp_display_present_frame_tracker_t *esp_display_present_te_compose_get_tracker(
    esp_display_present_te_compose_t *te_compose)
{
    return te_compose != NULL ? &te_compose->tracker : NULL;
}

esp_err_t esp_display_present_te_compose_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region)
{
    esp_display_present_te_compose_t *te_compose =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (te_compose == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    present_async_copy_wait_slot(&te_compose->async_copy,
                                 te_compose->next_drawbuf);
    return present_stage_acquire_tile(
               &te_compose->drawbuf_pool, &te_compose->next_drawbuf,
               te_compose->draw.color_bytes, area, pixel_format, out_region);
}

void esp_display_present_te_compose_cancel_frame(present_frame_ctx_t *ctx)
{
    esp_display_present_te_compose_t *te_compose =
        ctx != NULL ? ctx->mode_ctx : NULL;
    uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (te_compose == NULL || frame_id == 0) {
        return;
    }
    (void)present_async_copy_drain(&te_compose->async_copy);
    if (esp_display_present_tracker_get_building_frame(
                &te_compose->tracker) == frame_id) {
        esp_display_present_tracker_rollback_frame(
            &te_compose->tracker, frame_id,
            te_compose->building_previous_frame, false);
        te_compose->building_previous_frame = 0;
    }
    if (te_compose->compose_buffer_count >= 2 &&
            te_compose->has_active_buffer) {
        te_compose->has_active_buffer = false;
    }
}

esp_err_t esp_display_present_te_compose_submit_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region)
{
    esp_display_present_te_compose_t *te_compose =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (te_compose == NULL || te_compose->stopped || region == NULL ||
            region->surface.pixels == NULL || frame_id == 0 ||
            region->surface.width == 0 || region->surface.height == 0 ||
            (uint32_t)region->origin_x + region->surface.width >
            te_compose->logical_width ||
            (uint32_t)region->origin_y + region->surface.height >
            te_compose->logical_height ||
            region->surface.stride_bytes !=
            (size_t)region->surface.width * te_compose->draw.color_bytes) {
        return ESP_ERR_INVALID_ARG;
    }

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
    if (start_new) {
        te_compose->building_previous_frame = previous_frame;
    }

    esp_display_present_blit_fb_t dst = {
        .pixels = te_compose->draw.pixels,
        .width = te_compose->draw.width,
        .height = te_compose->draw.height,
        .stride_bytes = te_compose->draw.stride_bytes,
        .color_bytes = te_compose->draw.color_bytes,
        .rotation = te_compose->rotation,
        .ppa_handle = te_compose->ppa_handle,
    };
    /*
     * Panel wire order is big-endian RGB565. Swap the tile in place before
     * the rotate/copy hop: byte order is pixel-local, so it commutes with the
     * placement and no FB sweep is needed after the blit. create() rejects
     * swap unless color_bytes == 2, and the stride check above guarantees
     * the tile rows are contiguous.
     */
    if (present_async_copy_submit(&te_compose->async_copy, ctx->target, &dst,
                                  region, te_compose->draw.panel_byte_order) ==
            ESP_OK) {
        return ESP_OK;
    }
    /* A queued blit invalidates its row span afterwards, which would drop
     * an inline CPU copy into the same rows, so never run both at once. */
    ret = present_async_copy_drain(&te_compose->async_copy);
    if (ret == ESP_OK) {
        if (te_compose->draw.panel_byte_order) {
            present_rgb565_swap_in_place(
                region->surface.pixels,
                (size_t)region->surface.width * region->surface.height);
        }
        ret = present_target_blit_region(ctx->target, &dst, region);
    }
    if (ret != ESP_OK) {
        esp_display_present_tracker_rollback_frame(
            ctx->tracker, frame_id,
            start_new ? previous_frame : te_compose->building_previous_frame,
            false);
        te_compose->building_previous_frame = 0;
    }
    return ret;
}
