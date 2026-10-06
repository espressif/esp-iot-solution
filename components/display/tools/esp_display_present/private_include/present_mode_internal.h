/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present.h"
#include "esp_display_present_blit.h"
#include "esp_display_present_drawbuf.h"
#include "esp_display_present_frame_tracker.h"
#include "esp_display_present_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame context handed to stage functions.
 *
 * Deliberately narrow: the mode-private endpoint resources, the endpoint's
 * frame tracker, the present target and the frame being processed. Stage
 * functions take everything else from their own arguments.
 *
 * Discipline: fields are grouped by stage domain and this struct must not
 * grow into a god-struct. Later phases add new fields (for example the
 * repair stage's rendered-area list) only inside their own domain group.
 */
typedef struct {
    /** Mode-private endpoint resources (owned by the mode's create path). */
    void *mode_ctx;
    esp_display_present_frame_tracker_t *tracker;
    esp_display_present_target_t *target;
    uint64_t frame_id; /* Presenter-private completion ticket. */
    /** ACQUIRE domain: producer-requested panel window granularity. */
    esp_display_present_render_alignment_t render_alignment;
    /** REPAIR domain: filled by the facade from the frame submit. */
    struct {
        const esp_display_present_area_t *rendered_areas;
        size_t rendered_area_count;
    } repair;
} present_frame_ctx_t;

/* Stage function shapes used directly by the presenter facade. The frame id
 * travels inside the context; remaining arguments keep the shape the mode's
 * own primitives need. */
typedef esp_err_t (*present_stage_acquire_region_fn_t)(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

typedef esp_err_t (*present_stage_cancel_region_fn_t)(
    present_frame_ctx_t *ctx,
    void *pixels);

typedef esp_err_t (*present_stage_submit_region_fn_t)(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region);

/**
 * REPAIR hop. NULL means repair_op=none. Uses ctx->repair rendered areas;
 * must not present or rotate the FB pipeline.
 */
typedef esp_err_t (*present_stage_repair_frame_fn_t)(
    present_frame_ctx_t *ctx,
    bool *out_repaired);

typedef esp_err_t (*present_stage_commit_frame_fn_t)(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit);

/* Optional frame-lifecycle stages. begin_frame follows the FB prepare
 * contract; NULL means the mode has no frame-level begin/abort action and
 * the facade applies the generic default. */
typedef esp_err_t (*present_stage_begin_frame_fn_t)(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count);

typedef void (*present_stage_cancel_frame_fn_t)(
    present_frame_ctx_t *ctx);

/**
 * @brief One mode as a single row of stage actions.
 *
 * A present mode is data, not a monolithic endpoint: the facade only
 * assembles a present_frame_ctx_t and calls through this row. Producer-side
 * hops: begin/acquire/cancel_region/submit/repair/commit/cancel_frame.
 * FENCE/ISR stay on present_endpoint_ops_t.
 */
typedef struct {
    const char *name;
    present_stage_begin_frame_fn_t begin_frame;
    present_stage_acquire_region_fn_t acquire_region;
    present_stage_cancel_region_fn_t cancel_region;
    present_stage_submit_region_fn_t submit_region;
    present_stage_repair_frame_fn_t repair_frame;
    present_stage_commit_frame_fn_t commit_frame;
    present_stage_cancel_frame_fn_t cancel_frame;
} present_mode_ops_t;

esp_err_t present_stage_drawbuf_borrow(
    const esp_display_present_drawbuf_pool_t *pool,
    uint8_t *next_drawbuf,
    size_t row_bytes,
    uint16_t requested_rows,
    void **out_pixels,
    uint16_t *out_rows);

/**
 * @brief ACQUIRE helper: borrow a tile and fill a presenter region.
 *
 * Shared by TE compose and FB partition. GRAM DMA acquire keeps its own
 * inflight-gated path.
 */
esp_err_t present_stage_acquire_tile(
    const esp_display_present_drawbuf_pool_t *pool,
    uint8_t *next_drawbuf,
    uint8_t color_bytes,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

/** Shared cancel for lease-scoped tile borrows (no return-to-pool). */
static inline esp_err_t present_stage_cancel_tile_noop(
    present_frame_ctx_t *ctx,
    void *pixels)
{
    (void)ctx;
    (void)pixels;
    return ESP_OK;
}

/**
 * @brief SUBMIT helper: map a logical tile region and blit into @p dst.
 *
 * Used by TE compose and FB partition — same hop, different FB topology.
 */
#ifdef __cplusplus
}
#endif
