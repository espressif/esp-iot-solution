/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>

#include "esp_check.h"
#include "esp_display_present_dirty.h"

static const char *TAG = "present_dirty";

/*
 * Areas here are inclusive. GSP's corresponding half-open rectangle algebra
 * lives in src/present/gsp_dirty.c; semantic changes must be reviewed in both
 * implementations even though their coordinate encodings intentionally differ.
 */

static inline int present_area_min(int a, int b)
{
    return a < b ? a : b;
}

static inline int present_area_max(int a, int b)
{
    return a > b ? a : b;
}

static bool present_areas_overlap(const esp_display_present_area_t *a,
                                  const esp_display_present_area_t *b)
{
    return !(a->x2 < b->x1 || a->x1 > b->x2 || a->y2 < b->y1 || a->y1 > b->y2);
}

static esp_err_t present_area_append(esp_display_present_area_t *areas,
                                     size_t area_capacity,
                                     uint16_t *area_count,
                                     esp_display_present_area_t area)
{
    if ((size_t)(*area_count) >= area_capacity) {
        return ESP_ERR_NO_MEM;
    }

    areas[*area_count] = area;
    (*area_count)++;
    return ESP_OK;
}

static esp_err_t present_subtract_dirty_area(esp_display_present_area_t *areas,
                                             size_t area_capacity,
                                             uint16_t *area_count,
                                             const esp_display_present_area_t *dirty)
{
    uint16_t idx = 0;

    while (idx < *area_count) {
        esp_display_present_area_t region = areas[idx];

        if (!present_areas_overlap(&region, dirty)) {
            idx++;
            continue;
        }

        for (uint16_t move = idx; move + 1 < *area_count; move++) {
            areas[move] = areas[move + 1];
        }
        (*area_count)--;

        if (dirty->y1 > region.y1) {
            ESP_RETURN_ON_ERROR(present_area_append(areas, area_capacity, area_count,
            (esp_display_present_area_t) {
                .x1 = region.x1,
                .y1 = region.y1,
                .x2 = region.x2,
                .y2 = dirty->y1 - 1,
            }),
            TAG, "append top unsynced area failed");
        }
        if (dirty->y2 < region.y2) {
            ESP_RETURN_ON_ERROR(present_area_append(areas, area_capacity, area_count,
            (esp_display_present_area_t) {
                .x1 = region.x1,
                .y1 = dirty->y2 + 1,
                .x2 = region.x2,
                .y2 = region.y2,
            }),
            TAG, "append bottom unsynced area failed");
        }

        int overlap_y1 = present_area_max(region.y1, dirty->y1);
        int overlap_y2 = present_area_min(region.y2, dirty->y2);

        if (dirty->x1 > region.x1) {
            ESP_RETURN_ON_ERROR(present_area_append(areas, area_capacity, area_count,
            (esp_display_present_area_t) {
                .x1 = region.x1,
                .y1 = overlap_y1,
                .x2 = dirty->x1 - 1,
                .y2 = overlap_y2,
            }),
            TAG, "append left unsynced area failed");
        }
        if (dirty->x2 < region.x2) {
            ESP_RETURN_ON_ERROR(present_area_append(areas, area_capacity, area_count,
            (esp_display_present_area_t) {
                .x1 = dirty->x2 + 1,
                .y1 = overlap_y1,
                .x2 = region.x2,
                .y2 = overlap_y2,
            }),
            TAG, "append right unsynced area failed");
        }
    }

    return ESP_OK;
}

static void present_merge_unrendered_areas(esp_display_present_area_t *areas,
                                           uint16_t *area_count)
{
    bool merged;

    do {
        merged = false;
        for (uint16_t i = 0; i < *area_count; i++) {
            for (uint16_t j = i + 1; j < *area_count; j++) {
                esp_display_present_area_t *a = &areas[i];
                esp_display_present_area_t *b = &areas[j];

                if (a->y1 == b->y1 && a->y2 == b->y2 &&
                        b->x1 <= a->x2 + 1 && b->x2 >= a->x1 - 1) {
                    a->x1 = present_area_min(a->x1, b->x1);
                    a->x2 = present_area_max(a->x2, b->x2);
                } else if (a->x1 == b->x1 && a->x2 == b->x2 &&
                           b->y1 <= a->y2 + 1 && b->y2 >= a->y1 - 1) {
                    a->y1 = present_area_min(a->y1, b->y1);
                    a->y2 = present_area_max(a->y2, b->y2);
                } else {
                    continue;
                }

                for (uint16_t move = j; move + 1 < *area_count; move++) {
                    areas[move] = areas[move + 1];
                }
                (*area_count)--;
                merged = true;
                goto merge_restart;
            }
        }
merge_restart:
        ;
    } while (merged);
}

void esp_display_present_copy_dirty_areas(const esp_display_present_area_t *areas,
                                          const uint8_t *joined,
                                          uint16_t area_count,
                                          void *dst,
                                          const void *src,
                                          esp_display_present_area_copy_cb_t copy_cb,
                                          void *user_ctx)
{
    if (!areas || !joined || !dst || !src || !copy_cb) {
        return;
    }

    for (uint16_t i = 0; i < area_count; i++) {
        if (joined[i] != 0) {
            continue;
        }
        copy_cb(user_ctx, src, dst, &areas[i]);
    }
}

esp_err_t esp_display_present_build_unrendered_area_list(const esp_display_present_area_t *dirty_areas,
                                                         const uint8_t *joined,
                                                         uint16_t dirty_count,
                                                         uint16_t hor_res,
                                                         uint16_t ver_res,
                                                         esp_display_present_area_t *out_areas,
                                                         size_t out_capacity,
                                                         uint16_t *out_count)
{
    ESP_RETURN_ON_FALSE(hor_res > 0 && ver_res > 0 && out_areas && out_count,
                        ESP_ERR_INVALID_ARG, TAG, "invalid unrendered area args");
    ESP_RETURN_ON_FALSE(out_capacity > 0, ESP_ERR_INVALID_ARG, TAG, "scratch capacity is zero");

    uint16_t unsynced_count = 1;
    out_areas[0] = (esp_display_present_area_t) {
        .x1 = 0,
        .y1 = 0,
        .x2 = (int)hor_res - 1,
        .y2 = (int)ver_res - 1,
    };

    if (!dirty_areas || !joined || dirty_count == 0) {
        *out_count = unsynced_count;
        return ESP_OK;
    }

    for (uint16_t i = 0; i < dirty_count; i++) {
        if (joined[i] != 0) {
            continue;
        }

        ESP_RETURN_ON_ERROR(present_subtract_dirty_area(out_areas,
                                                        out_capacity,
                                                        &unsynced_count,
                                                        &dirty_areas[i]),
                            TAG, "subtract dirty area failed");
    }

    if (unsynced_count > 1) {
        present_merge_unrendered_areas(out_areas, &unsynced_count);
    }

    *out_count = unsynced_count;
    return ESP_OK;
}

esp_err_t esp_display_present_subtract_area_list(
    const esp_display_present_area_t *seed_areas,
    uint16_t seed_count,
    const esp_display_present_area_t *cut_areas,
    uint16_t cut_count,
    esp_display_present_area_t *out_areas,
    size_t out_capacity,
    uint16_t *out_count)
{
    ESP_RETURN_ON_FALSE(out_areas && out_count, ESP_ERR_INVALID_ARG, TAG,
                        "invalid subtract args");
    ESP_RETURN_ON_FALSE((size_t)seed_count <= out_capacity, ESP_ERR_NO_MEM, TAG,
                        "seed exceeds scratch capacity");

    uint16_t count = 0;
    for (uint16_t i = 0; i < seed_count; i++) {
        out_areas[count++] = seed_areas[i];
    }

    for (uint16_t i = 0; i < cut_count; i++) {
        ESP_RETURN_ON_ERROR(present_subtract_dirty_area(out_areas, out_capacity,
                                                        &count, &cut_areas[i]),
                            TAG, "subtract cut area failed");
    }

    if (count > 1) {
        present_merge_unrendered_areas(out_areas, &count);
    }

    *out_count = count;
    return ESP_OK;
}

esp_err_t esp_display_present_merge_area_into_list(
    esp_display_present_area_t *list,
    size_t capacity,
    uint16_t *count,
    const esp_display_present_area_t *add_areas,
    uint16_t add_count)
{
    ESP_RETURN_ON_FALSE(list && count, ESP_ERR_INVALID_ARG, TAG,
                        "invalid merge args");

    for (uint16_t i = 0; i < add_count; i++) {
        ESP_RETURN_ON_ERROR(present_area_append(list, capacity, count,
                                                add_areas[i]),
                            TAG, "append merge area failed");
    }

    if (*count > 1) {
        present_merge_unrendered_areas(list, count);
    }

    return ESP_OK;
}
