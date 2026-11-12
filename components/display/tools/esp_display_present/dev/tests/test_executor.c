/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "pixel/present_executor.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if PRESENT_HOST_DMA2D
#include <pthread.h>
#endif
#if PRESENT_HOST_PPA
#include "driver/ppa.h"
#endif
#if PRESENT_HOST_DMA2D
#include "esp_private/async_memcpy_dma2d.h"
#endif
#if PRESENT_HOST_PPA || PRESENT_HOST_DMA2D
#include "esp_cache.h"
static unsigned submissions, syncs;
static unsigned source_flushes, flushes, invalidates;
static void *expected_sync;
static size_t expected_sync_bytes;
static void *expected_source;
static size_t expected_source_bytes;
static size_t cache_line = 64;
static size_t strict_alignment = 1;
static esp_err_t driver_error, unregister_error, cache_error;
static int cache_error_flags;
size_t dma2d_get_buffer_alignment_constraint(const void *p)
{
    (void)p;
    return strict_alignment;
}
size_t esp_cache_get_line_size_by_addr(const void *p)
{
    (void)p;
    return cache_line;
}
esp_err_t esp_cache_msync(void *p, size_t n, int flags)
{
    if (flags == (ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED)) {
        if (expected_source) {
            assert(p == expected_source && n == expected_source_bytes);
        }
        source_flushes++;
    } else if (flags == ESP_CACHE_MSYNC_FLAG_DIR_C2M) {
        assert((uintptr_t)p % cache_line == 0 && n % cache_line == 0);
        if (expected_sync) {
            assert(p == expected_sync && n == expected_sync_bytes);
        }
        flushes++;
    } else {
        assert(flags == ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        assert((uintptr_t)p % cache_line == 0 && n % cache_line == 0);
        if (expected_sync) {
            assert(p == expected_sync && n == expected_sync_bytes);
        }
        invalidates++;
    }
    syncs++;
    return flags == cache_error_flags ? cache_error : ESP_OK;
}
#endif
/* Independent pixel oracle, also used to interpret fake driver descriptors. */
static void paint(const uint8_t *src, size_t ss, uint8_t *dst, size_t ds,
                  unsigned sx, unsigned sy, unsigned w, unsigned h,
                  unsigned dx, unsigned dy, unsigned rot, unsigned bpp, bool swap)
{
    for (unsigned y = 0; y < h; y++) {
        for (unsigned x = 0; x < w; x++) {
            unsigned ox = x, oy = y;
            if (rot == 1) {
                ox = h - 1 - y;
                oy = x;
            }
            if (rot == 2) {
                ox = w - 1 - x;
                oy = h - 1 - y;
            }
            if (rot == 3) {
                ox = y;
                oy = w - 1 - x;
            }
            for (unsigned b = 0; b < bpp; b++) {
                dst[(dy + oy)*ds + (dx + ox)*bpp + b] =
                    src[(sy + y) * ss + (sx + x) * bpp + (swap ? bpp - b - 1 : b)];
            }
        }
    }
}
#if PRESENT_HOST_PPA
esp_err_t ppa_register_client(const ppa_client_config_t *c, ppa_client_handle_t *h)
{
    assert(c->max_pending_trans_num == 1);
    *h = (void *)1;
    return ESP_OK;
}
esp_err_t ppa_unregister_client(ppa_client_handle_t h)
{
    assert(h);
    return unregister_error;
}
esp_err_t ppa_do_scale_rotate_mirror(ppa_client_handle_t h, const ppa_srm_oper_config_t *o)
{
    assert(h);
    submissions++;
    if (driver_error) {
        return driver_error;
    }
    unsigned bpp = o->in.srm_cm == PPA_SRM_COLOR_MODE_RGB565 ? 2 : 3;
    paint(o->in.buffer, o->in.pic_w * bpp, o->out.buffer, o->out.pic_w * bpp,
          o->in.block_offset_x, o->in.block_offset_y, o->in.block_w, o->in.block_h,
          o->out.block_offset_x, o->out.block_offset_y, (4 - o->rotation_angle) % 4, bpp, o->byte_swap);
    return ESP_OK;
}
#endif
#if PRESENT_HOST_PPA
/* Real P4 failure: (466,248) 131x352 RGB565 on rev3. The CPU fallback must
 * preserve both the copied region and pixels outside it; adjacent widths
 * remain eligible for SRM, and rejection must not touch cache or hardware. */
static void verify_fifo_boundary(esp_display_present_executor_t *ppa, esp_display_present_executor_t *cpu)
{
    const size_t allocation = 1024 * 600 * 3;
    uint8_t *src = aligned_alloc(64, allocation), *dst = aligned_alloc(64, allocation);
    uint8_t *expected = malloc(allocation);
    assert(src && dst && expected);
    for (size_t i = 0; i < allocation; ++i) {
        src[i] = (uint8_t)(i * 37 + i / 1024);
    }
    for (unsigned bpp = 2; bpp <= 3; ++bpp) {
        unsigned width = PRESENT_HOST_PPA_MB_HEIGHT == 32 ? (bpp == 2 ? 131 : 34) : (bpp == 2 ? 134 : 36);
        size_t bytes = 1024 * 600 * bpp;
        esp_display_present_layout_t layout = {1024, 600, 1024 * bpp,
                                               bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888,
                                               ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                              };
        esp_display_present_copy_request_t request = {
            .src = {src, bytes, layout}, .dst = {dst, bytes, layout},
            .src_area = {466, 248, width, 352}, .dst_x = 466, .dst_y = 248
        };
        unsigned before = submissions, before_syncs = syncs;
        esp_display_present_executor_reason_t reason;
        assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_ERR_NOT_SUPPORTED);
        assert(reason == ESP_DISPLAY_PRESENT_EXEC_LAYOUT);
        assert(esp_display_present_executor_execute(ppa, &request) == ESP_ERR_NOT_SUPPORTED);
        assert(submissions == before && syncs == before_syncs);
        memset(dst, 0xa5, bytes);
        memset(expected, 0xa5, bytes);
        paint(src, layout.stride_bytes, expected, layout.stride_bytes, 466, 248, width, 352, 466, 248, 0, bpp, false);
        esp_display_present_executor_dispatch_t dispatch = {ppa, cpu};
        esp_display_present_executor_selection_t selected;
        assert(esp_display_present_executor_run(&dispatch, &request, &selected) == ESP_OK);
        assert(selected.executor == cpu && selected.used_fallback);
        assert(submissions == before && syncs == before_syncs && !memcmp(dst, expected, bytes));
        for (int adjacent = -1; adjacent <= 1; adjacent += 2) {
            request.src_area.width = width + adjacent;
            assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
        }
        request.src_area.width = width;
        request.src_area.height = 351;
        assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
        request.src_area.height = 353;
        request.src_area.y = request.dst_y = 247;
        assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
        /* Equality inside a single macro block does not hit the cut-unit bug. */
        request.src_area = (esp_display_present_rect_t) {
            0, 0,
            PRESENT_HOST_PPA_MB_HEIGHT == 32 ? (bpp == 2 ? 3 : 2) : (bpp == 2 ? 6 : 4),
            PRESENT_HOST_PPA_MB_HEIGHT
        };
        request.dst_x = request.dst_y = 0;
        assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
    }
    free(src);
    free(dst);
    free(expected);
}
/* Quarter-turn output tails differ from IDF's input-dimension workaround.
 * The 128x130 RGB565 request blocked after 18 isolated hardware calls on P4.
 * Probe is pure; CPU fallback must preserve the entire destination allocation. */
static void verify_rotated_fifo_tail(esp_display_present_executor_t *ppa, esp_display_present_executor_t *cpu)
{
    const size_t capacity = 1024 * 600 * 3;
    uint8_t *src = aligned_alloc(64, capacity), *dst = aligned_alloc(64, capacity);
    uint8_t *expected = malloc(capacity);
    assert(src && dst && expected);
    for (size_t i = 0; i < capacity; ++i) {
        src[i] = (uint8_t)(i * 37 + i / 128);
    }
    for (unsigned bpp = 2; bpp <= 3; ++bpp) {
        esp_display_present_engine_pixel_format_t format = bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888;
        for (unsigned rotation = 1; rotation <= 3; rotation += 2) {
            esp_display_present_copy_request_t request = {
                .src = {src, 128 * 160 * bpp, {128, 160, 128 * bpp, format, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
                .dst = {dst, 1024 * 600 * bpp, {1024, 600, 1024 * bpp, format, ESP_DISPLAY_PRESENT_ORDER_NATIVE}},
                .src_area = {0, 0, 128, 130}, .dst_x = 727, .dst_y = 353, .rotation = rotation
            };
            esp_display_present_executor_reason_t reason;
            unsigned before = submissions, before_syncs = syncs;
            assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_ERR_NOT_SUPPORTED);
            assert(reason == ESP_DISPLAY_PRESENT_EXEC_LAYOUT);
            assert(esp_display_present_executor_execute(ppa, &request) == ESP_ERR_NOT_SUPPORTED);
            assert(submissions == before && syncs == before_syncs);
            memset(dst, 0xa5, capacity); memset(expected, 0xa5, capacity);
            paint(src, 128 * bpp, expected, 1024 * bpp, 0, 0, 128, 130, 727, 353, rotation, bpp, false);
            esp_display_present_executor_dispatch_t dispatch = {ppa, cpu};
            esp_display_present_executor_selection_t selected;
            assert(esp_display_present_executor_run(&dispatch, &request, &selected) == ESP_OK);
            assert(selected.executor == cpu && selected.used_fallback);
            assert(submissions == before && syncs == before_syncs && !memcmp(dst, expected, capacity));
            /* Small input tails already enable IDF's bypass; large output tails
             * are safe. Neither neighbor should lose hardware acceleration. */
            request.src_area.height = 129;
            assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
            request.src_area.height = 137;
            assert(esp_display_present_executor_probe(ppa, &request, &reason) == ESP_OK);
        }
    }
    free(src); free(dst); free(expected);
}

#endif
#if PRESENT_HOST_DMA2D
static pthread_mutex_t dma_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dma_changed = PTHREAD_COND_INITIALIZER;
static bool dma_deferred, dma_entered;
static unsigned dma_busy_count;
static async_memcpy_dma2d_trans_desc_t dma_pending;
static async_memcpy_dma2d_isr_cb_t dma_pending_cb;
static async_memcpy_dma2d_handle_t dma_pending_handle;
static void *dma_pending_user;

esp_err_t esp_async_memcpy_install_dma2d(const async_memcpy_dma2d_config_t *c, async_memcpy_dma2d_handle_t *h)
{
    assert(c->backlog == 1);
    *h = (void *)2;
    return ESP_OK;
}
esp_err_t esp_async_memcpy_uninstall_dma2d(async_memcpy_dma2d_handle_t h)
{
    assert(h);
    return unregister_error;
}
esp_err_t esp_async_memcpy_dma2d(async_memcpy_dma2d_handle_t h, const async_memcpy_dma2d_trans_desc_t *o,
                                 async_memcpy_dma2d_isr_cb_t cb, void *user)
{
    submissions++;
    if (dma_busy_count) {
        dma_busy_count--;
        return ESP_ERR_INVALID_STATE;
    }
    if (driver_error) {
        return driver_error;
    }
    pthread_mutex_lock(&dma_lock);
    if (dma_deferred) {
        dma_pending = *o;
        dma_pending_cb = cb;
        dma_pending_handle = h;
        dma_pending_user = user;
        dma_entered = true;
        pthread_cond_signal(&dma_changed);
        pthread_mutex_unlock(&dma_lock);
        return ESP_OK;
    }
    pthread_mutex_unlock(&dma_lock);
    unsigned bpp = o->pixel_format == ESP_COLOR_FOURCC_RGB16 ? 2 : 3;
    paint(o->src_buffer, o->src_stride * bpp, o->dst_buffer, o->dst_stride * bpp,
          o->src_x, o->src_y, o->copy_width, o->copy_height, o->dst_x, o->dst_y, 0, bpp, false);
    cb(h, NULL, user);
    return ESP_OK;
}
#endif

#if PRESENT_HOST_PPA || PRESENT_HOST_DMA2D
/* Cache maintenance must cover exactly the destination rows, including the
 * extra rotated row and the last cache line. The compact source must not be
 * rejected merely because its valid capacity ends before a cache line. */
static void verify_cache_bands(esp_display_present_executor_t *executor, bool is_dma)
{
    uint8_t *src = aligned_alloc(128, 3072), *dst = aligned_alloc(128, 3072);
    uint8_t expected[3072];
    assert(src && dst);
    for (size_t i = 0; i < 3072; i++) {
        src[i] = (uint8_t)i;
    }
    esp_display_present_layout_t layout = {32, 32, 96, ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                           ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                          };
    esp_display_present_copy_request_t request = {
        .src = {src, 3072, layout}, .dst = {dst, 3072, layout},
        .src_area = {1, 1, 3, 2}, .dst_x = 1,
    };
    for (unsigned rotation = 0; rotation < (is_dma ? 1U : 4U); rotation++) {
        request.rotation = rotation;
        for (unsigned last = 0; last < 2; last++) {
            bool quarter = rotation & 1U;
            request.dst_y = last ? (quarter ? 29 : 30) : 3;
            memset(dst, 0xa5, 3072);
            memset(expected, 0xa5, 3072);
            paint(src, 96, expected, 96, 1, 1, 3, 2, 1, request.dst_y, rotation, 2, false);
            expected_sync = dst + (last ? (quarter ? 2752 : 2880) : 256);
            expected_sync_bytes = last ? (quarter ? 320 : 192) : (quarter ? 320 : 256);
            expected_source = src + 96;
            expected_source_bytes = 192;
            unsigned before_sources = source_flushes, before_flushes = flushes, before_invalidates = invalidates;
            assert(esp_display_present_executor_execute(executor, &request) == ESP_OK);
            assert(source_flushes == before_sources + 1 && flushes == before_flushes + 1 && invalidates == before_invalidates + 1);
            assert(!memcmp(dst, expected, 3072));
            expected_sync = NULL;
            expected_source = NULL;
        }
    }
    esp_display_present_layout_t compact = {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                            ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                           };
    request = (esp_display_present_copy_request_t) {
        .src = {src, 32, compact}, .dst = {dst, 64, compact},
        .src_area = {0, 0, 4, 4},
    };
    expected_sync = dst;
    expected_sync_bytes = 64;
    expected_source = src;
    expected_source_bytes = 32;
    unsigned before_submissions = submissions, before_sources = source_flushes;
    unsigned before_flushes = flushes, before_invalidates = invalidates;
    assert(esp_display_present_executor_execute(executor, &request) == ESP_OK);
    assert(submissions == before_submissions + 1);
    assert(source_flushes == before_sources + 1 && flushes == before_flushes + 1 && invalidates == before_invalidates + 1);
    assert(!memcmp(src, dst, 32));
    expected_sync = NULL;
    expected_source = NULL;

    /* A larger runtime cache line requires a correspondingly padded
     * destination; rejection must have no hardware or cache side effects. */
    cache_line = 128;
    esp_display_present_executor_reason_t reason;
    before_submissions = submissions;
    unsigned before_syncs = syncs;
    assert(esp_display_present_executor_probe(executor, &request, &reason) == ESP_ERR_NOT_SUPPORTED);
    assert(reason == ESP_DISPLAY_PRESENT_EXEC_CAPACITY);
    assert(submissions == before_submissions && syncs == before_syncs);
    request.dst.capacity_bytes = 128;
    expected_sync = dst;
    expected_sync_bytes = 128;
    expected_source = src;
    expected_source_bytes = 32;
    assert(esp_display_present_executor_execute(executor, &request) == ESP_OK);
    assert(submissions == before_submissions + 1 && !memcmp(src, dst, 32));
    expected_sync = NULL;
    expected_source = NULL;
    cache_line = 64;
    free(src);
    free(dst);
}

static void verify_cache_failures(esp_display_present_executor_t *executor)
{
    uint8_t *src = aligned_alloc(64, 64), *dst = aligned_alloc(64, 64);
    assert(src && dst);
    esp_display_present_layout_t layout = {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                           ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                          };
    esp_display_present_copy_request_t request = {
        .src = {src, 32, layout}, .dst = {dst, 64, layout}, .src_area = {0, 0, 4, 4},
    };
    const int stages[] = {
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M,
        ESP_CACHE_MSYNC_FLAG_DIR_M2C,
    };
    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
        unsigned before = submissions;
        cache_error = ESP_FAIL;
        cache_error_flags = stages[i];
        assert(esp_display_present_executor_execute(executor, &request) == ESP_FAIL);
        assert(submissions == before + (i == 2));
    }
    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); ++i) {
        unsigned before = submissions;
        cache_error = ESP_ERR_NOT_SUPPORTED;
        cache_error_flags = stages[i];
        assert(esp_display_present_executor_execute(executor, &request) == ESP_OK);
        assert(submissions == before + 1);
    }
    cache_error = ESP_OK;
    cache_error_flags = 0;
    free(src);
    free(dst);
}

static void verify_instance_isolation(bool is_dma)
{
    esp_display_present_executor_t *a = NULL, *b = NULL;
    esp_err_t (*create)(esp_display_present_executor_t **) = is_dma ?
                                                             esp_display_present_executor_dma2d_create : esp_display_present_executor_ppa_create;
    assert(create(&a) == ESP_OK && create(&b) == ESP_OK);
    uint8_t *src = aligned_alloc(64, 64), *dst = aligned_alloc(64, 64);
    assert(src && dst);
    memset(src, 0x5a, 32);
    esp_display_present_layout_t layout = {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                           ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                          };
    esp_display_present_copy_request_t request = {
        .src = {src, 32, layout}, .dst = {dst, 64, layout}, .src_area = {0, 0, 4, 4},
    };
    assert(esp_display_present_executor_execute(a, &request) == ESP_OK);
    assert(esp_display_present_executor_execute(b, &request) == ESP_OK);
    assert(esp_display_present_executor_destroy(a) == ESP_OK);
    memset(dst, 0, 32);
    assert(esp_display_present_executor_execute(b, &request) == ESP_OK && !memcmp(src, dst, 32));
    assert(esp_display_present_executor_destroy(b) == ESP_OK);
    free(src);
    free(dst);
}
#endif

#if PRESENT_HOST_DMA2D
static void verify_dma_retry_and_drain(esp_display_present_executor_t *executor)
{
    uint8_t *src = aligned_alloc(64, 64), *dst = aligned_alloc(64, 64);
    assert(src && dst);
    for (unsigned i = 0; i < 64; i++) {
        src[i] = (uint8_t)i;
    }
    esp_display_present_layout_t layout = {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                           ESP_DISPLAY_PRESENT_ORDER_NATIVE
                                          };
    esp_display_present_copy_request_t request = {
        .src = {src, 32, layout}, .dst = {dst, 64, layout},
        .src_area = {0, 0, 4, 4},
    };
    memset(dst, 0xa5, 64);
    unsigned before = submissions;
    dma_busy_count = 1;
    assert(esp_display_present_executor_execute(executor, &request) == ESP_OK);
    assert(dma_busy_count == 0 && submissions == before + 2);
    assert(!memcmp(src, dst, 32));

    esp_display_present_pixel_engine_t *engine;
    esp_display_present_pixel_config_t config = {
        .max_jobs = 1, .asynchronous = true,
        .execute = esp_display_present_executor_execute, .execute_user = executor,
    };
    assert(esp_display_present_pixel_create(&config, &engine) == ESP_OK);
    memset(dst, 0xa5, 64);
    pthread_mutex_lock(&dma_lock);
    dma_deferred = true;
    dma_entered = false;
    pthread_mutex_unlock(&dma_lock);
    esp_display_present_copy_ticket_t ticket;
    assert(esp_display_present_pixel_submit(engine, &request, &ticket) == ESP_OK);
    pthread_mutex_lock(&dma_lock);
    while (!dma_entered) {
        pthread_cond_wait(&dma_changed, &dma_lock);
    }
    async_memcpy_dma2d_trans_desc_t pending = dma_pending;
    async_memcpy_dma2d_isr_cb_t callback = dma_pending_cb;
    async_memcpy_dma2d_handle_t handle = dma_pending_handle;
    void *user = dma_pending_user;
    dma_deferred = false;
    pthread_mutex_unlock(&dma_lock);
    assert(esp_display_present_pixel_stop(engine, 0) == ESP_ERR_TIMEOUT);
    assert(dst[0] == 0xa5);
    paint(pending.src_buffer, pending.src_stride * 2, pending.dst_buffer, pending.dst_stride * 2,
          pending.src_x, pending.src_y, pending.copy_width, pending.copy_height,
          pending.dst_x, pending.dst_y, 0, 2, false);
    callback(handle, NULL, user);
    assert(esp_display_present_pixel_stop(engine, UINT32_MAX) == ESP_OK);
    assert(!memcmp(src, dst, 32));
    assert(esp_display_present_pixel_release(engine, ticket) == ESP_OK);
    assert(esp_display_present_pixel_destroy(engine) == ESP_OK);
    free(src);
    free(dst);
}
#endif
static void verify(esp_display_present_executor_t *e, esp_display_present_executor_t *cpu, bool is_dma)
{
    uint8_t *src = aligned_alloc(64, 192), *dst = aligned_alloc(64, 192), expected[192];
    assert(src && dst);
    for (size_t i = 0; i < 192; i++) {
        src[i] = (uint8_t)i;
    }
    esp_display_present_executor_caps_t caps;
    assert(esp_display_present_executor_get_caps(e, &caps) == ESP_OK);
    assert(caps.format_mask == 3 && caps.rotation_mask == (is_dma ? 1 : 15));
    for (unsigned bpp = 2; bpp <= 3; bpp++) {
        for (unsigned rot = 0; rot < 4; rot++) {
            esp_display_present_layout_t layout = {5, 5, 6 * bpp, bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888, ESP_DISPLAY_PRESENT_ORDER_NATIVE};
            esp_display_present_copy_request_t r = {.src = {src, 192, layout}, .dst = {dst, 192, layout},
                                                    .src_area = {1, 1, 3, 2}, .dst_x = 1, .dst_y = 1, .rotation = rot
                                                   };
            if (bpp == 2 && !is_dma) {
                r.dst.layout.byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
            }
            memset(dst, 0xa5, 192);
            memset(expected, 0xa5, 192);
            esp_display_present_executor_reason_t why;
#if PRESENT_HOST_PPA || PRESENT_HOST_DMA2D
            unsigned before = submissions, before_sync = syncs;
#endif
            esp_err_t error = esp_display_present_executor_probe(e, &r, &why);
            assert(error == (is_dma && rot ? ESP_ERR_NOT_SUPPORTED : ESP_OK));
            assert(why == (is_dma && rot ? ESP_DISPLAY_PRESENT_EXEC_TRANSFORM : ESP_DISPLAY_PRESENT_EXEC_SUPPORTED));
            assert(!memcmp(dst, expected, 192));
#if PRESENT_HOST_PPA || PRESENT_HOST_DMA2D
            assert(submissions == before && syncs == before_sync);
#endif
            esp_display_present_executor_dispatch_t dispatch = {e, NULL};
            esp_display_present_executor_selection_t selected;
            assert(esp_display_present_executor_select(&dispatch, &r, &selected) == error);
            if (error) {
                assert(!selected.executor);
            }
            dispatch.fallback = cpu;
            assert(esp_display_present_executor_run(&dispatch, &r, &selected) == ESP_OK);
            assert(selected.executor == (is_dma && rot ? cpu : e));
            assert(selected.used_fallback == (is_dma && rot != 0));
            paint(src, layout.stride_bytes, expected, layout.stride_bytes, 1, 1, 3, 2, 1, 1, rot, bpp, bpp == 2 && !is_dma);
            assert(!memcmp(dst, expected, 192));
            r.src_area.width = 100;
            assert(esp_display_present_executor_run(&dispatch, &r, &selected) == ESP_ERR_INVALID_ARG);
            assert(!selected.executor && selected.primary_reason == ESP_DISPLAY_PRESENT_EXEC_INVALID_REQUEST);
        }
    }
#if PRESENT_HOST_PPA || PRESENT_HOST_DMA2D
    if (e != cpu) {
        esp_display_present_layout_t l = {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE};
        esp_display_present_copy_request_t r = {.src = {src, 32, l}, .dst = {dst, 64, l}, .src_area = {0, 0, 4, 4}};
        esp_display_present_executor_dispatch_t d = {e, cpu};
        esp_display_present_executor_selection_t selected;
        unsigned before = submissions, before_sync = syncs;
        assert(esp_display_present_executor_select(&d, &r, &selected) == ESP_OK && selected.executor == e);
        assert(submissions == before && syncs == before_sync);
        memset(dst, 0xa5, 192);
        driver_error = ESP_FAIL;
        assert(esp_display_present_executor_run(&d, &r, &selected) == ESP_FAIL);
        assert(selected.executor == e && !selected.used_fallback && dst[0] == 0xa5);
        driver_error = ESP_OK;
        strict_alignment = 16;
        assert(esp_display_present_executor_select(&d, &r, &selected) == ESP_OK && selected.used_fallback);
        assert(selected.primary_reason == (is_dma ? ESP_DISPLAY_PRESENT_EXEC_ALIGNMENT : ESP_DISPLAY_PRESENT_EXEC_MEMORY));
        if (!is_dma) {
            /* IDF PPA SRM rejects strict MSPI memory even when every row and
             * the complete window are individually aligned: macroblocks
             * inside SRM cannot preserve that alignment. */
            esp_display_present_layout_t aligned = {8, 2, 16, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE};
            esp_display_present_copy_request_t aligned_request = {
                .src = {src, 32, aligned}, .dst = {dst, 64, aligned}, .src_area = {0, 0, 8, 2},
            };
            unsigned prior = submissions;
            assert(esp_display_present_executor_run(&d, &aligned_request, &selected) == ESP_OK);
            assert(selected.used_fallback && selected.primary_reason == ESP_DISPLAY_PRESENT_EXEC_MEMORY);
            assert(submissions == prior && memcmp(src, dst, 32) == 0);
        }
        strict_alignment = 1;
        unregister_error = ESP_FAIL;
        assert(esp_display_present_executor_destroy(e) == ESP_FAIL);
        assert(esp_display_present_executor_run(&d, &r, &selected) == ESP_OK && selected.executor == e);
        unregister_error = ESP_OK;
    }
#endif
    free(src);
    free(dst);
}
/* Arbitrary injected executor: chooser must not depend on built-in kind enums. */
typedef struct {
    unsigned calls;
    esp_err_t probe_error, execute_error;
} injected_t;
static esp_err_t injected_caps(void *context, esp_display_present_executor_caps_t *c)
{
    (void)context;
    *c = (esp_display_present_executor_caps_t) {
        .name = "injected"
    };
    return ESP_OK;
}
static esp_err_t injected_probe(void *context, const esp_display_present_copy_request_t *r, esp_display_present_executor_reason_t *why)
{
    (void)r;
    injected_t *i = context;
    *why = i->probe_error ? ESP_DISPLAY_PRESENT_EXEC_LAYOUT : ESP_DISPLAY_PRESENT_EXEC_SUPPORTED;
    return i->probe_error;
}
static esp_err_t injected_execute(void *context, const esp_display_present_copy_request_t *r)
{
    (void)r;
    injected_t *i = context;
    i->calls++;
    return i->execute_error;
}
static esp_err_t injected_destroy(void *context)
{
    (void)context;
    return ESP_OK;
}
static void injection(esp_display_present_executor_t *cpu)
{
    const esp_display_present_executor_ops_t ops = {injected_caps, injected_probe, injected_execute, injected_destroy};
    injected_t state = {0};
    esp_display_present_executor_t e = {&ops, &state};
    uint16_t src = 0x1234, dst = 0;
    esp_display_present_layout_t l = {1, 1, 2, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE};
    esp_display_present_copy_request_t r = {.src = {&src, 2, l}, .dst = {&dst, 2, l}, .src_area = {0, 0, 1, 1}};
    esp_display_present_executor_dispatch_t d = {&e, cpu};
    esp_display_present_executor_selection_t selected;
    state.probe_error = ESP_ERR_INVALID_STATE;
    assert(esp_display_present_executor_run(&d, &r, &selected) == ESP_ERR_INVALID_STATE && !state.calls && dst == 0);
    state.probe_error = ESP_OK;
    state.execute_error = ESP_ERR_NOT_SUPPORTED;
    assert(esp_display_present_executor_run(&d, &r, &selected) == ESP_ERR_NOT_SUPPORTED && state.calls == 1 && dst == 0);
    state.probe_error = ESP_ERR_NOT_SUPPORTED;
    assert(esp_display_present_executor_run(&d, &r, &selected) == ESP_OK && selected.used_fallback && dst == src);
    assert(esp_display_present_executor_destroy(&e) == ESP_OK);
}
int main(void)
{
    esp_display_present_executor_t *cpu, *e;
    assert(esp_display_present_executor_cpu_create(&cpu) == ESP_OK);
    verify(cpu, cpu, false);
    injection(cpu);
    assert(esp_display_present_executor_ppa_create(NULL) == ESP_ERR_INVALID_ARG);
    e = NULL;
#if PRESENT_HOST_PPA
    assert(esp_display_present_executor_ppa_create(&e) == ESP_OK);
    verify(e, cpu, false);
    verify_cache_bands(e, false);
    verify_cache_failures(e);
    verify_fifo_boundary(e, cpu);
    verify_rotated_fifo_tail(e, cpu);
    assert(esp_display_present_executor_destroy(e) == ESP_OK);
    verify_instance_isolation(false);
#else
    assert(esp_display_present_executor_ppa_create(&e) == ESP_ERR_NOT_SUPPORTED && !e);
#endif
    e = NULL;
#if PRESENT_HOST_DMA2D
    assert(esp_display_present_executor_dma2d_create(&e) == ESP_OK);
    verify(e, cpu, true);
    verify_cache_bands(e, true);
    verify_cache_failures(e);
    verify_dma_retry_and_drain(e);
    assert(esp_display_present_executor_destroy(e) == ESP_OK);
    verify_instance_isolation(true);
#else
    assert(esp_display_present_executor_dma2d_create(&e) == ESP_ERR_NOT_SUPPORTED && !e);
#endif
    assert(esp_display_present_executor_destroy(cpu) == ESP_OK);
    puts("executor matrix: capability, pure probe, injection, fallback, pixels and teardown passed");
    return 0;
}
