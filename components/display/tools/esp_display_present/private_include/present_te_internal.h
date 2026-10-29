/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_display_present_te_compose.h"
#include "esp_display_present_frame_tracker.h"
#include "esp_display_present_drawbuf.h"
#include "esp_display_present_te.h"
#include "esp_display_present_target.h"
#include "present_async_copy.h"
#include "present_buffer_repair.h"
#include "present_te_pool.h"

#include "esp_lcd_panel_ops.h"

#define PRESENT_TE_COMPOSE_MAX_BUFFERS ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES

struct esp_display_present_te_compose {
    /* Sync / frame lifecycle */
    esp_display_present_frame_tracker_t tracker;
    esp_display_present_te_sync_context_t *te_ctx;
    uint8_t te_timeouts;
    uint64_t building_previous_frame;
    bool te_degraded;
    bool stopped;
    uint8_t compose_buffer_count;
    bool has_active_buffer;
    esp_display_present_target_t *target;
    void *display_buffer;
    present_buffer_repair_state_t repair;
    present_te_pool_t pool;

    /* Panel out */
    esp_lcd_panel_handle_t panel;
    esp_display_present_rotation_t rotation;
    uint16_t logical_width;
    uint16_t logical_height;
    size_t max_damage_areas;
    uint32_t transfer_timeout_ms;
    uint32_t acquire_timeout_ms;

    esp_display_present_drawbuf_pool_t drawbuf_pool;
    uint8_t next_drawbuf;
    void *ppa_handle;
    /** Swap + tile copies overlap rendering when the drawbuf pool has two. */
    present_async_copy_t async_copy;

    struct {
        uint8_t *pixels;
        uint8_t *buffers[PRESENT_TE_COMPOSE_MAX_BUFFERS];
        size_t stride_bytes;
        uint16_t width;
        uint16_t height;
        uint8_t color_bytes;
        bool panel_byte_order;
    } draw;
};

esp_err_t present_te_compose_push_frame(
    esp_display_present_te_compose_t *te_compose, uint8_t *pixels,
    bool *out_submitted_any);

esp_err_t present_te_compose_repair_create(
    esp_display_present_te_compose_t *te_compose);
void present_te_compose_repair_destroy(
    esp_display_present_te_compose_t *te_compose);
