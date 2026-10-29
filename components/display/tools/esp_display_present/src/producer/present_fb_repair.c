/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_fb_internal.h"
#include "present_mode_internal.h"

esp_err_t present_fb_repair(esp_display_present_fb_endpoint_t *frame,
                            void *panel_pixels,
                            const esp_display_present_area_t *rendered_areas,
                            size_t rendered_area_count)
{
    if (frame == NULL || panel_pixels == NULL || frame->fb.disp_fb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    present_buffer_repair_view_t view = {
        .target = frame->target,
        .buffer_count = frame->fb.frame_buffer_count,
        .display_buffer = frame->fb.disp_fb,
        .draw_buffer = panel_pixels,
        .logical_width = frame->width,
        .logical_height = frame->height,
        .physical_width = frame->physical_width,
        .physical_height = frame->physical_height,
        .physical_stride_bytes = frame->physical_stride_bytes,
        .color_bytes = frame->color_bytes,
    };
    for (uint8_t index = 0; index < view.buffer_count; ++index) {
        view.buffers[index] = frame->fb.frame_buffers[index];
    }
    return present_buffer_repair_apply(
               &frame->repair.state, &view, rendered_areas,
               rendered_area_count);
}

esp_err_t
present_fb_repair_rotate(esp_display_present_fb_endpoint_t *surface,
                         const esp_display_present_fb_lease_t *lease,
                         const esp_display_present_area_t *rendered_areas,
                         size_t rendered_area_count, void **out_panel_pixels)
{
    if (surface == NULL || lease == NULL || out_panel_pixels == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_panel_pixels = lease->surface.pixels;
    if (surface->fb.transform == NULL) {
        return ESP_OK;
    }
    void *draw_buffer = surface->fb.draw_fb;
    if (draw_buffer == NULL &&
            surface->fb.defer_draw_acquire_until_transform) {
        esp_display_present_tracker_buf_t *next = NULL;
        esp_err_t ret = esp_display_present_tracker_pool_acquire_next(
                            &surface->tracker, &next);
        if (ret != ESP_OK) {
            return ret;
        }
        surface->fb.draw_fb = next->buffer;
        draw_buffer = next->buffer;
    }
    if (draw_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_display_present_area_t *areas = rendered_areas;
    size_t area_count = rendered_area_count;
    esp_display_present_area_t full = {
        .x1 = 0,
        .y1 = 0,
        .x2 = surface->width - 1,
        .y2 = surface->height - 1,
    };
    if (area_count == 0) {
        areas = &full;
        area_count = 1;
    }
    for (size_t index = 0; index < area_count; ++index) {
        esp_display_present_transform_copy_request_t request = {
            .source = {
                .pixels = lease->surface.pixels,
                .width = lease->surface.width,
                .height = lease->surface.height,
                .stride_bytes = lease->surface.stride_bytes,
                .color_bytes = lease->surface.color_bytes,
            },
            .source_origin = {
                .x = 0,
                .y = 0,
            },
            .destination_pixels = draw_buffer,
            .logical_area = areas[index],
        };
        esp_err_t ret =
            esp_display_present_transform_copy(surface->fb.transform, &request);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    *out_panel_pixels = draw_buffer;
    return ESP_OK;
}

esp_err_t esp_display_present_fb_repair_complement(present_frame_ctx_t *ctx,
                                                   bool *out_repaired)
{
    esp_display_present_fb_endpoint_t *frame = ctx != NULL ? ctx->mode_ctx : NULL;
    if (frame == NULL || out_repaired == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_repaired = false;
    esp_err_t copy_ret = present_async_copy_drain(&frame->fb.async_copy);
    if (copy_ret != ESP_OK) {
        return copy_ret;
    }
    const esp_display_present_fb_lease_t *lease = &frame->lease.valet;
    if (!present_fb_lease_is_valid(frame, lease)) {
        return ESP_ERR_INVALID_ARG;
    }
    void *panel_pixels = lease->surface.pixels;
    if (panel_pixels == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret =
        present_fb_repair(frame, panel_pixels, ctx->repair.rendered_areas,
                          ctx->repair.rendered_area_count);
    if (ret == ESP_OK) {
        *out_repaired = true;
    }
    return ret;
}

esp_err_t esp_display_present_fb_repair_rotate_stage(present_frame_ctx_t *ctx,
                                                     bool *out_repaired)
{
    esp_display_present_fb_endpoint_t *frame = ctx != NULL ? ctx->mode_ctx : NULL;
    if (frame == NULL || out_repaired == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_repaired = false;
    const esp_display_present_fb_lease_t *lease = &frame->lease.valet;
    if (!present_fb_lease_is_valid(frame, lease)) {
        return ESP_ERR_INVALID_ARG;
    }
    void *panel_pixels = NULL;
    esp_err_t ret =
        present_fb_repair_rotate(frame, lease, ctx->repair.rendered_areas,
                                 ctx->repair.rendered_area_count, &panel_pixels);
    if (ret == ESP_OK && frame->fb.transform != NULL) {
        *out_repaired = true;
    }
    return ret;
}
