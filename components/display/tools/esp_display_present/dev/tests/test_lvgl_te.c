/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "present_lvgl.h"
#include "sync_panel.h"
#if LVGL_VERSION_MINOR == 0
#include "src/display/lv_display_private.h"
#define lv_display_get_buf_active(display) ((display)->buf_act)
#endif
#include <stdio.h>
#define OK(x) assert((x) == ESP_OK)
#define WIDTH 7
#define HEIGHT 9

static esp_display_present_engine_t *create(sync_panel_t *s, esp_display_present_engine_backend_t kind, uint32_t timeout)
{
    esp_display_present_engine_config_t config = {.backend = {
            .width = s->panel.width, .height = s->panel.height, .tile_rows = 2,
            .buffer_count = 2, .max_frames = 2, .format = s->panel.bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888,
            .panel = sync_panel_interface(s), .te_window_us = 1000000, .te_budget_us = 1000
        },
        .backend_kind = kind, .timeout_ms = timeout
    };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&config, &p));
    return p;
}

static void background(lv_display_t *display, uint32_t color)
{
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

static lv_obj_t *box(lv_display_t *display, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(lv_display_get_screen_active(display));
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

static void check_image(sync_panel_t *s, unsigned phase)
{
    /* Independent image oracle, including separated dirty rectangles and old
     * rectangle restoration when a box moves. */
    pthread_mutex_lock(&s->mutex);
    for (unsigned y = 0; y < HEIGHT; y++) {
        for (unsigned x = 0; x < WIDTH; x++) {
            uint32_t rgb = 0x123456;
            unsigned left = phase == 2 ? 2 : 1;
            if (phase && x >= left && x < left + 2 && y >= 1 && y < 3) {
                rgb = 0xff0000;
            }
            if (phase && x >= 4 && x < 6 && y >= 6 && y < 8) {
                rgb = 0x0000ff;
            }
            size_t offset = (y * WIDTH + x) * s->panel.bpp;
            if (s->panel.bpp == 2) {
                uint16_t expected = (uint16_t)(((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 31));
                uint16_t actual;
                memcpy(&actual, s->panel.pixels + offset, 2);
                assert(actual == expected);
            } else {
                assert(s->panel.pixels[offset] == (rgb >> 16));
                assert(s->panel.pixels[offset + 1] == ((rgb >> 8) & 255));
                assert(s->panel.pixels[offset + 2] == (rgb & 255));
            }
        }
    }
    pthread_mutex_unlock(&s->mutex);
}

static void finish(sync_panel_t *s, esp_display_present_engine_t *p, esp_display_present_engine_backend_t kind)
{
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE) {
        pthread_t worker = sync_start_worker(s, p);
        OK(esp_display_present_engine_drain(p, 1000));
        assert(pthread_join(worker, NULL) == 0);
    } else {
        while (s->panel.count) {
            sync_complete(s);
        }
        OK(esp_display_present_engine_drain(p, 0));
    }
}

static void images(esp_display_present_engine_backend_t kind, size_t bpp, bool pooled)
{
    sync_panel_t s;
    sync_panel_init(&s);
    s.panel.width = WIDTH;
    s.panel.height = HEIGHT;
    s.panel.bpp = bpp;
    s.panel.service_previous = true;
    esp_display_present_engine_t *p = create(&s, kind, 1000);
    esp_lv_present_native_t *binding;
    if (pooled) {
#if LVGL_VERSION_MINOR < 5
        assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_ERR_NOT_SUPPORTED);
        OK(esp_lv_present_native_create(p, 2, &binding));
#else
        OK(esp_lv_present_native_create_pooled(p, 2, &binding));
#endif
    } else {
        OK(esp_lv_present_native_create(p, 4, &binding));    /* Split LVGL bands across 2-row leases. */
    }
    lv_display_t *display = esp_lv_present_native_get_display(binding);
    background(display, 0x123456);
    lv_refr_now(display);
    OK(esp_lv_present_native_get_error(binding));
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.committed_frames == 1);
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE) {
        assert(s.panel.submitted == 0);
    } else {
        assert(s.panel.submitted >= 5);
    }
    lv_draw_buf_t *draw = lv_display_get_buf_active(display);
    memset(draw->data, 0, draw->data_size);
    finish(&s, p, kind);
    check_image(&s, 0);
    lv_obj_t *red = box(display, 1, 1, 2, 2, 0xff0000);
    (void)box(display, 4, 6, 2, 2, 0x0000ff);
    lv_refr_now(display);
    OK(esp_lv_present_native_get_error(binding));
    finish(&s, p, kind);
    check_image(&s, 1);
    lv_obj_set_pos(red, 2, 1);
    lv_refr_now(display);
    OK(esp_lv_present_native_get_error(binding));
    memset(draw->data, 0xcc, draw->data_size);
    /* Destroy LVGL before the last committed transmission completes. */
    OK(esp_lv_present_native_destroy(binding));
    finish(&s, p, kind);
    check_image(&s, 2);
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.completed_frames == 3 && status.first_error == ESP_OK);
    OK(esp_display_present_engine_stop(p, 0));
    OK(esp_display_present_engine_destroy(p));
    sync_panel_deinit(&s);
}

static void backpressure(uint32_t timeout, bool deliver, bool pooled)
{
    sync_panel_t s;
    sync_panel_init(&s);
    esp_display_present_engine_t *p = create(&s, ESP_DISPLAY_PRESENT_ENGINE_TE, timeout);
    esp_lv_present_native_t *binding;
    if (pooled) {
#if LVGL_VERSION_MINOR < 5
        assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_ERR_NOT_SUPPORTED);
        OK(esp_lv_present_native_create(p, 2, &binding));
#else
        OK(esp_lv_present_native_create_pooled(p, 2, &binding));
#endif
    } else {
        OK(esp_lv_present_native_create(p, 4, &binding));
    }
    lv_display_t *display = esp_lv_present_native_get_display(binding);
    background(display, 0xffffff);
    lv_refr_now(display);
    pthread_t worker;
    if (deliver) {
        worker = sync_start_worker(&s, p);
    }
    background(display, 0xff0000);
    lv_refr_now(display); /* Waits for the first frame or explicitly pauses. */
    if (deliver) {
        assert(pthread_join(worker, NULL) == 0);
        OK(esp_lv_present_native_get_error(binding));
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(p, &status));
        assert(status.completed_frames == 1 && status.committed_frames == 2);
        finish(&s, p, ESP_DISPLAY_PRESENT_ENGINE_TE);
    } else {
        assert(esp_lv_present_native_get_error(binding) == ESP_ERR_TIMEOUT);
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(p, &status));
        assert(!status.frame_active && status.pending_frames == 1 && status.first_error == ESP_OK);
        lv_tick_inc(100);
        lv_timer_handler();
        OK(esp_display_present_engine_get_status(p, &status));
        assert(status.committed_frames == 1); /* Paused, no silent retries. */
        finish(&s, p, ESP_DISPLAY_PRESENT_ENGINE_TE);
        for (unsigned i = 0; i < 32; i++) {
            assert(s.panel.pixels[i] == 255);
        }
    }
    OK(esp_lv_present_native_destroy(binding));
    OK(esp_display_present_engine_stop(p, 0));
    OK(esp_display_present_engine_destroy(p));
    sync_panel_deinit(&s);
}

static void multiple_instances(void)
{
    sync_panel_t a, b;
    sync_panel_init(&a);
    sync_panel_init(&b);
    a.panel.immediate = b.panel.immediate = true;
    esp_display_present_engine_t *pa = create(&a, ESP_DISPLAY_PRESENT_ENGINE_TE, 0);
    esp_display_present_engine_t *pb = create(&b, ESP_DISPLAY_PRESENT_ENGINE_REGION, 0);
    esp_lv_present_native_t *ba, *bb;
    OK(esp_lv_present_native_create(pa, 2, &ba));
    OK(esp_lv_present_native_create(pb, 2, &bb));
    background(esp_lv_present_native_get_display(ba), 0xffffff);
    background(esp_lv_present_native_get_display(bb), 0x000000);
    lv_refr_now(esp_lv_present_native_get_display(ba));
    lv_refr_now(esp_lv_present_native_get_display(bb));
    esp_display_present_engine_notify_te(pb, test_now_us()); /* GRAM ignores notification. */
    assert(!esp_display_present_engine_notify_te_isr(pb, test_now_us()));
    OK(esp_display_present_engine_service(pb));
    assert(a.panel.submitted == 0);
    esp_display_present_engine_notify_te(pa, test_now_us());
    OK(esp_display_present_engine_service(pa));
    for (unsigned i = 0; i < 32; i++) {
        assert(a.panel.pixels[i] == 255 && b.panel.pixels[i] == 0);
    }
    OK(esp_lv_present_native_destroy(ba));
    OK(esp_lv_present_native_destroy(bb));
    OK(esp_display_present_engine_stop(pa, 0));
    OK(esp_display_present_engine_stop(pb, 0));
    OK(esp_display_present_engine_destroy(pa));
    OK(esp_display_present_engine_destroy(pb));
    sync_panel_deinit(&a);
    sync_panel_deinit(&b);
}
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned count;
} copy_notifications_t;
static bool copy_notification(void *user, bool isr)
{
    assert(!isr);
    copy_notifications_t *n = user;
    pthread_mutex_lock(&n->lock);
    n->count++;
    pthread_cond_broadcast(&n->changed);
    pthread_mutex_unlock(&n->lock);
    return false;
}

static void background_backend_worker(bool async, esp_display_present_engine_backend_t kind, bool pooled)
{
    copy_notifications_t n = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    sync_panel_t panel;
    sync_panel_init(&panel);
    esp_display_present_engine_config_t config = {.backend = {
            .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = sync_panel_interface(&panel),
            .async_copy = async, .on_progress = copy_notification, .progress_user = &n,
            .te_window_us = 1000000, .te_budget_us = 1000
        },
        .backend_kind = kind, .timeout_ms = 1000, .background_service = true
    };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&config, &p));
    esp_lv_present_native_t *binding;
    if (pooled) {
#if LVGL_VERSION_MINOR < 5
        assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_ERR_NOT_SUPPORTED);
        OK(esp_lv_present_native_create(p, 2, &binding));
#else
        OK(esp_lv_present_native_create_pooled(p, 2, &binding));
#endif
    } else {
        OK(esp_lv_present_native_create(p, 4, &binding));
    }
    lv_display_t *display = esp_lv_present_native_get_display(binding);
    background(display, 0xffffff);
    lv_refr_now(display);
    if (async && kind == ESP_DISPLAY_PRESENT_ENGINE_TE) {
        /* TE must finish both tile copies before the synthetic edge. GRAM's
         * identity path borrows its draw tiles directly even with async_copy;
         * there are no copy completions to wait for on that path. */
        struct timespec copy_deadline;
        assert(clock_gettime(CLOCK_REALTIME, &copy_deadline) == 0);
        copy_deadline.tv_sec += 2;
        pthread_mutex_lock(&n.lock);
        while (n.count < 2) {
            assert(pthread_cond_timedwait(&n.changed, &n.lock, &copy_deadline) == 0);
        }
        pthread_mutex_unlock(&n.lock);
        /* Finish COPY_DRAIN before injecting the next physical TE edge. */
        OK(esp_display_present_engine_service(p));
    }
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_TE) {
        esp_display_present_engine_notify_te_isr(p, test_now_us());
    }
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline); /* sync_panel's cond uses default clock. */
    deadline.tv_sec += 2;
    pthread_mutex_lock(&panel.mutex);
    size_t expected = kind == ESP_DISPLAY_PRESENT_ENGINE_TE ? 1 : 2;
    while (panel.panel.submitted < expected) {
        assert(pthread_cond_timedwait(&panel.changed, &panel.mutex, &deadline) == 0);
    }
    assert(panel.panel.count == expected);
    pthread_mutex_unlock(&panel.mutex);
    /* Sender progresses without an owner pump (apart from the TE copy drain
     * above). The LVGL fallback is independent of every queued DMA source. */
    lv_draw_buf_t *draw = lv_display_get_buf_active(display);
    pthread_mutex_lock(&panel.mutex);
    for (size_t i = 0; i < panel.panel.count; ++i) {
        const esp_display_present_surface_t *source = &panel.panel.pending[i].surface;
        uintptr_t source_begin = (uintptr_t)source->pixels;
        uintptr_t draw_begin = (uintptr_t)draw->data;
        assert(draw_begin + draw->data_size <= source_begin ||
               source_begin + source->capacity_bytes <= draw_begin);
    }
    pthread_mutex_unlock(&panel.mutex);
    if (kind == ESP_DISPLAY_PRESENT_ENGINE_REGION) {
        /* Identity output must retain the original presenter tiles until DMA
         * completes, rather than allocate converted copies just to notify us. */
        void *pool[2];
        size_t count, bytes;
        OK(esp_display_present_engine_get_draw_buffers(p, pool, 2, &count, &bytes));
        assert(count == 2);
        pthread_mutex_lock(&panel.mutex);
        assert(panel.panel.pending[0].surface.pixels != panel.panel.pending[1].surface.pixels);
        for (size_t i = 0; i < panel.panel.count; ++i) {
            const esp_display_present_surface_t *source = &panel.panel.pending[i].surface;
            assert(source->pixels == pool[0] || source->pixels == pool[1]);
            assert(source->capacity_bytes <= bytes);
        }
        pthread_mutex_unlock(&panel.mutex);
    }
    memset(draw->data, 0, draw->data_size);
    for (size_t i = 0; i < expected; ++i) {
        sync_complete(&panel);
    }
    OK(esp_display_present_engine_drain(p, 1000));
    OK(esp_lv_present_native_get_error(binding));
    pthread_mutex_lock(&panel.mutex);
    for (unsigned i = 0; i < 32; i++) {
        assert(panel.panel.pixels[i] == 255);
    }
    pthread_mutex_unlock(&panel.mutex);
    OK(esp_lv_present_native_destroy(binding));
    OK(esp_display_present_engine_stop(p, 1000));
    OK(esp_display_present_engine_destroy(p));
    sync_panel_deinit(&panel);
    pthread_cond_destroy(&n.changed);
    pthread_mutex_destroy(&n.lock);
}
int main(void)
{
    lv_init();
    images(ESP_DISPLAY_PRESENT_ENGINE_REGION, 2, false);
    images(ESP_DISPLAY_PRESENT_ENGINE_TE, 2, false);
    images(ESP_DISPLAY_PRESENT_ENGINE_REGION, 3, false);
    images(ESP_DISPLAY_PRESENT_ENGINE_TE, 3, false);
    images(ESP_DISPLAY_PRESENT_ENGINE_REGION, 2, true);
    images(ESP_DISPLAY_PRESENT_ENGINE_TE, 2, true);
    backpressure(0, false, false);
    backpressure(0, false, true);
    backpressure(5, false, false);
    backpressure(1000, true, false);
    backpressure(1000, true, true);
    multiple_instances();
    background_backend_worker(false, ESP_DISPLAY_PRESENT_ENGINE_TE, false);
    background_backend_worker(true, ESP_DISPLAY_PRESENT_ENGINE_TE, false);
    background_backend_worker(true, ESP_DISPLAY_PRESENT_ENGINE_REGION, false);
    background_backend_worker(true, ESP_DISPLAY_PRESENT_ENGINE_TE, true);
    background_backend_worker(true, ESP_DISPLAY_PRESENT_ENGINE_REGION, true);
    lv_deinit();
    puts("LVGL GRAM/TE: reference images, padding, source lifetime, wait/pause and isolation passed");
    return 0;
}
