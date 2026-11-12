/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "present_lvgl.h"
#if LVGL_VERSION_MINOR == 0
#include "src/display/lv_display_private.h"
#define lv_display_get_buf_active(display) ((display)->buf_act)
#endif
#include "scan_panel.h"
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#define OK(x) assert((x) == ESP_OK)
#define W 7
#define H 9
static void check(scan_panel_t *panel, size_t bpp, bool changed, unsigned rotation)
{
    for (unsigned y = 0; y < H; y++) {
        for (unsigned x = 0; x < W; x++) {
            uint32_t rgb = 0x123456;
            if (changed && x >= 1 && x < 3 && y >= 1 && y < 3) {
                rgb = 0xff0000;
            }
            if (changed && x >= 4 && x < 6 && y >= 6 && y < 8) {
                rgb = 0x0000ff;
            }
            unsigned px = x, py = y, pw = rotation & 1 ? H : W;
            switch (rotation) {
            case 1: px = H - 1 - y; py = x; break;
            case 2: px = W - 1 - x; py = H - 1 - y; break;
            case 3: px = y; py = W - 1 - x; break;
            }
            const uint8_t *pixel = (const uint8_t *)panel->current.pixels + (py * pw + px) * bpp;
            if (bpp == 2) {
                uint16_t actual, expected = (uint16_t)(((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x7e0) | ((rgb >> 3) & 31));
                memcpy(&actual, pixel, 2);
                assert(actual == expected);
            } else {
                assert(pixel[0] == (rgb >> 16) && pixel[1] == ((rgb >> 8) & 255) && pixel[2] == (rgb & 255));
            }
        }
    }
}

static void box(lv_obj_t *screen, int x, int y, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(screen);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, 2, 2);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
}

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned count;
} notifications_t;
static bool progress(void *user, bool isr)
{
    assert(!isr);
    notifications_t *n = user;
    pthread_mutex_lock(&n->lock);
    n->count++;
    pthread_cond_broadcast(&n->changed);
    pthread_mutex_unlock(&n->lock);
    return false;
}

static void wait_scan_submit(esp_display_present_engine_t *p, scan_panel_t *panel, notifications_t *n)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 2;
    for (;;) {
        pthread_mutex_lock(&n->lock);
        unsigned seen = n->count;
        pthread_mutex_unlock(&n->lock);
        OK(esp_display_present_engine_service(p));
        if (panel->cookie) {
            return;
        }
        pthread_mutex_lock(&n->lock);
        while (seen == n->count) {
            assert(pthread_cond_timedwait(&n->changed, &n->lock, &deadline) == 0);
        }
        pthread_mutex_unlock(&n->lock);
    }
}

typedef struct {
    void *buffers[16];
    size_t count;
    unsigned seen, deferred_bands;
    void *submitted;
} pool_observer_t;
static void assert_borrowed(lv_event_t *event)
{
    pool_observer_t *observer = lv_event_get_user_data(event);
    lv_display_t *display = lv_event_get_target(event);
    void *active = lv_display_get_buf_active(display)->data;
    if (lv_event_get_code(event) == LV_EVENT_FLUSH_START) {
        observer->submitted = active;
        return;
    }
    if (lv_event_get_code(event) == LV_EVENT_FLUSH_FINISH) {
        if (!lv_display_flush_is_last(display)) {
            /* The next tile is not acquired or rebound inside flush. */
            assert(active == observer->submitted);
            observer->deferred_bands++;
        }
        return;
    }
    bool found = false;
    for (size_t i = 0; i < observer->count; ++i) {
        found |= observer->buffers[i] == active;
    }
    assert(found);
    observer->seen++;
}

typedef struct {
    esp_display_present_engine_t *presenter;
    pool_observer_t *observer;
} handoff_t;
static void *other_producer(void *arg)
{
    handoff_t *handoff = arg;
    esp_display_present_engine_t *p = handoff->presenter;
    OK(esp_display_present_engine_rebind_producer(p, 1000));
    void *pool[16];
    size_t count, bytes;
    OK(esp_display_present_engine_get_draw_buffers(p, pool, 16, &count, &bytes));
    assert(count == handoff->observer->count && bytes);
    for (size_t i = 0; i < count; ++i) {
        assert(pool[i] == handoff->observer->buffers[i]);
    }
    esp_display_present_frame_request_t request = {0};
    OK(esp_display_present_engine_begin_next_frame(p, &request));
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(p, &lease));
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
        found |= pool[i] == lease.surface.pixels;
    }
    assert(found);
    memset(lease.surface.pixels, 0, 2);
    esp_display_present_engine_area_t area = {0, 0, 0, 0};
    OK(esp_display_present_engine_submit_buffer(p, &lease, &area, 2));
    OK(esp_display_present_engine_commit_frame(p, NULL));
    OK(esp_display_present_engine_service(p));
    return NULL;
}

static void exercise(esp_display_present_present_mode_t mode, size_t bpp, bool async, bool pooled, unsigned rotation)
{
    notifications_t n = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    scan_panel_t panel = {0};
    esp_display_present_engine_config_t cfg = {.backend = {
            .width = W, .height = H, .tile_rows = 2, .buffer_count = 3, .max_frames = 4, .rotation = rotation,
            .format = bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888, .active_writes = true,
            .panel = scan_interface(&panel), .async_copy = async, .on_progress = progress, .progress_user = &n
        },
        .timeout_ms = 1000, .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = mode
    };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&cfg, &p));
    esp_lv_present_native_t *binding;
    pool_observer_t observer = {0};
#if LVGL_VERSION_MINOR < 5
    if (pooled) {
        assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_ERR_NOT_SUPPORTED);
        pooled = false; /* Exercise the real copy fallback with identical pixels. */
    }
#endif
    if (pooled) {
        size_t bytes;
        OK(esp_display_present_engine_get_draw_buffers(p, observer.buffers, 16, &observer.count, &bytes));
        assert(observer.count && bytes);
    }
    if (pooled) {
        OK(esp_lv_present_native_create_pooled(p, 2, &binding));
    } else {
        OK(esp_lv_present_native_create(p, 4, &binding));
    }
    lv_display_t *display = esp_lv_present_native_get_display(binding);
    if (pooled) {
        lv_display_add_event_cb(display, assert_borrowed, LV_EVENT_RENDER_START, &observer);
        lv_display_add_event_cb(display, assert_borrowed, LV_EVENT_FLUSH_START, &observer);
        lv_display_add_event_cb(display, assert_borrowed, LV_EVENT_FLUSH_FINISH, &observer);
    }
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x123456), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(display);
    OK(esp_lv_present_native_get_error(binding));
    if (pooled) {
        assert(observer.seen && observer.deferred_bands);
    }
    wait_scan_submit(p, &panel, &n);
    assert(panel.cookie && !panel.current.pixels);
    scan_confirm(&panel);
    OK(esp_display_present_engine_drain(p, 0));
    check(&panel, bpp, false, rotation);
    void *original = panel.current.pixels;
    uint8_t snapshot[W * H * 3];
    memcpy(snapshot, original, W * H * bpp);
    box(screen, 1, 1, 0xff0000);
    box(screen, 4, 6, 0x0000ff);
    lv_refr_now(display);
    OK(esp_lv_present_native_get_error(binding));
    wait_scan_submit(p, &panel, &n);
    assert(panel.cookie);
    if (mode != ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE) {
        assert(panel.pending.pixels != original);
        assert(memcmp(original, snapshot, W * H * bpp) == 0);
    }
    /* Completed receipts do not retire original until the switch confirmation. */
    assert(esp_display_present_engine_drain(p, 0) == ESP_ERR_TIMEOUT);
    OK(esp_lv_present_native_destroy(binding)); /* Persistent LVGL DIRECT buffer no longer needed. */
    scan_confirm(&panel);
    OK(esp_display_present_engine_drain(p, 0));
    check(&panel, bpp, true, rotation);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.completed_frames == 2 && status.first_error == ESP_OK);
    if (pooled) {
        /* Model a different task, such as GSP, taking ownership of the same
         * presenter pool between two LVGL bindings. */
        handoff_t handoff = {p, &observer};
        pthread_t worker;
        assert(pthread_create(&worker, NULL, other_producer, &handoff) == 0);
        assert(pthread_join(worker, NULL) == 0);
        OK(esp_display_present_engine_rebind_producer(p, 1000));
        wait_scan_submit(p, &panel, &n);
        scan_confirm(&panel);
        OK(esp_display_present_engine_drain(p, 1000));
        OK(esp_lv_present_native_create_pooled(p, 2, &binding));
        OK(esp_lv_present_native_destroy(binding));
    }
    panel.stop_timeout = true;
    assert(esp_display_present_engine_stop(p, 0) == ESP_ERR_TIMEOUT);
    panel.stop_timeout = false;
    OK(esp_display_present_engine_stop(p, 1000));
    OK(esp_display_present_engine_destroy(p));
    pthread_cond_destroy(&n.changed);
    pthread_mutex_destroy(&n.lock);
}
int main(void)
{
    const esp_display_present_present_mode_t modes[] = {ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL,
                                                        ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO
                                                       };
    lv_init();
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); i++) {
        exercise(modes[i], 2, false, false, 0);
        exercise(modes[i], 3, false, false, 0);
    }
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, true, false, 0);
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, 3, true, false, 0);
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, true, false, 0);
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 3, true, false, 0);
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, false, true, 0);
    exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, 2, true, true, 0);
    for (unsigned rotation = 1; rotation < 4; ++rotation) {
        exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, false, false, rotation);
        exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 3, false, false, rotation);
        exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 2, true, false, rotation);
        exercise(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, 3, true, false, rotation);
    }
    lv_deinit();
    puts("LVGL FB modes: FULL/DIRECT/PARTIAL, RGB565/888, dirty offsets and scanout lifetime passed");
    return 0;
}
