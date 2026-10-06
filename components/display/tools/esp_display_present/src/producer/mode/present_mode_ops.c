/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mode rows: one table of stage actions per product path. The facade only
 * assembles a frame context and calls through these tables; FENCE/ISR stay
 * on the endpoint lifecycle path.
 */

#include "present_mode_internal.h"

#include "esp_display_present_fb.h"
#include "esp_display_present_gram.h"
#include "esp_display_present_te_compose.h"
#include "present_fb_internal.h"

/* Surface-backed modes draw straight into the lease; submit is a no-op. */
static esp_err_t fb_switch_submit_noop(
    present_frame_ctx_t *ctx,
    const esp_display_presenter_region_t *region)
{
    (void)ctx;
    (void)region;
    return ESP_OK;
}

const present_mode_ops_t present_mode_gram_dma = {
    .name = "gram_dma",
    .acquire_region = esp_display_present_gram_acquire_drawbuf,
    .cancel_region = esp_display_present_gram_cancel_drawbuf,
    .submit_region = esp_display_present_gram_submit_drawbuf,
    .commit_frame = esp_display_present_gram_commit_frame,
    .cancel_frame = esp_display_present_gram_cancel_frame,
};

const present_mode_ops_t present_mode_te_compose = {
    .name = "te_compose",
    .begin_frame = esp_display_present_te_compose_begin,
    .acquire_region = esp_display_present_te_compose_acquire_drawbuf,
    .cancel_region = present_stage_cancel_tile_noop,
    .submit_region = esp_display_present_te_compose_submit_drawbuf,
    .repair_frame = esp_display_present_te_compose_repair,
    .commit_frame = esp_display_present_te_compose_commit_frame,
    .cancel_frame = esp_display_present_te_compose_cancel_frame,
};

const present_mode_ops_t present_mode_fb_partition = {
    .name = "fb_partition",
    .begin_frame = esp_display_present_fb_begin,
    .acquire_region = esp_display_present_fb_acquire_drawbuf,
    .cancel_region = present_stage_cancel_tile_noop,
    .submit_region = esp_display_present_fb_write_partition,
    .repair_frame = esp_display_present_fb_repair_complement,
    .commit_frame = esp_display_present_fb_commit_ordered,
    .cancel_frame = present_fb_cancel_frame_stage,
};

const present_mode_ops_t present_mode_fb_switch = {
    .name = "fb_switch",
    .begin_frame = esp_display_present_fb_prepare,
    .acquire_region = esp_display_present_fb_acquire_surface,
    .cancel_region = present_stage_cancel_tile_noop,
    .submit_region = fb_switch_submit_noop,
    .repair_frame = esp_display_present_fb_repair_rotate_stage,
    .commit_frame = esp_display_present_fb_commit,
    .cancel_frame = present_fb_cancel_frame_stage,
};
