/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_display_present.h"
#include "esp_display_present_target.h"
#include "present_mode_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *pixels;
    size_t stride_bytes;
    uint16_t width;
    uint16_t height;
    uint8_t color_bytes;
} esp_display_present_fb_pixels_t;

typedef struct {
    bool supports_coverage_areas;
    bool surface_retains_content;
    bool previous_surface_readable;
    uint8_t frame_buffer_count;
    uint16_t width;
    uint16_t height;
    size_t stride_bytes;
    uint8_t color_bytes;
    size_t max_damage_areas;
} esp_display_present_fb_caps_t;

typedef struct {
    esp_display_present_target_t *target;
    size_t max_damage_areas;
    /** Optional fixed producer. NULL binds the first producer task. */
    TaskHandle_t submit_task;
    /** Pipeline free-buffer acquire budget in ms; 0 selects the default. */
    uint32_t pipeline_acquire_timeout_ms;
    /** Presenter-owned drawbuf pool borrowed by the acquire stage. */
    const esp_display_present_drawbuf_pool_t *drawbuf_pool;
} esp_display_present_fb_endpoint_config_t;

typedef struct esp_display_present_fb_endpoint
    esp_display_present_fb_endpoint_t;

/** A single-producer surface lease. Do not retain it after submit/cancel. */
typedef struct esp_display_present_fb_lease {
    esp_display_present_fb_endpoint_t *owner;
    uint64_t frame_id;
    uint32_t token;
    esp_display_present_fb_pixels_t surface;
} esp_display_present_fb_lease_t;

esp_err_t esp_display_present_fb_create(
    const esp_display_present_fb_endpoint_config_t *config,
    esp_display_present_fb_endpoint_t **out_surface);

/**
 * Stop accepting frames after all submitted fences complete. Cancel any active
 * lease first. A permitted scanout tail returns the private two-phase-stop
 * signal so the presenter can detach target callbacks before discarding it.
 *
 * Fault escape: once the tracker is faulted, the frame-done ISR path is dead
 * and pool/present bookkeeping can never retire, so stop discards it through
 * the tracker force-stop path (same contract as the GRAM and TE endpoints)
 * instead of blocking teardown forever. The frame buffers are borrowed
 * panel-driver memory and the drawbuf is facade-owned scratch, so releasing
 * them here cannot cause a use-after-free. An active producer lease still
 * blocks stop even when faulted.
 */
esp_err_t esp_display_present_fb_stop(
    esp_display_present_fb_endpoint_t *surface);

/** Finish a tail-bearing stop after target callbacks are detached. */
esp_err_t esp_display_present_fb_finish_stop_after_callbacks(
    esp_display_present_fb_endpoint_t *surface);

/** Delete a stopped surface endpoint after target callbacks are unregistered. */
esp_err_t esp_display_present_fb_delete(
    esp_display_present_fb_endpoint_t *surface);

esp_err_t esp_display_present_fb_get_caps(
    const esp_display_present_fb_endpoint_t *surface,
    esp_display_present_fb_caps_t *out_caps);

/**
 * SEED/validate for mode 3: open frame_id. Coverage is facade FramePlan;
 * lease is created on first acquire_region.
 */
esp_err_t esp_display_present_fb_begin(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count);

/**
 * SEED for modes 5/6: optional prev→draw copy (owed-before temporary form).
 * Must not rewrite FramePlan outputs; lease waits for acquire_region.
 */
esp_err_t esp_display_present_fb_prepare(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count);

/** ACQUIRE: ensure lease, then return a surface sub-view (modes 5/6). */
esp_err_t esp_display_present_fb_acquire_surface(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

/** Expose the endpoint's frame tracker for stage context assembly. */
esp_display_present_frame_tracker_t *esp_display_present_fb_get_tracker(
    esp_display_present_fb_endpoint_t *surface);

/** Stage: borrow one tile from the presenter-owned drawbuf pool. */
esp_err_t esp_display_present_fb_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

esp_err_t esp_display_present_fb_commit(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit);

/** COMMIT with ORDERED barrier (mode3 partition after REPAIR). */
esp_err_t esp_display_present_fb_commit_ordered(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit);

/** REPAIR complement (mode3): fill unrendered regions from disp_fb. */
esp_err_t esp_display_present_fb_repair_complement(
    present_frame_ctx_t *ctx,
    bool *out_repaired);

/** REPAIR rotate (mode5): logical lease → physical draw_fb; no-op if none. */
esp_err_t esp_display_present_fb_repair_rotate_stage(
    present_frame_ctx_t *ctx,
    bool *out_repaired);

/**
 * @brief Stage: copy the current partition drawbuf area into the active surface.
 *
 * The endpoint keeps the frame lease active across multiple areas; this stage
 * only commits one rendered partition into the active draw target.
 */
esp_err_t esp_display_present_fb_write_partition(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region);

bool esp_display_present_fb_notify_transfer_done_from_isr(
    esp_display_present_fb_endpoint_t *surface);

bool esp_display_present_fb_notify_frame_done_from_isr(
    esp_display_present_fb_endpoint_t *surface,
    BaseType_t *need_yield);

uint64_t esp_display_present_fb_get_completed_transfer(
    const esp_display_present_fb_endpoint_t *surface);

uint64_t esp_display_present_fb_get_last_submitted(
    const esp_display_present_fb_endpoint_t *surface);

uint64_t esp_display_present_fb_get_completed_present(
    const esp_display_present_fb_endpoint_t *surface);

bool esp_display_present_fb_is_faulted(
    const esp_display_present_fb_endpoint_t *surface);

/** Mode 3 (FB partition + REPAIR complement) as one row of stage actions. */
extern const present_mode_ops_t present_mode_fb_partition;

/** Mode 5/6 (direct/full framebuffer switch) as one row of stage actions. */
extern const present_mode_ops_t present_mode_fb_switch;

#ifdef __cplusplus
}
#endif
