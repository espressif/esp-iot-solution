/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_buffer_repair.h"

#include <stdlib.h>
#include <string.h>

#include "esp_display_present_blit.h"
#include "esp_display_present_dirty.h"
#include "esp_log.h"

static const char *TAG = "present_repair";

typedef enum {
    REPAIR_PLAN_DIFF,
    REPAIR_PLAN_BASELINE_COMMIT,
    REPAIR_PLAN_BASELINE_RESET,
} repair_plan_t;

static uint8_t buffer_index(const present_buffer_repair_view_t *view,
                            const void *buffer)
{
    for (uint8_t index = 0; index < view->buffer_count; ++index) {
        if (view->buffers[index] == buffer) {
            return index;
        }
    }
    return UINT8_MAX;
}

static uint16_t capture_current(present_buffer_repair_state_t *state,
                                const present_buffer_repair_view_t *view,
                                const esp_display_present_area_t *areas,
                                size_t area_count)
{
    uint16_t count = 0;
    for (size_t index = 0; index < area_count &&
            count < state->current_capacity; ++index) {
        esp_display_present_area_t area = areas[index];
        if (area.x1 < 0) {
            area.x1 = 0;
        }
        if (area.y1 < 0) {
            area.y1 = 0;
        }
        if (area.x2 >= view->logical_width) {
            area.x2 = view->logical_width - 1;
        }
        if (area.y2 >= view->logical_height) {
            area.y2 = view->logical_height - 1;
        }
        if (area.x2 >= area.x1 && area.y2 >= area.y1) {
            state->current[count++] = area;
        }
    }
    return count;
}

static void reset_to(present_buffer_repair_state_t *state,
                     uint8_t buffer_count, uint8_t target)
{
    for (uint8_t index = 0; index < buffer_count; ++index) {
        state->buffers[index].count = 0;
        state->buffers[index].revision = 0;
        state->buffers[index].valid = false;
    }
    state->revision = 0;
    if (target < buffer_count) {
        state->buffers[target].valid = true;
    }
}

static void commit_diff(present_buffer_repair_state_t *state,
                        uint8_t buffer_count, uint8_t target,
                        const esp_display_present_area_t *current,
                        uint16_t current_count)
{
    if (target >= buffer_count) {
        return;
    }
    if (state->revision == UINT32_MAX) {
        reset_to(state, buffer_count, target);
    }
    uint32_t revision = ++state->revision;
    for (uint8_t index = 0; index < buffer_count; ++index) {
        if (index == target || !state->buffers[index].valid) {
            continue;
        }
        if (esp_display_present_merge_area_into_list(
                    state->buffers[index].areas, state->pending_capacity,
                    &state->buffers[index].count, current,
                    current_count) != ESP_OK) {
            state->buffers[index].valid = false;
            state->buffers[index].count = 0;
        }
    }
    state->buffers[target].count = 0;
    state->buffers[target].revision = revision;
    state->buffers[target].valid = true;
}

static repair_plan_t build_plan(present_buffer_repair_state_t *state,
                                uint8_t target, uint8_t source,
                                const esp_display_present_area_t *current,
                                uint16_t current_count,
                                uint16_t *out_repair_count)
{
    *out_repair_count = 0;
    if (!state->buffers[target].valid) {
        return REPAIR_PLAN_BASELINE_COMMIT;
    }
    uint16_t repair_count = 0;
    esp_err_t ret = esp_display_present_subtract_area_list(
                        state->buffers[target].areas,
                        state->buffers[target].count, current, current_count,
                        state->repair, state->repair_capacity, &repair_count);
    if (ret != ESP_OK) {
        return REPAIR_PLAN_BASELINE_RESET;
    }
    *out_repair_count = repair_count;
    if (repair_count == 0) {
        return REPAIR_PLAN_DIFF;
    }
    if (state->buffers[source].valid &&
            state->buffers[source].revision == state->revision) {
        return REPAIR_PLAN_DIFF;
    }
    return REPAIR_PLAN_BASELINE_RESET;
}

static esp_err_t copy_areas(const present_buffer_repair_view_t *view,
                            const esp_display_present_area_t *areas,
                            uint16_t count)
{
    if (count == 0) {
        return ESP_OK;
    }
    /* No whole-buffer writeback here: the DMA2D copy writes back and
     * invalidates the rows it touches, and CPU copies stay in cache until
     * the commit/push writes back the whole draw buffer before scanout. */
    esp_display_present_blit_plane_t destination = {
        .pixels = view->draw_buffer,
        .width = view->physical_width,
        .height = view->physical_height,
        .stride_bytes = view->physical_stride_bytes,
        .color_bytes = view->color_bytes,
    };
    esp_display_present_blit_plane_t source = destination;
    source.pixels = view->display_buffer;
    for (uint16_t index = 0; index < count; ++index) {
        esp_display_present_area_t physical;
        esp_err_t ret = esp_display_present_target_map_logical_area_to_physical(
                            view->target, &areas[index], &physical);
        if (ret == ESP_OK) {
            ret = esp_display_present_blit_copy_framebuffer_area(
                      &destination, &source, &physical);
        }
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_err_t build_complement(present_buffer_repair_state_t *state,
                                  const present_buffer_repair_view_t *view,
                                  const esp_display_present_area_t *rendered,
                                  size_t rendered_count,
                                  uint16_t *out_count)
{
    memcpy(state->dirty, rendered, rendered_count * sizeof(*state->dirty));
    memset(state->dirty_joined, 0, rendered_count);
    esp_err_t ret = esp_display_present_build_unrendered_area_list(
                        state->dirty, state->dirty_joined,
                        (uint16_t)rendered_count, view->logical_width,
                        view->logical_height, state->unrendered,
                        state->unrendered_capacity, out_count);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!state->display_buffer_valid && *out_count != 0) {
        ESP_LOGE(TAG, "first partial frame must cover the surface");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t present_buffer_repair_create(
    present_buffer_repair_state_t *state, uint8_t buffer_count,
    size_t max_damage_areas, bool buffers_initially_valid,
    bool display_buffer_valid)
{
    if (state == NULL || buffer_count > ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS ||
            max_damage_areas == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(state, 0, sizeof(*state));
    state->display_buffer_valid = display_buffer_valid;
    state->unrendered_capacity =
        (uint16_t)ESP_DISPLAY_PRESENT_UNRENDERED_AREA_SCRATCH_COUNT(
            max_damage_areas);
    state->dirty = calloc(max_damage_areas, sizeof(*state->dirty));
    state->dirty_joined = calloc(max_damage_areas, 1);
    state->unrendered = calloc(state->unrendered_capacity,
                               sizeof(*state->unrendered));
    if (state->dirty == NULL || state->dirty_joined == NULL ||
            state->unrendered == NULL) {
        present_buffer_repair_destroy(state);
        return ESP_ERR_NO_MEM;
    }
    if (buffer_count < 2) {
        return ESP_OK;
    }
    state->pending_capacity = (uint16_t)(max_damage_areas * 4U);
    state->current_capacity = (uint16_t)(max_damage_areas + 1U);
    state->repair_capacity =
        (uint16_t)ESP_DISPLAY_PRESENT_UNRENDERED_AREA_SCRATCH_COUNT(
            state->pending_capacity);
    for (uint8_t index = 0; index < buffer_count; ++index) {
        state->buffers[index].areas = calloc(
                                          state->pending_capacity,
                                          sizeof(esp_display_present_area_t));
        if (state->buffers[index].areas == NULL) {
            present_buffer_repair_destroy(state);
            return ESP_ERR_NO_MEM;
        }
        state->buffers[index].valid = buffers_initially_valid;
    }
    state->current = calloc(state->current_capacity, sizeof(*state->current));
    state->repair = calloc(state->repair_capacity, sizeof(*state->repair));
    if (state->current == NULL || state->repair == NULL ||
            state->dirty == NULL || state->dirty_joined == NULL) {
        present_buffer_repair_destroy(state);
        return ESP_ERR_NO_MEM;
    }
    state->enabled = true;
    return ESP_OK;
}

void present_buffer_repair_destroy(present_buffer_repair_state_t *state)
{
    if (state == NULL) {
        return;
    }
    for (uint8_t index = 0;
            index < ESP_DISPLAY_PRESENT_MAX_FRAME_BUFFERS; ++index) {
        free(state->buffers[index].areas);
    }
    free(state->current);
    free(state->repair);
    free(state->dirty);
    free(state->dirty_joined);
    free(state->unrendered);
    memset(state, 0, sizeof(*state));
}

esp_err_t present_buffer_repair_begin_draw(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *dirty_areas,
    size_t dirty_area_count,
    bool sync)
{
    if (state == NULL || view == NULL || view->target == NULL ||
            view->display_buffer == NULL || view->draw_buffer == NULL ||
            (dirty_areas == NULL) != (dirty_area_count == 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t target = buffer_index(view, view->draw_buffer);
    uint8_t source = buffer_index(view, view->display_buffer);
    bool diff = state->enabled && target != UINT8_MAX &&
                source != UINT8_MAX && target != source &&
                state->buffers[target].valid &&
                state->buffers[source].valid &&
                state->buffers[source].revision == state->revision;
    /* The producer is about to overwrite this buffer; it only becomes a
     * trusted baseline again at record_draw(), so a cancelled frame falls
     * back to a complement copy. */
    if (state->enabled && target != UINT8_MAX) {
        state->buffers[target].valid = false;
    }
    if (!sync) {
        return ESP_OK;
    }

    const esp_display_present_area_t *copy = NULL;
    uint16_t copy_count = 0;
    if (diff) {
        uint16_t current_count = capture_current(
                                     state, view, dirty_areas,
                                     dirty_area_count);
        diff = esp_display_present_subtract_area_list(
                   state->buffers[target].areas, state->buffers[target].count,
                   state->current, current_count, state->repair,
                   state->repair_capacity, &copy_count) == ESP_OK;
        copy = state->repair;
    }
    if (!diff) {
        esp_err_t ret = build_complement(state, view, dirty_areas,
                                         dirty_area_count, &copy_count);
        if (ret != ESP_OK) {
            return ret;
        }
        copy = state->unrendered;
    }
    return copy_areas(view, copy, copy_count);
}

void present_buffer_repair_record_draw(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *drawn_areas,
    size_t drawn_area_count,
    bool committed)
{
    if (state == NULL || view == NULL || !state->enabled) {
        return;
    }
    uint8_t target = buffer_index(view, view->draw_buffer);
    if (!committed || target == UINT8_MAX) {
        reset_to(state, view->buffer_count, UINT8_MAX);
        return;
    }
    const esp_display_present_area_t full = {
        .x1 = 0, .y1 = 0,
        .x2 = view->logical_width - 1,
        .y2 = view->logical_height - 1,
    };
    const esp_display_present_area_t *current = &full;
    uint16_t current_count = 1;
    if (drawn_area_count != 0) {
        current_count = capture_current(state, view, drawn_areas,
                                        drawn_area_count);
        current = state->current;
        if (current_count < drawn_area_count) {
            current = &full;
            current_count = 1;
        }
    }
    commit_diff(state, view->buffer_count, target, current, current_count);
}

esp_err_t present_buffer_repair_apply(
    present_buffer_repair_state_t *state,
    const present_buffer_repair_view_t *view,
    const esp_display_present_area_t *rendered_areas,
    size_t rendered_area_count)
{
    if (state == NULL || view == NULL || view->target == NULL ||
            view->display_buffer == NULL || view->draw_buffer == NULL ||
            view->buffer_count == 0 ||
            (rendered_areas == NULL) != (rendered_area_count == 0)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t target = buffer_index(view, view->draw_buffer);
    uint8_t source = buffer_index(view, view->display_buffer);
    bool diff_active = state->enabled && target != UINT8_MAX &&
                       source != UINT8_MAX;
    if (rendered_area_count == 0) {
        if (diff_active) {
            const esp_display_present_area_t full = {
                .x1 = 0, .y1 = 0,
                .x2 = view->logical_width - 1,
                .y2 = view->logical_height - 1,
            };
            commit_diff(state, view->buffer_count, target, &full, 1);
        }
        return ESP_OK;
    }

    uint16_t current_count = 0;
    repair_plan_t plan = REPAIR_PLAN_BASELINE_COMMIT;
    if (diff_active && state->display_buffer_valid) {
        current_count = capture_current(
                            state, view, rendered_areas, rendered_area_count);
        uint16_t repair_count = 0;
        plan = build_plan(state, target, source, state->current,
                          current_count, &repair_count);
        if (plan == REPAIR_PLAN_DIFF) {
            esp_err_t ret = copy_areas(view, state->repair, repair_count);
            if (ret != ESP_OK) {
                return ret;
            }
            commit_diff(state, view->buffer_count, target, state->current,
                        current_count);
            return ESP_OK;
        }
    }

    uint16_t unrendered_count = 0;
    esp_err_t ret = build_complement(state, view, rendered_areas,
                                     rendered_area_count, &unrendered_count);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = copy_areas(view, state->unrendered, unrendered_count);
    if (ret != ESP_OK) {
        return ret;
    }
    if (diff_active && plan == REPAIR_PLAN_BASELINE_RESET) {
        reset_to(state, view->buffer_count, target);
    } else if (diff_active) {
        commit_diff(state, view->buffer_count, target, state->current,
                    current_count);
    }
    return ESP_OK;
}
