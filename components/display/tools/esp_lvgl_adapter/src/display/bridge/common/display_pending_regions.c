/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display_pending_regions.h"

static bool rects_touch_or_overlap(const lv_area_t *a, const lv_area_t *b)
{
    bool x_overlap = a->x1 <= b->x2 && a->x2 >= b->x1;
    bool y_overlap = a->y1 <= b->y2 && a->y2 >= b->y1;
    bool x_adjacent = (int64_t)a->x2 + 1 == b->x1 || (int64_t)b->x2 + 1 == a->x1;
    bool y_adjacent = (int64_t)a->y2 + 1 == b->y1 || (int64_t)b->y2 + 1 == a->y1;
    return (x_overlap && y_overlap) || (x_adjacent && y_overlap) ||
           (y_adjacent && x_overlap);
}

static uint64_t rect_area(const lv_area_t *rect)
{
    return (uint64_t)((int64_t)rect->x2 - rect->x1 + 1) *
           (uint64_t)((int64_t)rect->y2 - rect->y1 + 1);
}

static lv_area_t rect_union(const lv_area_t *a, const lv_area_t *b)
{
    return (lv_area_t) {
        .x1 = LV_MIN(a->x1, b->x1), .y1 = LV_MIN(a->y1, b->y1),
        .x2 = LV_MAX(a->x2, b->x2), .y2 = LV_MAX(a->y2, b->y2),
    };
}

bool display_pending_regions_add(lv_area_t *rects, uint16_t *count,
                                 uint16_t capacity, lv_area_t rect)
{
    if (!rects || !count || !capacity || *count > capacity ||
            rect.x2 < rect.x1 || rect.y2 < rect.y1) {
        return false;
    }

    for (uint16_t index = 0; index < *count;) {
        if (rects_touch_or_overlap(&rects[index], &rect)) {
            rect = rect_union(&rects[index], &rect);
            rects[index] = rects[--(*count)];
            index = 0;
        } else {
            ++index;
        }
    }
    if (*count < capacity) {
        rects[(*count)++] = rect;
        return true;
    }

    /* The incoming rectangle is now disjoint and its area is constant,
     * so compare bounding area minus existing area to minimize inflation. */
    uint16_t best = 0;
    uint64_t best_cost = UINT64_MAX;
    for (uint16_t index = 0; index < *count; ++index) {
        lv_area_t joined = rect_union(&rects[index], &rect);
        uint64_t cost = rect_area(&joined) - rect_area(&rects[index]);
        if (cost < best_cost) {
            best = index;
            best_cost = cost;
        }
    }
    rects[best] = rect_union(&rects[best], &rect);
    return true;
}
