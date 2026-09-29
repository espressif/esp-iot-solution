/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_display_present_config.h"
#include "esp_display_present_target.h"

typedef struct {
    esp_display_present_area_t *areas;
    uint16_t count;
    uint32_t revision;
    bool valid;
} present_buffer_repair_buffer_t;

typedef struct {
    bool enabled;
    bool display_buffer_valid;
    uint32_t revision;
    uint16_t pending_capacity;
    uint16_t repair_capacity;
    uint16_t current_capacity;
    present_buffer_repair_buffer_t
    buffers[ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS];
    esp_display_present_area_t *current;
    esp_display_present_area_t *repair;
    esp_display_present_area_t *dirty;
    uint8_t *dirty_joined;
    esp_display_present_area_t *unrendered;
    uint16_t unrendered_capacity;
} present_buffer_repair_state_t;

typedef struct {
    esp_display_present_target_t *target;
    void *buffers[ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS];
    uint8_t buffer_count;
    void *display_buffer;
    void *draw_buffer;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t physical_width;
    uint16_t physical_height;
    size_t physical_stride_bytes;
    uint8_t color_bytes;
} present_buffer_repair_view_t;

esp_err_t present_buffer_repair_create(
    present_buffer_repair_state_t *state, uint8_t buffer_count,
    size_t max_damage_areas, bool buffers_initially_valid,
    bool display_buffer_valid);

void present_buffer_repair_destroy(present_buffer_repair_state_t *state);

/**
 * DIRECT seed: before the producer draws into view->draw_buffer, copy from
 * view->display_buffer only the pixels that changed since the draw buffer
 * was last presented, excluding @p dirty_areas that the producer redraws.
 * With @p sync false (full-coverage frame) nothing is copied. Either way the
 * draw buffer stops being a baseline until present_buffer_repair_record_draw().
 */
esp_err_t present_buffer_repair_begin_draw(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *dirty_areas,
    size_t dirty_area_count,
    bool sync);

/**
 * DIRECT commit: record @p drawn_areas (none = full surface) as the change
 * set of view->draw_buffer. @p committed false forgets all history.
 */
void present_buffer_repair_record_draw(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *drawn_areas,
    size_t drawn_area_count,
    bool committed);

esp_err_t present_buffer_repair_apply(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *rendered_areas,
    size_t rendered_area_count);
