/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../src/backend/present_image_repair.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static bool fail_coverage_alloc;
void *__real_present_alloc(size_t);
void *__wrap_present_alloc(size_t bytes)
{
    return fail_coverage_alloc ? NULL : __real_present_alloc(bytes);
}
static void lazy_coverage(void)
{
    present_image_builder_t image;
    assert(present_image_init(&image, 1024, 600) == ESP_OK);
    assert(!image.coverage && image.coverage_bytes == 76800);
    assert(present_image_begin(&image, 0, SIZE_MAX) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        0, 0, 1024, 300
    }) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        0, 300, 1024, 300
    }) == ESP_OK);
    assert(present_image_full(&image) && !image.coverage);
    assert(present_image_commit(&image) == ESP_OK);
    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    for (unsigned i = 0; i < PRESENT_MAX_DAMAGE_AREAS; ++i)
        assert(present_image_cover(&image, (esp_display_present_rect_t) {
        i * 2, 0, 1, 1
    }) == ESP_OK);
    fail_coverage_alloc = true;
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        100, 0, 1, 1
    }) == ESP_ERR_NO_MEM);
    fail_coverage_alloc = false;
    assert(!image.coverage && !present_image_full(&image));
    present_image_prepare_repair(&image, false);
    esp_display_present_rect_t area;
    assert(!present_image_next_repair(&image, &area));
    assert(present_image_commit(&image) == ESP_ERR_NO_MEM);
    present_image_cancel(&image);
    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    for (unsigned i = 0; i < 40; ++i)
        assert(present_image_cover(&image, (esp_display_present_rect_t) {
        i * 2, 0, 1, 1
    }) == ESP_OK);
    assert(image.coverage && !present_image_full(&image));
    present_image_cancel(&image);
    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    assert(!image.coverage && image.coverage_error == ESP_OK);
    present_image_cancel(&image);
    present_image_deinit(&image);
}
static uint32_t rng = 0x42abcdef;
static unsigned random_u(unsigned limit)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng % limit;
}
static void rectangle_reference(void)
{
    enum {W = 17, H = 9, N = W * H};
    present_image_builder_t image;
    assert(present_image_init(&image, W, H) == ESP_OK);
    assert(present_image_begin(&image, 0, SIZE_MAX) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        0, 0, W, H
    }) == ESP_OK);
    assert(present_image_commit(&image) == ESP_OK);
    for (unsigned trial = 0; trial < 256; ++trial) {
        bool written[N] = {0};
        bool repaired[N] = {0};
        assert(present_image_begin(&image, 1, 0) == ESP_OK);
        unsigned n = 1 + random_u(80);
        for (unsigned i = 0; i < n; ++i) {
            unsigned x = random_u(W), y = random_u(H);
            unsigned w = 1 + random_u(W - x), h = 1 + random_u(H - y);
            if (trial & 1) {
                w = h = 1;
            }
            assert(present_image_cover(&image, (esp_display_present_rect_t) {
                x, y, w, h
            }) == ESP_OK);
            for (unsigned yy = y; yy < y + h; ++yy)
                for (unsigned xx = x; xx < x + w; ++xx) {
                    written[yy * W + xx] = true;
                }
        }
        bool full = true;
        for (unsigned i = 0; i < N; ++i) {
            full &= written[i];
        }
        assert(present_image_full(&image) == full);
        present_image_prepare_repair(&image, true);
        esp_display_present_rect_t area;
        while (present_image_next_repair(&image, &area)) {
            for (unsigned y = area.y; y < area.y + area.height; ++y)
                for (unsigned x = area.x; x < area.x + area.width; ++x) {
                    assert(!written[y * W + x] && !repaired[y * W + x]);
                    repaired[y * W + x] = true;
                }
            assert(present_image_mark_repaired(&image, &area) == ESP_OK);
        }
        for (unsigned i = 0; i < N; ++i) {
            assert(written[i] != repaired[i]);
        }
        present_image_cancel(&image);
    }
    present_image_deinit(&image);
}
static unsigned rect_pixels(esp_display_present_rect_t area)
{
    return area.width * area.height;
}
/* Seeded baseline pixels are not application coverage and are not repaired again.
 * A seed that only overlaps part of the historical damage still leaves the rest. */
static void seeded_baseline(void)
{
    enum { W = 8, H = 4 };
    present_image_builder_t image;
    assert(present_image_init(&image, W, H) == ESP_OK);
    assert(present_image_begin(&image, 0, SIZE_MAX) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        0, 0, W, H
    }) == ESP_OK);
    assert(present_image_commit(&image) == ESP_OK);

    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    assert(present_image_seed(&image, (esp_display_present_rect_t) {
        0, 0, W, H
    }) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        1, 1, 1, 1
    }) == ESP_OK);
    assert(!present_image_full(&image));
    assert(image.writes.count == 1 && !image.writes.full);
    present_image_prepare_repair(&image, true);
    esp_display_present_rect_t area;
    assert(!present_image_next_repair(&image, &area));
    assert(image.repaired_pixels == 0);
    assert(present_image_commit(&image) == ESP_OK);
    assert(image.history[image.latest % PRESENT_IMAGE_HISTORY].damage.count == 1);

    /* Replace that one-pixel publication with a wide historical write, then
     * seed only the opposite corner. Repair keeps the historical pixels. */
    assert(present_image_begin(&image, 0, 1) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        2, 0, 6, 4
    }) == ESP_OK);
    assert(present_image_commit(&image) == ESP_OK);
    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    assert(present_image_seed(&image, (esp_display_present_rect_t) {
        0, 0, 2, 2
    }) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        0, 0, 1, 1
    }) == ESP_OK);
    present_image_prepare_repair(&image, false);
    unsigned repaired = 0;
    while (present_image_next_repair(&image, &area)) {
        assert(area.x >= 2);
        repaired += rect_pixels(area);
        assert(present_image_mark_repaired(&image, &area) == ESP_OK);
    }
    assert(repaired == 24);
    assert(!present_image_full(&image));
    assert(present_image_commit(&image) == ESP_OK);

    /* The seed bbox overlaps the historical damage. The overlap is not copied
     * again; the historical pixels outside the bbox still are. */
    assert(present_image_begin(&image, 0, 1) == ESP_OK);
    assert(present_image_cover(&image, (esp_display_present_rect_t) {
        6, 0, 2, 2
    }) == ESP_OK);
    assert(present_image_commit(&image) == ESP_OK);
    assert(present_image_begin(&image, 1, 0) == ESP_OK);
    assert(present_image_seed(&image, (esp_display_present_rect_t) {
        5, 0, 2, 2
    }) == ESP_OK);
    present_image_prepare_repair(&image, false);
    repaired = 0;
    while (present_image_next_repair(&image, &area)) {
        assert(!(area.x < 7 && area.x + area.width > 5 && area.y < 2));
        repaired += rect_pixels(area);
        assert(present_image_mark_repaired(&image, &area) == ESP_OK);
    }
    assert(repaired == 2);
    present_image_deinit(&image);
}
int main(void)
{
    lazy_coverage();
    rectangle_reference();
    seeded_baseline();
    enum { W = 17, H = 9, N = W * H };
    present_image_builder_t b;
    assert(present_image_init(&b, W, H) == ESP_OK);
    uint16_t buffers[3][N], reference[N];
    memset(buffers, 0xa5, sizeof(buffers));
    memset(reference, 0, sizeof(reference));
    size_t baseline = SIZE_MAX;
    for (unsigned frame = 0; frame < 80; frame++) {
        size_t candidate = frame % 3;
        present_image_begin(&b, candidate, baseline);
        if (!frame) {
            for (unsigned i = 0; i < N; i++) {
                buffers[candidate][i] = reference[i] = i + 1;
            }
            present_image_cover(&b, (esp_display_present_rect_t) {
                0, 0, W, H
            });
            assert(present_image_full(&b));
        } else {
            unsigned x = frame % W, y = (frame / 3) % H;
            reference[y * W + x] = buffers[candidate][y * W + x] = 1000 + frame;
            present_image_cover(&b, (esp_display_present_rect_t) {
                x, y, 1, 1
            });
            assert(!present_image_full(&b));
        }
        present_image_prepare_repair(&b, false);
        esp_display_present_rect_t a;
        while (present_image_next_repair(&b, &a)) {
            assert(baseline != SIZE_MAX);
            for (unsigned y = a.y; y < a.y + a.height; y++) {
                memcpy(buffers[candidate] + y * W + a.x, buffers[baseline] + y * W + a.x, a.width * 2);
            }
            present_image_mark_repaired(&b, &a);
        }
        assert(!memcmp(buffers[candidate], reference, sizeof(reference)));
        if (frame > 2) {
            assert(b.repaired_pixels <= 2);
        }
        assert(present_image_commit(&b) == ESP_OK);
        baseline = candidate;
    }
    /* Cancel invalidates a contaminated allocation; history overflow cannot
     * turn a conservative dirty union into invented exact coverage. */
    size_t candidate = (baseline + 1) % 3;
    present_image_begin(&b, candidate, baseline);
    present_image_cover(&b, (esp_display_present_rect_t) {
        0, 0, 1, 1
    });
    present_image_cancel(&b);
    assert(!b.versions[candidate]);
    present_image_begin(&b, candidate, baseline);
    for (unsigned i = 0; i < 40; i++)
        present_image_cover(&b, (esp_display_present_rect_t) {
        (i * 2) % W, (i * 2) / W, 1, 1
    });
    assert(b.writes.full && !present_image_full(&b));
    present_image_prepare_repair(&b, false);
    esp_display_present_rect_t a;
    while (present_image_next_repair(&b, &a)) {
        present_image_mark_repaired(&b, &a);
    }
    assert(b.repaired_pixels == N - 40);
    /* Adjacent horizontal bands coalesce without overflowing PRESENT_MAX_DAMAGE_AREAS. */
    present_image_cancel(&b);
    present_image_begin(&b, candidate, baseline);
    for (unsigned y = 0; y < 8; y++) {
        present_image_cover(&b, (esp_display_present_rect_t) {
            2, y, 5, 1
        });
    }
    assert(!b.writes.full && b.writes.count == 1 && b.writes.areas[0].height == 8);
    /* Missing history safely requires a full repair, minus successful writes. */
    present_image_cancel(&b);
    b.versions[candidate] = 1;
    present_image_begin(&b, candidate, baseline);
    present_image_cover(&b, (esp_display_present_rect_t) {
        3, 2, 2, 1
    });
    present_image_prepare_repair(&b, false);
    while (present_image_next_repair(&b, &a)) {
        present_image_mark_repaired(&b, &a);
    }
    assert(b.repaired_pixels == N - 2);
    /* DIRECT protects against writes outside producer-declared coverage. */
    present_image_cancel(&b);
    b.versions[candidate] = b.versions[baseline];
    present_image_begin(&b, candidate, baseline);
    present_image_cover(&b, (esp_display_present_rect_t) {
        3, 2, 2, 1
    });
    present_image_prepare_repair(&b, true);
    while (present_image_next_repair(&b, &a)) {
        present_image_mark_repaired(&b, &a);
    }
    assert(b.repaired_pixels == N - 2);
    present_image_deinit(&b);
    return 0;
}
