/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h>

#include "esp_display_present_blit.h"
#include "esp_display_present_fb.h"
#include "esp_display_present_frame_tracker.h"
#include "esp_display_present_panel.h"
#include "esp_display_present_drawbuf.h"
#include "esp_display_present_transform.h"
#include "esp_display_present_profile.h"
#include "present_buffer_repair.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "present_async_copy.h"

struct esp_display_present_fb_endpoint {
    esp_display_present_target_t *target;
    esp_display_present_frame_tracker_t tracker;
    /** Immutable target policy; surface code does not reinterpret mode. */
    esp_display_present_profile_t profile;
    /** Panel rotation. Full-fresh modes own a transform; partial fuses into draw_fb. */
    esp_display_present_rotation_t rotation;
    esp_lcd_panel_handle_t panel;
    uint16_t width;
    uint16_t height;
    size_t stride_bytes;
    uint16_t physical_width;
    uint16_t physical_height;
    size_t physical_stride_bytes;
    uint8_t color_bytes;
    size_t max_damage_areas;
    portMUX_TYPE partial_lock;
    bool closing;
    bool stopped;
    /** Shared INTERNAL|DMA work area for renderer bounce and GRAM staging. */
    struct {
        uint8_t *data;
        size_t bytes;
    } scratch;
    struct {
        esp_display_present_transform_t *transform;
        /** Borrowed drawbuf pool metadata; presenter owns the memory. */
        esp_display_present_drawbuf_pool_t drawbuf_pool;
        uint8_t next_drawbuf;
        void *disp_fb;
        void *draw_fb;
        void *frame_buffers[ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES];
        uint8_t frame_buffer_count;
        /** DOUBLE_FULL + rotation draws into one logical transform surface
         * and ping-pongs the two pipeline FBs only as rotated scanout
         * destinations, same as TRIPLE_FULL rotation. The next physical FB
         * is acquired immediately before transform, after VSYNC recycles it. */
        bool defer_draw_acquire_until_transform;
        bool submit_gate_enabled;
        /**
         * Some RGB drivers report frame-done only when the DMA switches
         * buffers, so a second switch requested before the first lands would
         * retire one buffer late. Keep at most one switch outstanding.
         */
        bool single_pending_switch;
        /** Optional PPA SRM client for partition_rotate tile blits. */
        void *ppa_handle;
        /** Tile copies overlap rendering when the drawbuf pool has two. */
        present_async_copy_t async_copy;
    } fb;
    struct {
        present_buffer_repair_state_t state;
    } repair;
    struct {
        /** Producer-facing pixels for begin/lease (draw_fb or transform). */
        void *logical_surface;
        bool active;
        uint64_t frame_id;
        uint32_t token;
        /** Begin-time lease snapshot consumed by stage functions. */
        esp_display_present_fb_lease_t valet;
    } lease;
    uint32_t submitted_frames;
};

static inline bool present_fb_uses_pipeline(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           surface->profile.storage ==
           ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB;
}

static inline bool present_fb_requires_repair(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           surface->profile.fb == ESP_DISPLAY_PRESENT_FB_REPAIR;
}

static inline bool present_fb_supports_coverage(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           surface->profile.fb != ESP_DISPLAY_PRESENT_FB_FULL;
}

static inline bool present_fb_retains_content(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           surface->profile.fb == ESP_DISPLAY_PRESENT_FB_PERSISTENT;
}

static inline bool present_fb_previous_readable(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           surface->profile.fb == ESP_DISPLAY_PRESENT_FB_DIRECT;
}

/** DIRECT without a transform tracks per-buffer damage history. */
static inline bool present_fb_tracks_direct_history(
    const esp_display_present_fb_endpoint_t *surface)
{
    return present_fb_previous_readable(surface) &&
           surface->fb.transform == NULL &&
           surface->repair.state.enabled;
}

static inline present_buffer_repair_view_t present_fb_repair_view(
    const esp_display_present_fb_endpoint_t *surface,
    void *display_buffer,
    void *draw_buffer)
{
    present_buffer_repair_view_t view = {
        .target = surface->target,
        .buffer_count = surface->fb.frame_buffer_count,
        .display_buffer = display_buffer,
        .draw_buffer = draw_buffer,
        .logical_width = surface->width,
        .logical_height = surface->height,
        .physical_width = surface->physical_width,
        .physical_height = surface->physical_height,
        .physical_stride_bytes = surface->physical_stride_bytes,
        .color_bytes = surface->color_bytes,
    };
    for (uint8_t index = 0; index < view.buffer_count; ++index) {
        view.buffers[index] = surface->fb.frame_buffers[index];
    }
    return view;
}

static inline void present_fb_invalidate_lease(
    esp_display_present_fb_endpoint_t *surface,
    esp_display_present_fb_lease_t *lease)
{
    surface->lease.active = false;
    surface->lease.frame_id = 0;
    if (lease != NULL) {
        memset(lease, 0, sizeof(*lease));
    }
}

static inline bool present_fb_lease_is_valid(
    const esp_display_present_fb_endpoint_t *surface,
    const esp_display_present_fb_lease_t *lease)
{
    return surface != NULL && lease != NULL && surface->lease.active &&
           lease->owner == surface &&
           lease->frame_id == surface->lease.frame_id &&
           lease->token == surface->lease.token &&
           lease->surface.pixels != NULL;
}

/** ACQUIRE-owned lease: create on first region borrow for the frame. */
static inline esp_err_t present_fb_ensure_lease(
    esp_display_present_fb_endpoint_t *frame,
    uint64_t frame_id)
{
    if (frame == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->closing) {
        return ESP_ERR_INVALID_STATE;
    }
    if (frame->lease.active) {
        return frame->lease.frame_id == frame_id
               ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    void *pixels = frame->fb.transform != NULL
                   ? frame->lease.logical_surface
                   : frame->fb.draw_fb;
    if (pixels == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    ++frame->lease.token;
    if (frame->lease.token == 0) {
        ++frame->lease.token;
    }
    frame->lease.active = true;
    frame->lease.frame_id = frame_id;
    frame->lease.valet = (esp_display_present_fb_lease_t) {
        .owner = frame,
        .frame_id = frame_id,
        .token = frame->lease.token,
        .surface = {
            .pixels = pixels,
            .stride_bytes = frame->stride_bytes,
            .width = frame->width,
            .height = frame->height,
            .color_bytes = frame->color_bytes,
        },
    };
    return ESP_OK;
}

/** COMMIT-switch barrier: PENDING waits one in-flight; ORDERED keeps two. */
typedef enum {
    PRESENT_SWITCH_BARRIER_PENDING,
    PRESENT_SWITCH_BARRIER_ORDERED,
} present_switch_barrier_t;

esp_err_t present_fb_switch_commit(
    esp_display_present_fb_endpoint_t *surface,
    uint64_t frame_id,
    void *panel_pixels,
    present_switch_barrier_t barrier);

esp_err_t present_fb_repair(
    esp_display_present_fb_endpoint_t *surface,
    void *panel_pixels,
    const esp_display_present_area_t *rendered_areas,
    size_t rendered_area_count);

esp_err_t present_fb_repair_rotate(
    esp_display_present_fb_endpoint_t *surface,
    const esp_display_present_fb_lease_t *lease,
    const esp_display_present_area_t *rendered_areas,
    size_t rendered_area_count,
    void **out_panel_pixels);

bool IRAM_ATTR present_fb_transfer_done_isr(
    esp_display_present_fb_endpoint_t *surface);

bool IRAM_ATTR present_fb_frame_done_isr(
    esp_display_present_fb_endpoint_t *surface,
    BaseType_t *need_yield);

uint64_t present_fb_completed_transfer(
    const esp_display_present_fb_endpoint_t *surface);

uint64_t present_fb_completed_present(
    const esp_display_present_fb_endpoint_t *surface);

/** Shared frame-abort stage for the FB mode rows: drop the active lease. */
static inline void present_fb_cancel_frame_stage(
    present_frame_ctx_t *ctx)
{
    esp_display_present_fb_endpoint_t *surface =
        ctx != NULL ? ctx->mode_ctx : NULL;
    if (surface != NULL) {
        (void)present_async_copy_drain(&surface->fb.async_copy);
    }
    if (surface != NULL &&
            present_fb_lease_is_valid(surface, &surface->lease.valet)) {
        present_fb_invalidate_lease(surface, &surface->lease.valet);
    }
}
