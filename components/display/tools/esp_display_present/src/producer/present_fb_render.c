/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_fb_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_display_present_blit.h"
#include "esp_display_present_dirty.h"
#include "esp_display_present_geometry.h"
#include "esp_display_present_ppa.h"
#include "esp_display_present_endpoint.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "present_fb";

static bool fb_uses_transform(
    const esp_display_present_fb_endpoint_t *frame)
{
    return frame != NULL && frame->fb.transform != NULL;
}

static const char *fb_contract_name(esp_display_present_fb_contract_t fb)
{
    switch (fb) {
    case ESP_DISPLAY_PRESENT_FB_PERSISTENT:
        return "fb_persistent";
    case ESP_DISPLAY_PRESENT_FB_FULL:
        return "fb_switch";
    case ESP_DISPLAY_PRESENT_FB_DIRECT:
        return "fb_direct";
    case ESP_DISPLAY_PRESENT_FB_REPAIR:
        return "fb_repair";
    default:
        return "fb_unknown";
    }
}

static void copy_full_surface(
    const esp_display_present_fb_endpoint_t *frame,
    const void *source_pixels,
    void *destination_pixels)
{
    esp_display_present_blit_plane_t dst = {
        .pixels = destination_pixels,
        .width = frame->width,
        .height = frame->height,
        .stride_bytes = frame->stride_bytes,
        .color_bytes = frame->color_bytes,
    };
    esp_display_present_blit_plane_t src = {
        .pixels = (void *)source_pixels,
        .width = frame->width,
        .height = frame->height,
        .stride_bytes = frame->stride_bytes,
        .color_bytes = frame->color_bytes,
    };
    esp_display_present_blit_copy_request_t request = {
        .source = src,
        .source_area = {
            .x1 = 0,
            .y1 = 0,
            .x2 = frame->width - 1,
            .y2 = frame->height - 1,
        },
        .destination = dst,
        .destination_origin = {
            .x = 0,
            .y = 0,
        },
        .flags = ESP_DISPLAY_PRESENT_BLIT_COPY_NONE,
    };
    (void)esp_display_present_blit_copy_area(&request);
}

static const char *present_mode_name(esp_display_present_mode_t mode)
{
    switch (mode) {
    case ESP_DISPLAY_PRESENT_MODE_NONE:
        return "NONE";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL:
        return "DOUBLE_FULL";
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL:
        return "TRIPLE_FULL";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT:
        return "DOUBLE_DIRECT";
    case ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL:
        return "TRIPLE_PARTIAL";
    case ESP_DISPLAY_PRESENT_MODE_TE_SYNC:
        return "TE_SYNC";
    case ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL:
        return "DOUBLE_PARTIAL";
    default:
        return "UNKNOWN";
    }
}

static esp_err_t create_partial_storage(esp_display_present_fb_endpoint_t *frame)
{
    bool direct = present_fb_previous_readable(frame) &&
                  !fb_uses_transform(frame) &&
                  frame->fb.frame_buffer_count >= 2;
    if (!present_fb_requires_repair(frame) && !direct) {
        return ESP_OK;
    }
    /* DIRECT only seeds after a presented frame, so its display buffer is
     * always a valid copy source. */
    return present_buffer_repair_create(
               &frame->repair.state, frame->fb.frame_buffer_count,
               frame->max_damage_areas, false, direct);
}

static void destroy_partial_storage(esp_display_present_fb_endpoint_t *frame)
{
    present_buffer_repair_destroy(&frame->repair.state);
}

static void try_create_partition_ppa(esp_display_present_fb_endpoint_t *frame)
{
    if (frame == NULL || !present_fb_requires_repair(frame)) {
        return;
    }
    (void)esp_display_present_ppa_try_open_tile_client(frame->rotation,
                                                       &frame->fb.ppa_handle);
}

static void fb_create_fail_cleanup(esp_display_present_fb_endpoint_t *frame)
{
    if (frame == NULL) {
        return;
    }
    if (frame->fb.transform != NULL) {
        esp_display_present_transform_delete(frame->fb.transform);
        frame->fb.transform = NULL;
    }
    present_async_copy_deinit(&frame->fb.async_copy);
    esp_display_present_ppa_close_tile_client(&frame->fb.ppa_handle);
    destroy_partial_storage(frame);
    esp_display_present_tracker_deinit_pool(&frame->tracker);
    esp_display_present_tracker_deinit_switch(&frame->tracker);
    free(frame);
}

static void init_surface_base(
    esp_display_present_fb_endpoint_t *frame,
    const esp_display_present_fb_endpoint_config_t *config,
    const esp_display_present_target_info_t *info)
{
    frame->partial_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    frame->target = config->target;
    frame->profile = info->fb.profile;
    frame->panel = info->hw.panel;
    frame->width = info->hw.width;
    frame->height = info->hw.height;
    frame->stride_bytes = (size_t)info->hw.width * info->hw.color_bytes;
    frame->physical_width = info->hw.width;
    frame->physical_height = info->hw.height;
    if (info->hw.rotation == ESP_DISPLAY_PRESENT_ROTATE_90 ||
            info->hw.rotation == ESP_DISPLAY_PRESENT_ROTATE_270) {
        frame->physical_width = info->hw.height;
        frame->physical_height = info->hw.width;
    }
    frame->physical_stride_bytes =
        (size_t)frame->physical_width * info->hw.color_bytes;
    frame->color_bytes = info->hw.color_bytes;
    frame->rotation = info->hw.rotation;
    frame->fb.frame_buffer_count = info->fb.frame_buffer_count;
    frame->max_damage_areas = config->max_damage_areas;
    for (uint8_t index = 0; index < info->fb.frame_buffer_count; ++index) {
        frame->fb.frame_buffers[index] = info->fb.frame_buffers[index];
    }
}

static esp_err_t init_fb_storage(
    esp_display_present_fb_endpoint_t *frame,
    uint32_t pipeline_acquire_timeout_ms)
{
    esp_display_present_tracker_frame_buffer_plan_t plan;
    esp_err_t ret = esp_display_present_tracker_build_frame_buffer_plan(
                        &frame->profile, frame->fb.frame_buffers,
                        frame->fb.frame_buffer_count, &plan);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!plan.use_pipeline) {
        frame->fb.draw_fb = frame->fb.frame_buffers[0];
        return ESP_OK;
    }
    ret = esp_display_present_tracker_init_pool(
              &frame->tracker, frame->fb.frame_buffers,
              frame->fb.frame_buffer_count, &plan, true,
              pipeline_acquire_timeout_ms);
    if (ret == ESP_OK) {
        frame->fb.disp_fb = plan.disp_fb;
        frame->fb.draw_fb = plan.draw_fb;
    }
    return ret;
}

static esp_err_t init_surface_storage(
    esp_display_present_fb_endpoint_t *frame,
    const esp_display_present_fb_endpoint_config_t *config)
{
    return init_fb_storage(frame, config->pipeline_acquire_timeout_ms);
}

static esp_err_t init_transform_lease_surface(
    esp_display_present_fb_endpoint_t *frame,
    uint8_t color_bytes)
{
    esp_err_t ret = esp_display_present_transform_create(
                        frame->target, color_bytes, &frame->fb.transform);
    esp_display_present_transform_surface_t logical = {0};
    if (ret == ESP_OK) {
        ret = esp_display_present_transform_get_logical_surface(
                  frame->fb.transform, &logical);
    }
    if (ret != ESP_OK) {
        return ret;
    }
    if (logical.width != frame->width || logical.height != frame->height ||
            logical.stride_bytes != frame->stride_bytes) {
        return ESP_ERR_INVALID_STATE;
    }
    frame->lease.logical_surface = logical.pixels;
    return ESP_OK;
}

static esp_err_t init_lease_surface(
    esp_display_present_fb_endpoint_t *frame,
    uint8_t color_bytes)
{
    /* Canvas source:
     *   full-fresh + rotation -> logical transform buffer (submit rotates)
     *   partial + rotation    -> physical draw_fb (partition copy rotates)
     *   plain FB              -> draw_fb
     */
    if (frame->rotation != ESP_DISPLAY_PRESENT_ROTATE_0 &&
            !present_fb_requires_repair(frame)) {
        esp_err_t ret = init_transform_lease_surface(frame, color_bytes);
        /* Force 1-buffer logical draw. The two DOUBLE_FULL scanout FBs only
         * rotate/refresh, matching TRIPLE_FULL rotation (logical draw +
         * ping-pong physical destinations). */
        if (ret == ESP_OK &&
                frame->profile.mode == ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL) {
            frame->fb.defer_draw_acquire_until_transform = true;
        }
        return ret;
    }
    frame->lease.logical_surface = frame->fb.draw_fb;
    return frame->lease.logical_surface != NULL
           ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t esp_display_present_fb_create(
    const esp_display_present_fb_endpoint_config_t *config,
    esp_display_present_fb_endpoint_t **out_frame)
{
    if (out_frame != NULL) {
        *out_frame = NULL;
    }
    const esp_display_present_target_info_t *info = config != NULL
                                                    ? esp_display_present_target_get_info(config->target) : NULL;
    if (config == NULL || info == NULL || out_frame == NULL ||
            (info->hw.color_bytes != 2 && info->hw.color_bytes != 3) ||
            config->max_damage_areas == 0 ||
            config->max_damage_areas > UINT16_MAX ||
            ESP_DISPLAY_PRESENT_UNRENDERED_AREA_SCRATCH_COUNT(
                config->max_damage_areas) > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (info->fb.profile.storage == ESP_DISPLAY_PRESENT_STORAGE_GRAM) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_display_present_fb_endpoint_t *frame = heap_caps_calloc(
                                                   1, sizeof(*frame), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (frame == NULL) {
        return ESP_ERR_NO_MEM;
    }
    init_surface_base(frame, config, info);
    esp_err_t ret =
        esp_display_present_tracker_init_switch(&frame->tracker);
    if (ret != ESP_OK) {
        fb_create_fail_cleanup(frame);
        return ret;
    }
    ret = init_surface_storage(frame, config);
    if (ret != ESP_OK) {
        fb_create_fail_cleanup(frame);
        return ret;
    }
    ret = init_lease_surface(frame, info->hw.color_bytes);
    if (ret != ESP_OK) {
        fb_create_fail_cleanup(frame);
        return ret;
    }

    ret = create_partial_storage(frame);
    if (ret != ESP_OK) {
        fb_create_fail_cleanup(frame);
        return ret;
    }
    try_create_partition_ppa(frame);

    frame->fb.submit_gate_enabled =
        frame->profile.frame_done_release ==
        ESP_DISPLAY_PRESENT_FRAME_DONE_RELEASE_SUBMIT;
    frame->fb.single_pending_switch =
        info->hw.panel_interface == ESP_DISPLAY_PRESENT_PANEL_IF_RGB;
    if (config->drawbuf_pool != NULL) {
        frame->fb.drawbuf_pool = *config->drawbuf_pool;
    }
    ret = present_async_copy_init(&frame->fb.async_copy,
                                  &frame->fb.drawbuf_pool,
                                  present_fb_requires_repair(frame));
    if (ret != ESP_OK) {
        fb_create_fail_cleanup(frame);
        return ret;
    }
    esp_display_present_tracker_init_frame(
        &frame->tracker, config->submit_task, false);
    *out_frame = frame;
    ESP_LOGI(TAG,
             "surface: mode=%s contract=%s storage=%s areas=%u retain=%u"
             " previous=%u repair=%u buffers=%u size=%ux%u physical=%ux%u"
             " rotate=%d ppa=%u async_copy=%u",
             present_mode_name(frame->profile.mode),
             fb_contract_name(frame->profile.fb),
             fb_uses_transform(frame) ? "transform"
             : (present_fb_requires_repair(frame) &&
                frame->rotation != ESP_DISPLAY_PRESENT_ROTATE_0
                ? "partition_rotate"
                : "fb"),
             (unsigned)present_fb_supports_coverage(frame),
             (unsigned)present_fb_retains_content(frame),
             (unsigned)(present_fb_previous_readable(frame) &&
                        !fb_uses_transform(frame)),
             (unsigned)present_fb_requires_repair(frame),
             (unsigned)frame->fb.frame_buffer_count,
             (unsigned)frame->width, (unsigned)frame->height,
             (unsigned)frame->physical_width,
             (unsigned)frame->physical_height, (int)info->hw.rotation,
             frame->fb.ppa_handle != NULL ? 1U : 0U,
             (unsigned)frame->fb.async_copy.enabled);
    return ESP_OK;
}

esp_err_t esp_display_present_fb_stop(
    esp_display_present_fb_endpoint_t *frame)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&frame->partial_lock);
    if (frame->stopped) {
        portEXIT_CRITICAL(&frame->partial_lock);
        return ESP_OK;
    }
    frame->closing = true;
    bool lease_busy = frame->lease.active;
    bool hardware_busy =
        esp_display_present_tracker_pool_has_inflight(&frame->tracker) ||
        esp_display_present_tracker_has_pending_present(&frame->tracker);
    portEXIT_CRITICAL(&frame->partial_lock);
    esp_display_present_tracker_request_switch_stop(&frame->tracker);
    if (lease_busy) {
        return ESP_ERR_INVALID_STATE;
    }
    if (hardware_busy &&
            !esp_display_present_tracker_is_faulted(&frame->tracker)) {
        /* quiesce() permits one scanout-owned present tail for FB endpoints.
         * The presenter must detach target callbacks before this bookkeeping
         * can be abandoned, otherwise a late ISR could touch freed state. */
        return ESP_DISPLAY_PRESENT_STOP_NEEDS_CALLBACK_DETACH;
    }
    /* A faulted tracker means the frame-done ISR path is dead: inflight pool
     * entries and pending presents can never retire. The frame buffers are
     * borrowed panel-driver memory and the drawbuf is facade-owned scratch,
     * so abandoning that bookkeeping cannot cause a UAF. With hardware idle,
     * any fences still queued are failed-frame bookkeeping and are safe to
     * discard for the same reason — the GRAM/TE teardown contract. */
    esp_err_t ret = esp_display_present_tracker_stop(&frame->tracker);
    if (ret == ESP_ERR_INVALID_STATE) {
        ret = esp_display_present_tracker_force_stop(&frame->tracker);
    }
    if (ret == ESP_OK) {
        frame->stopped = true;
    }
    return ret;
}

esp_err_t esp_display_present_fb_finish_stop_after_callbacks(
    esp_display_present_fb_endpoint_t *frame)
{
    if (frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&frame->partial_lock);
    bool valid = frame->closing && !frame->lease.active;
    portEXIT_CRITICAL(&frame->partial_lock);
    if (!valid) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_display_present_tracker_force_stop(&frame->tracker);
    if (ret == ESP_OK) {
        frame->stopped = true;
    }
    return ret;
}

esp_err_t esp_display_present_fb_delete(
    esp_display_present_fb_endpoint_t *frame)
{
    esp_err_t ret = esp_display_present_fb_stop(frame);
    if (ret != ESP_OK) {
        return ret;
    }
    if (frame->fb.transform != NULL) {
        esp_display_present_transform_delete(frame->fb.transform);
        frame->fb.transform = NULL;
    }
    present_async_copy_deinit(&frame->fb.async_copy);
    esp_display_present_ppa_close_tile_client(&frame->fb.ppa_handle);
    destroy_partial_storage(frame);
    esp_display_present_tracker_deinit_pool(&frame->tracker);
    esp_display_present_tracker_deinit_switch(&frame->tracker);
    free(frame);
    return ESP_OK;
}

esp_display_present_frame_tracker_t *esp_display_present_fb_get_tracker(
    esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL ? &surface->tracker : NULL;
}

esp_err_t esp_display_present_fb_get_caps(
    const esp_display_present_fb_endpoint_t *frame,
    esp_display_present_fb_caps_t *out_caps)
{
    if (frame == NULL || out_caps == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_caps = (esp_display_present_fb_caps_t) {
        .supports_coverage_areas = present_fb_supports_coverage(frame),
        .surface_retains_content = present_fb_retains_content(frame),
        .previous_surface_readable =
            present_fb_previous_readable(frame) && !fb_uses_transform(frame),
            .frame_buffer_count = frame->fb.frame_buffer_count,
            .width = frame->width,
            .height = frame->height,
            .stride_bytes = frame->stride_bytes,
            .color_bytes = frame->color_bytes,
            .max_damage_areas = frame->max_damage_areas,
    };
    return ESP_OK;
}

esp_err_t esp_display_present_fb_begin(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count)
{
    /* FramePlan (coverage / render areas) lives in the facade. begin_frame
     * only validates the producer may open this frame_id. Lease is deferred
     * to the first acquire_region. */
    esp_display_present_fb_endpoint_t *frame =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    (void)request;
    (void)out_render_areas;
    (void)render_area_capacity;
    (void)out_render_area_count;
    if (frame == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->lease.active || frame->closing) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_display_present_tracker_validate_frame(
               &frame->tracker, frame_id, xTaskGetCurrentTaskHandle());
}

esp_err_t esp_display_present_fb_prepare(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count)
{
    /* SEED only. FramePlan negotiates coverage; the first acquire creates
     * the lease and seeds draw from the previous framebuffer. */
    esp_display_present_fb_endpoint_t *frame =
        ctx != NULL ? ctx->mode_ctx : NULL;
    (void)out_render_areas;
    (void)render_area_capacity;
    (void)out_render_area_count;
    esp_err_t ret = esp_display_present_fb_begin(
                        ctx, NULL, NULL, 0, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t dirty_count = request != NULL ? request->dirty_area_count : 0;
    if (frame->profile.fb != ESP_DISPLAY_PRESENT_FB_DIRECT ||
            fb_uses_transform(frame) ||
            frame->fb.disp_fb == NULL ||
            frame->fb.draw_fb == NULL ||
            frame->fb.disp_fb == frame->fb.draw_fb) {
        return ESP_OK;
    }
    /* Scroll, empty dirty and the first frame are planned as full-coverage
     * rasters, so the previous frame is not needed in draw_fb. */
    bool sync = dirty_count != 0 && frame->submitted_frames != 0 &&
                !request->has_scroll;
    if (present_fb_tracks_direct_history(frame)) {
        present_buffer_repair_view_t view = present_fb_repair_view(
                                                frame, frame->fb.disp_fb,
                                                frame->fb.draw_fb);
        return present_buffer_repair_begin_draw(
                   &frame->repair.state, &view,
                   sync ? request->dirty_areas : NULL,
                   sync ? dirty_count : 0, sync);
    }
    if (sync) {
        copy_full_surface(frame, frame->fb.disp_fb, frame->fb.draw_fb);
    }
    return ESP_OK;
}

esp_err_t esp_display_present_fb_acquire_surface(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region)
{
    esp_display_present_fb_endpoint_t *surface =
        ctx != NULL ? ctx->mode_ctx : NULL;
    const uint64_t frame_id = ctx != NULL ? ctx->frame_id : 0;
    if (surface == NULL || area == NULL || out_region == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = present_fb_ensure_lease(surface, frame_id);
    if (ret != ESP_OK) {
        return ret;
    }
    const esp_display_present_fb_pixels_t *lease_surface =
        &surface->lease.valet.surface;
    if (lease_surface->pixels == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_region = (esp_display_presenter_region_t) {
        .surface = {
            .pixels = (uint8_t *)lease_surface->pixels +
            (size_t)area->y1 * lease_surface->stride_bytes +
            (size_t)area->x1 * lease_surface->color_bytes,
            .stride_bytes = lease_surface->stride_bytes,
            .width = (uint16_t)(area->x2 - area->x1 + 1),
            .height = (uint16_t)(area->y2 - area->y1 + 1),
            .pixel_format = pixel_format,
        },
        .origin_x = (uint16_t)area->x1,
        .origin_y = (uint16_t)area->y1,
    };
    return ESP_OK;
}
