/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_display_present_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DISPLAY_PRESENT_UNRENDERED_AREA_SCRATCH_COUNT(dirty_count) \
    ((size_t)(dirty_count) * 4U + 4U)

typedef void (*esp_display_present_area_copy_cb_t)(void *user_ctx,
                                                   const void *src,
                                                   void *dst,
                                                   const esp_display_present_area_t *area);

void esp_display_present_copy_dirty_areas(const esp_display_present_area_t *areas,
                                          const uint8_t *joined,
                                          uint16_t area_count,
                                          void *dst,
                                          const void *src,
                                          esp_display_present_area_copy_cb_t copy_cb,
                                          void *user_ctx);

esp_err_t esp_display_present_build_unrendered_area_list(const esp_display_present_area_t *dirty_areas,
                                                         const uint8_t *joined,
                                                         uint16_t dirty_count,
                                                         uint16_t hor_res,
                                                         uint16_t ver_res,
                                                         esp_display_present_area_t *out_areas,
                                                         size_t out_capacity,
                                                         uint16_t *out_count);

/**
 * Compute out = seed_areas minus the union of cut_areas (inclusive rectangle
 * algebra), then coalesce edge-adjacent survivors. Both inputs may alias none
 * of the output. Returns ESP_ERR_NO_MEM if the intermediate list exceeds
 * out_capacity so the caller can fall back to a full-surface copy.
 */
esp_err_t esp_display_present_subtract_area_list(
    const esp_display_present_area_t *seed_areas,
    uint16_t seed_count,
    const esp_display_present_area_t *cut_areas,
    uint16_t cut_count,
    esp_display_present_area_t *out_areas,
    size_t out_capacity,
    uint16_t *out_count);

/**
 * Append add_areas into list and coalesce edge-adjacent rectangles in place.
 * Returns ESP_ERR_NO_MEM if the merged list would exceed capacity; on failure
 * the list content is left unspecified and the caller should invalidate it.
 */
esp_err_t esp_display_present_merge_area_into_list(
    esp_display_present_area_t *list,
    size_t capacity,
    uint16_t *count,
    const esp_display_present_area_t *add_areas,
    uint16_t add_count);

#ifdef __cplusplus
}
#endif
