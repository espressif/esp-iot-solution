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
#include "esp_display_present_target.h"
#include "present_mode_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_display_present_te_compose esp_display_present_te_compose_t;

typedef struct {
    void *pixels;
    size_t stride_bytes;
    uint16_t width;
    uint16_t height;
} esp_display_present_te_compose_surface_t;

/**
 * @brief Create a TE-synchronized GRAM frame commit endpoint.
 *
 * The endpoint owns one to three persistent full-screen draw buffers. Region
 * submit blits tiles into the active buffer. The default single-buffer path
 * waits synchronously at commit. Two or more buffers use a TE-local pool:
 * submit frees the previous baseline immediately so the next compose can
 * overlap GRAM DMA. Transfer-done confirms the submitted surface as baseline.
 * Acquire stages borrow tiles from the presenter-owned @p drawbuf_pool.
 *
 * @p transfer_timeout_ms is the per-push transfer-done wait budget;
 * 0 selects ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS. A timeout
 * faults the tracker with ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT.
 * @p acquire_timeout_ms is the producer backpressure budget; 0 selects
 * ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS.
 */
esp_err_t esp_display_present_te_compose_create(
    esp_display_present_target_t *target,
    size_t max_damage_areas,
    const esp_display_present_drawbuf_pool_t *drawbuf_pool,
    uint32_t transfer_timeout_ms,
    uint32_t acquire_timeout_ms,
    esp_display_present_te_compose_t **out_endpoint);

/** Acquire one compose surface before partition rendering begins. */
esp_err_t esp_display_present_te_compose_begin(
    present_frame_ctx_t *ctx,
    const esp_display_present_surface_request_t *request,
    esp_display_present_area_t *out_render_areas,
    size_t render_area_capacity,
    size_t *out_render_area_count);

/** Stop after all transfer callbacks complete. Keep target callbacks connected
 *  while retrying ESP_ERR_INVALID_STATE. */
esp_err_t esp_display_present_te_compose_stop(
    esp_display_present_te_compose_t *te_compose);

/** Release a stopped te_compose. Also stops an already-idle te_compose. */
esp_err_t esp_display_present_te_compose_delete(
    esp_display_present_te_compose_t *te_compose);

esp_err_t esp_display_present_te_compose_get_surface(
    const esp_display_present_te_compose_t *te_compose,
    esp_display_present_te_compose_surface_t *out_surface);

esp_err_t esp_display_present_te_compose_validate_frame(
    esp_display_present_te_compose_t *te_compose,
    uint64_t frame_id);

/** Expose the endpoint's frame tracker for stage context assembly. */
esp_display_present_frame_tracker_t *esp_display_present_te_compose_get_tracker(
    esp_display_present_te_compose_t *te_compose);

/** Stage: borrow one tile from the presenter-owned drawbuf pool. */
esp_err_t esp_display_present_te_compose_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

/**
 * @brief Stage: submit a tightly packed area drawbuf into the full-screen buffer.
 */
esp_err_t esp_display_present_te_compose_submit_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region);

/** Stage: repair stale regions of the acquired rotating compose buffer. */
esp_err_t esp_display_present_te_compose_repair(
    present_frame_ctx_t *ctx, bool *out_repaired);

/** Stage: commit inclusive logical areas; NULL/0 sends the full logical surface. */
esp_err_t esp_display_present_te_compose_commit_frame(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit);

/** Cancel a compose-only build that has not reached the TE transport. */
void esp_display_present_te_compose_cancel_frame(present_frame_ctx_t *ctx);

bool esp_display_present_te_compose_notify_transfer_done_from_isr(
    esp_display_present_te_compose_t *te_compose);

uint64_t esp_display_present_te_compose_get_completed_transfer_frame(
    const esp_display_present_te_compose_t *te_compose);

uint64_t esp_display_present_te_compose_get_last_submitted_frame(
    const esp_display_present_te_compose_t *te_compose);

uint64_t esp_display_present_te_compose_get_completed_present_frame(
    const esp_display_present_te_compose_t *te_compose);

/** Mode 2 (TE anti-tearing compose) as one row of stage actions. */
extern const present_mode_ops_t present_mode_te_compose;

#ifdef __cplusplus
}
#endif
