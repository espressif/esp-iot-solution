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
#include "esp_display_present_drawbuf.h"
#include "present_mode_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_display_present_gram_endpoint
    esp_display_present_gram_endpoint_t;

/**
 * Create a GRAM partition transport.
 *
 * @p transfer_timeout_ms is the per-push transfer-done wait budget;
 * 0 selects ESP_DISPLAY_PRESENT_DEFAULT_TRANSFER_TIMEOUT_MS. A timeout
 * faults the tracker with ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT.
 */
esp_err_t esp_display_present_gram_create(
    esp_display_present_target_t *target,
    const esp_display_present_drawbuf_pool_t *drawbuf_pool,
    uint32_t transfer_timeout_ms,
    esp_display_present_gram_endpoint_t **out_endpoint);

/** Stop after the in-flight partition transfer is released. */
esp_err_t esp_display_present_gram_stop(
    esp_display_present_gram_endpoint_t *endpoint);

/** Delete a stopped GRAM partition transport. */
esp_err_t esp_display_present_gram_delete(
    esp_display_present_gram_endpoint_t *endpoint);

/** Expose the endpoint's frame tracker for stage context assembly. */
esp_display_present_frame_tracker_t *esp_display_present_gram_get_tracker(
    esp_display_present_gram_endpoint_t *endpoint);

/**
 * @brief Stage: acquire one GRAM DMA draw buffer for a tightly packed area.
 *
 * The returned region height may be smaller than the area height when the
 * area does not fit in one draw buffer.
 */
esp_err_t esp_display_present_gram_acquire_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_present_area_t *area,
    esp_display_present_pixel_format_t pixel_format,
    esp_display_presenter_region_t *out_region);

/** Stage: return an acquired GRAM draw buffer that was not submitted. */
esp_err_t esp_display_present_gram_cancel_drawbuf(
    present_frame_ctx_t *ctx,
    void *pixels);

/**
 * @brief Stage: submit an acquired GRAM draw buffer to panel GRAM.
 *
 * @p region must describe a tightly packed area. The buffer remains owned by
 * the GRAM endpoint until the transfer-done callback releases it.
 */
esp_err_t esp_display_present_gram_submit_drawbuf(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region);

/** Stage: finish the frame after all partition drawbuf regions have retired. */
esp_err_t esp_display_present_gram_commit_frame(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_submit_t *submit);

/**
 * End a partially submitted frame after its accepted DMA transfers drain.
 * A void mode callback cannot report the terminal error, so a drain failure
 * faults the tracker and makes the next producer operation fail.
 */
void esp_display_present_gram_cancel_frame(present_frame_ctx_t *ctx);

bool esp_display_present_gram_notify_transfer_done_from_isr(
    esp_display_present_gram_endpoint_t *endpoint);

uint64_t esp_display_present_gram_get_completed_frame(
    const esp_display_present_gram_endpoint_t *endpoint);

uint64_t esp_display_present_gram_get_last_submitted_frame(
    const esp_display_present_gram_endpoint_t *endpoint);

/** Mode 1 (GRAM DMA kick / wait-inflight) as one row of stage actions. */
extern const present_mode_ops_t present_mode_gram_dma;

#ifdef __cplusplus
}
#endif
