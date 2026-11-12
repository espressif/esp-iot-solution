/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_image_repair.h"
#include "backend/present_coverage.h"
#include <string.h>
#include <assert.h>

static bool contains(esp_display_present_rect_t a, esp_display_present_rect_t b)
{
    return b.x >= a.x && b.y >= a.y && b.width <= a.width - (b.x - a.x) &&
           b.x - a.x <= a.width && b.y - a.y <= a.height && b.height <= a.height - (b.y - a.y);
}
static bool merge_adjacent(esp_display_present_rect_t *area, esp_display_present_rect_t b)
{
    if (area->x == b.x && area->width == b.width) {
        uint32_t ay2 = area->y + area->height, by2 = b.y + b.height;
        if (b.y <= ay2 && area->y <= by2) {
            uint32_t y1 = area->y < b.y ? area->y : b.y;
            uint32_t y2 = ay2 > by2 ? ay2 : by2;
            area->y = y1;
            area->height = y2 - y1;
            return true;
        }
    }
    if (area->y == b.y && area->height == b.height) {
        uint32_t ax2 = area->x + area->width, bx2 = b.x + b.width;
        if (b.x <= ax2 && area->x <= bx2) {
            uint32_t x1 = area->x < b.x ? area->x : b.x;
            uint32_t x2 = ax2 > bx2 ? ax2 : bx2;
            area->x = x1;
            area->width = x2 - x1;
            return true;
        }
    }
    return false;
}
static void damage_add(present_damage_set_t *damage, esp_display_present_rect_t a)
{
    if (damage->full) {
        return;
    }
    for (size_t i = 0; i < damage->count; i++) {
        if (contains(damage->areas[i], a)) {
            return;
        }
        if (contains(a, damage->areas[i])) {
            damage->areas[i] = a;
            return;
        }
        if (merge_adjacent(&damage->areas[i], a)) {
            for (size_t j = 0; j < damage->count;) {
                if (j != i && (contains(damage->areas[i], damage->areas[j]) || merge_adjacent(&damage->areas[i], damage->areas[j]))) {
                    damage->areas[j] = damage->areas[--damage->count];
                    if (i == damage->count) {
                        i = j;
                    }
                    j = 0;
                } else {
                    j++;
                }
            }
            return;
        }
    }
    if (damage->count == PRESENT_MAX_DAMAGE_AREAS) {
        damage->full = true;
        damage->count = 0;
        return;
    }
    damage->areas[damage->count++] = a;
}
esp_err_t present_image_init(present_image_builder_t *builder, uint32_t w, uint32_t h)
{
    if (!builder || !w || !h || (size_t)w > SIZE_MAX / h) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(builder, 0, sizeof(*builder));
    size_t pixels = (size_t)w * h;
    builder->coverage_bytes = pixels / 8 + (pixels % 8 != 0);
    /* Common rectangle path has no resident full-screen bitmap. */
    builder->width = w;
    builder->height = h;
    builder->candidate = builder->baseline = builder->published_baseline = SIZE_MAX;
    return ESP_OK;
}
void present_image_deinit(present_image_builder_t *builder)
{
    present_free(builder->coverage);
    builder->coverage = NULL;
}
void present_image_reset(present_image_builder_t *builder, uint32_t w, uint32_t h)
{
    assert((size_t)w * h == (size_t)builder->width * builder->height);
    if (builder->published_baseline != SIZE_MAX) {
        present_allocation_drop(&builder->allocations[builder->published_baseline], PRESENT_REF_BASELINE);
    }
    builder->published_baseline = SIZE_MAX;
    builder->width = w;
    builder->height = h;
    builder->latest = 0;
    memset(builder->versions, 0, sizeof(builder->versions));
    memset(builder->history, 0, sizeof(builder->history));
}
esp_err_t present_image_begin(present_image_builder_t *builder, size_t candidate, size_t baseline)
{
    assert(candidate < PRESENT_IMAGE_SLOTS && (baseline == SIZE_MAX || baseline < PRESENT_IMAGE_SLOTS));
    if (!present_allocation_write(&builder->allocations[candidate], candidate == baseline, builder->allow_scanout_writes)) {
        return ESP_ERR_INVALID_STATE;
    }
    builder->candidate = candidate;
    builder->baseline = baseline;
    builder->baseline_borrow = baseline != SIZE_MAX && baseline != candidate;
    if (builder->baseline_borrow) {
        present_allocation_hold(&builder->allocations[baseline], PRESENT_REF_COPY);
    }
    present_free(builder->coverage);
    builder->coverage = NULL;
    builder->bitmap_dirty = false;
    builder->coverage_error = ESP_OK;
    builder->exact = (present_damage_set_t) {
        0
    };
    builder->full_covered = false;
    builder->writes = (present_damage_set_t) {
        0
    };
    builder->repair = (present_damage_set_t) {
        0
    };
    builder->seeded = (present_damage_set_t) {
        0
    };
    builder->rect = builder->offset = builder->repaired_pixels = 0;
    builder->repair_ready = false;
    return ESP_OK;
}
static void bitmap_cover(present_image_builder_t *builder, esp_display_present_rect_t area)
{
    for (uint32_t y = area.y; y < area.y + area.height; ++y) {
        present_coverage_set(builder->coverage, (size_t)y * builder->width + area.x, area.width);
    }
}
/* Exact union may never widen. On fragmentation overflow preserve every
 * existing rectangle in a lazy bitmap. Allocation failure invalidates this
 * build explicitly; it never authorizes baseline repair over new pixels. */
static esp_err_t cover_exact(present_image_builder_t *builder, esp_display_present_rect_t area)
{
    if (builder->coverage_error != ESP_OK) {
        return builder->coverage_error;
    }
    if (builder->full_covered) {
        return ESP_OK;
    }
    if (area.x == 0 && area.y == 0 && area.width == builder->width && area.height == builder->height) {
        builder->full_covered = true;
        return ESP_OK;
    }
    if (builder->coverage) {
        bitmap_cover(builder, area);
        return ESP_OK;
    }
    present_damage_set_t previous = builder->exact;
    damage_add(&builder->exact, area);
    if (!builder->exact.full && builder->exact.count == 1 &&
            builder->exact.areas[0].x == 0 && builder->exact.areas[0].y == 0 &&
            builder->exact.areas[0].width == builder->width && builder->exact.areas[0].height == builder->height) {
        builder->full_covered = true;
    }
    if (builder->exact.full) {
        builder->coverage = present_alloc(builder->coverage_bytes);
        if (!builder->coverage) {
            builder->exact = previous;
            return builder->coverage_error = ESP_ERR_NO_MEM;
        }
        for (size_t i = 0; i < previous.count; ++i) {
            bitmap_cover(builder, previous.areas[i]);
        }
        bitmap_cover(builder, area);
    }
    return ESP_OK;
}
esp_err_t present_image_cover(present_image_builder_t *builder, esp_display_present_rect_t area)
{
    assert(present_valid_rect(builder->width, builder->height, &area));
    esp_err_t error = cover_exact(builder, area);
    if (error != ESP_OK) {
        return error;
    }
    damage_add(&builder->writes, area);
    if (builder->full_covered) {
        builder->writes.full = true;
        builder->writes.count = 0;
    }
    return ESP_OK;
}
esp_err_t present_image_seed(present_image_builder_t *builder, esp_display_present_rect_t area)
{
    assert(present_valid_rect(builder->width, builder->height, &area));
    damage_add(&builder->seeded, area);
    return ESP_OK;
}
static uint32_t exact_run_end(const present_image_builder_t *builder, uint32_t x, uint32_t y)
{
    uint32_t end = x;
    for (size_t pass = 0; pass < builder->exact.count; ++pass) {
        uint32_t before = end;
        for (size_t i = 0; i < builder->exact.count; ++i) {
            esp_display_present_rect_t a = builder->exact.areas[i];
            if (y >= a.y && y - a.y < a.height && a.x <= end && a.x + a.width > end) {
                end = a.x + a.width;
            }
        }
        if (before == end) {
            break;
        }
    }
    return end;
}
static uint32_t seeded_run_end(const present_image_builder_t *builder, uint32_t x, uint32_t y)
{
    if (builder->seeded.full) {
        return builder->width;
    }
    uint32_t end = x;
    for (size_t pass = 0; pass < builder->seeded.count; ++pass) {
        uint32_t before = end;
        for (size_t i = 0; i < builder->seeded.count; ++i) {
            esp_display_present_rect_t a = builder->seeded.areas[i];
            if (y >= a.y && y - a.y < a.height && a.x <= end && a.x + a.width > end) {
                end = a.x + a.width;
            }
        }
        if (before == end) {
            break;
        }
    }
    return end;
}
static bool seeded_contains(const present_image_builder_t *builder, uint32_t x, uint32_t y)
{
    if (builder->seeded.full) {
        return true;
    }
    for (size_t i = 0; i < builder->seeded.count; ++i) {
        esp_display_present_rect_t a = builder->seeded.areas[i];
        if (x >= a.x && y >= a.y && x - a.x < a.width && y - a.y < a.height) {
            return true;
        }
    }
    return false;
}
bool present_image_full(const present_image_builder_t *builder)
{
    if (builder->coverage_error != ESP_OK) {
        return false;
    }
    if (builder->full_covered) {
        return true;
    }
    if (builder->coverage) {
        return present_coverage_full(builder->coverage, (size_t)builder->width * builder->height);
    }
    for (uint32_t y = 0; y < builder->height; ++y)
        if (exact_run_end(builder, 0, y) != builder->width) {
            return false;
        }
    return true;
}

void present_image_prepare_repair(present_image_builder_t *builder, bool direct)
{
    if (builder->repair_ready) {
        return;
    }
    builder->repair_ready = true;
    if (builder->coverage_error != ESP_OK || builder->full_covered || builder->baseline == SIZE_MAX || builder->baseline == builder->candidate) {
        return;
    }
    uint64_t from = builder->versions[builder->candidate], to = builder->versions[builder->baseline];
    if (direct || !from || !to || from > to || to - from > PRESENT_IMAGE_HISTORY) {
        builder->repair.full = true;
    } else
        for (uint64_t v = from; v < to;) {
            ++v;
            present_image_delta_t *delta = &builder->history[v % PRESENT_IMAGE_HISTORY];
            if (delta->version != v || delta->damage.full) {
                builder->repair.full = true;
                break;
            }
            for (size_t i = 0; i < delta->damage.count; i++) {
                damage_add(&builder->repair, delta->damage.areas[i]);
            }
        }
    if (builder->repair.full) {
        builder->repair.count = 1;
        builder->repair.areas[0] = (esp_display_present_rect_t) {
            0, 0, builder->width, builder->height
        };
    }
}
static bool covered(const present_image_builder_t *builder, size_t i)
{
    if (builder->full_covered || seeded_contains(builder, (uint32_t)(i % builder->width), (uint32_t)(i / builder->width))) {
        return true;
    }
    if (builder->coverage) {
        return (builder->coverage[i / 8] & (1u << (i % 8))) != 0;
    }
    uint32_t x = i % builder->width, y = i / builder->width;
    return exact_run_end(builder, x, y) > x;
}
static bool row_uncovered(const present_image_builder_t *builder, uint32_t y, uint32_t x, uint32_t width)
{
    if (builder->full_covered || builder->seeded.full) {
        return false;
    }
    if (builder->coverage) {
        return present_coverage_all_zero(builder->coverage, (size_t)y * builder->width + x, width);
    }
    for (size_t i = 0; i < builder->exact.count; ++i) {
        esp_display_present_rect_t a = builder->exact.areas[i];
        if (y >= a.y && y - a.y < a.height && a.x < x + width && x < a.x + a.width) {
            return false;
        }
    }
    for (size_t i = 0; i < builder->seeded.count; ++i) {
        esp_display_present_rect_t a = builder->seeded.areas[i];
        if (y >= a.y && y - a.y < a.height && a.x < x + width && x < a.x + a.width) {
            return false;
        }
    }
    return true;
}
bool present_image_next_repair(present_image_builder_t *builder, esp_display_present_rect_t *out)
{
    assert(builder->repair_ready);
    if (builder->coverage_error != ESP_OK) {
        return false;
    }
    while (builder->rect < builder->repair.count) {
        esp_display_present_rect_t a = builder->repair.areas[builder->rect];
        size_t count = (size_t)a.width * a.height;
        uint32_t ry = (uint32_t)(builder->offset / a.width);
        uint32_t rx = (uint32_t)(builder->offset % a.width);
        while (ry < a.height) {
            uint32_t y = a.y + ry;
            size_t row_base = (size_t)y * builder->width;
            /* Fast skip over covered pixels on this row, 8 pixels at a time when byte-aligned. */
            while (rx < a.width) {
                size_t bit = row_base + a.x + rx;
                if (builder->coverage && bit % 8 == 0 && rx + 8 <= a.width && builder->coverage[bit / 8] == 0xff) {
                    rx += 8;
                    continue;
                }
                if (!builder->coverage) {
                    uint32_t end = builder->full_covered ? a.x + a.width : exact_run_end(builder, a.x + rx, y);
                    uint32_t seeded_end = seeded_run_end(builder, a.x + rx, y);
                    if (seeded_end > end) {
                        end = seeded_end;
                    }
                    if (end == a.x + rx) {
                        break;
                    }
                    rx = end < a.x + a.width ? end - a.x : a.width;
                    continue;
                }
                if (!covered(builder, bit)) {
                    break;
                }
                rx++;
            }
            if (rx >= a.width) {
                ry++;
                rx = 0;
                continue;
            }
            uint32_t x = a.x + rx;
            uint32_t end = x + 1;
            uint32_t limit = a.x + a.width;
            if (!builder->coverage) {
                /* Find the next covered interval from rectangle boundaries. */
                end = limit;
                for (size_t i = 0; i < builder->exact.count; ++i) {
                    esp_display_present_rect_t covered_area = builder->exact.areas[i];
                    if (y >= covered_area.y && y - covered_area.y < covered_area.height &&
                            covered_area.x > x && covered_area.x < end) {
                        end = covered_area.x;
                    }
                }
                for (size_t i = 0; i < builder->seeded.count; ++i) {
                    esp_display_present_rect_t covered_area = builder->seeded.areas[i];
                    if (y >= covered_area.y && y - covered_area.y < covered_area.height &&
                            covered_area.x > x && covered_area.x < end) {
                        end = covered_area.x;
                    }
                }
            }
            while (builder->coverage && end < limit) {
                size_t bit = row_base + end;
                if (builder->coverage && bit % 8 == 0 && end + 8 <= limit && builder->coverage[bit / 8] == 0x00) {
                    end += 8;
                    continue;
                }
                if (covered(builder, bit)) {
                    break;
                }
                end++;
            }
            uint32_t span_w = end - x;
            uint32_t rows = 1;
            while (ry + rows < a.height &&
                    row_uncovered(builder, y + rows, x, span_w)) {
                rows++;
            }
            uint32_t next_rx = (end - a.x);
            uint32_t next_ry = ry;
            if (next_rx >= a.width) {
                next_rx = 0;
                next_ry = (rx == 0) ? (ry + rows) : (ry + 1);
            }
            builder->offset = (size_t)next_ry * a.width + next_rx;
            *out = (esp_display_present_rect_t) {
                x, y, span_w, rows
            };
            return true;
        }
        builder->offset = count;
        builder->rect++;
        builder->offset = 0;
    }
    return false;
}
esp_err_t present_image_mark_repaired(present_image_builder_t *builder, const esp_display_present_rect_t *area)
{
    esp_err_t error = cover_exact(builder, *area);
    if (error == ESP_OK) {
        builder->repaired_pixels += (size_t)area->width * area->height;
    }
    return error;
}

esp_err_t present_image_commit(present_image_builder_t *builder)
{
    if (builder->coverage_error != ESP_OK) {
        return builder->coverage_error;
    }
    if (builder->latest == UINT64_MAX) {
        return ESP_ERR_NO_MEM;
    }
    present_allocation_written(&builder->allocations[builder->candidate]);
    if (builder->baseline_borrow) {
        present_allocation_drop(&builder->allocations[builder->baseline], PRESENT_REF_COPY);
        builder->baseline_borrow = false;
    }
    if (builder->published_baseline != SIZE_MAX) {
        present_allocation_drop(&builder->allocations[builder->published_baseline], PRESENT_REF_BASELINE);
    }
    present_allocation_hold(&builder->allocations[builder->candidate], PRESENT_REF_BASELINE);
    builder->published_baseline = builder->candidate;
    uint64_t v = ++builder->latest;
    builder->versions[builder->candidate] = v;
    builder->history[v % PRESENT_IMAGE_HISTORY] = (present_image_delta_t) {
        v, builder->writes
    };
    return ESP_OK;
}
void present_image_cancel(present_image_builder_t *builder)
{
    if (builder->candidate < PRESENT_IMAGE_SLOTS) {
        if (builder->allocations[builder->candidate].writer) {
            present_allocation_written(&builder->allocations[builder->candidate]);
        }
        if (builder->published_baseline == builder->candidate) {
            present_allocation_drop(&builder->allocations[builder->candidate], PRESENT_REF_BASELINE);
            builder->published_baseline = SIZE_MAX;
        }
        builder->versions[builder->candidate] = 0;
    }
    if (builder->baseline_borrow) {
        present_allocation_drop(&builder->allocations[builder->baseline], PRESENT_REF_COPY);
        builder->baseline_borrow = false;
    }
}
