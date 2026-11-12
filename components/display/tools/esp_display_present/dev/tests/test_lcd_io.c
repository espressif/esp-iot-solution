/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "platform/lcd/present_lcd_io.h"
#include "esp_cache.h"
#include "esp_memory_utils.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

struct fake_lcd {
    esp_lcd_panel_io_color_trans_done_cb_t done;
    void *user;
    bool early, detach_fail, attach_fail;
    esp_err_t draw_result;
    size_t draws, pending;
    int x0, y0, x1, y1;
    uint8_t bytes[64];
    bool staged;
    const void *pending_source;
    size_t band_bytes, history_bytes;
    uint8_t history[192];
};

static esp_err_t cache_result;
static bool internal = true, dma_capable = true;
static size_t cache_calls;
static pthread_mutex_t stage_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t stage_changed = PTHREAD_COND_INITIALIZER;

bool esp_ptr_internal(const void *p)
{
    (void)p;
    return internal;
}

bool esp_ptr_dma_capable(const void *p)
{
    (void)p;
    return dma_capable;
}

bool esp_ptr_dma_ext_capable(const void *p)
{
    (void)p;
    return !internal && dma_capable;
}

esp_err_t esp_cache_msync(void *p, size_t bytes, int flags)
{
    assert(p && bytes && flags == (ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED));
    ++cache_calls;
    return cache_result;
}

esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t io,
                                                    const esp_lcd_panel_io_callbacks_t *cb, void *user)
{
    if ((!cb->on_color_trans_done && io->detach_fail) || (cb->on_color_trans_done && io->attach_fail)) {
        return ESP_FAIL;
    }
    io->done = cb->on_color_trans_done;
    io->user = user;
    return ESP_OK;
}

esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p,
                                    int x0, int y0, int x1, int y1, const void *pixels)
{
    if (p->staged) {
        pthread_mutex_lock(&stage_lock);
        ++p->draws;
        p->band_bytes = (size_t)(x1 - x0) * (y1 - y0) * 2;
        p->pending_source = pixels;
        assert(p->band_bytes <= 64);
        if (p->early) {
            memcpy(p->history + p->history_bytes, pixels, p->band_bytes);
            p->history_bytes += p->band_bytes;
        } else {
            ++p->pending;
        }
        pthread_cond_broadcast(&stage_changed);
        pthread_mutex_unlock(&stage_lock);
        if (p->early) {
            (void)p->done(p, NULL, p->user);
        }
        return p->draw_result;
    }
    ++p->draws;
    assert(cache_calls >= p->draws);
    p->x0 = x0;
    p->y0 = y0;
    p->x1 = x1;
    p->y1 = y1;
    size_t count = (size_t)(x1 - x0) * (size_t)(y1 - y0) * 2;
    assert(count <= sizeof(p->bytes));
    memcpy(p->bytes, pixels, count);
    if (p->early) {
        (void)p->done(p, NULL, p->user);
    } else {
        ++p->pending;
    }
    return p->draw_result;
}

static void complete(struct fake_lcd *p)
{
    if (p->staged) {
        pthread_mutex_lock(&stage_lock);
        assert(p->pending && p->done);
        memcpy(p->history + p->history_bytes, p->pending_source, p->band_bytes);
        p->history_bytes += p->band_bytes;
        --p->pending;
        pthread_mutex_unlock(&stage_lock);
        (void)p->done(p, NULL, p->user);
        return;
    }
    assert(p->pending && p->done);
    --p->pending;
    (void)p->done(p, NULL, p->user);
}

typedef struct {
    uint64_t cookies[8];
    size_t count;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool block, entered, leave;
} sink_t;

static bool sink_event(void *context, const esp_display_present_backend_event_t *e, bool isr)
{
    sink_t *sink = context;
    assert(isr && e->kind == ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE && e->result == ESP_OK);
    sink->cookies[sink->count++] = e->transaction_cookie;
    if (sink->block) {
        pthread_mutex_lock(&sink->lock);
        sink->entered = true;
        pthread_cond_signal(&sink->changed);
        while (!sink->leave) {
            pthread_cond_wait(&sink->changed, &sink->lock);
        }
        pthread_mutex_unlock(&sink->lock);
    }
    return true;
}

static esp_display_present_lcd_io_t *make(struct fake_lcd *lcd, size_t depth, esp_display_present_panel_adapter_t *interface)
{
    esp_display_present_lcd_io_config_t config = { .panel = lcd, .io = lcd, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                   .byte_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE, .max_inflight = depth
                                                 };
    esp_display_present_lcd_io_t *adapter;
    assert(esp_display_present_lcd_io_create(&config, &adapter) == ESP_OK);
    assert(esp_display_present_lcd_io_get_interface(adapter, interface) == ESP_OK);
    return adapter;
}

static void fifo_and_validation(void)
{
    struct fake_lcd lcd = {0};
    esp_display_present_panel_adapter_t api;
    esp_display_present_lcd_io_t *adapter = make(&lcd, 2, &api);
    sink_t sink = {0};
    assert(api.attach(api.context, sink_event, &sink) == ESP_OK);
    _Alignas(64) uint8_t data[64] = {0x12, 0x34};
    esp_display_present_surface_t surface = { data, sizeof(data), {2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE} };
    esp_display_present_rect_t area = {3, 5, 2, 2};
    for (int variant = 0; variant < 5; ++variant) {
        esp_display_present_surface_t bad = surface;
        if (variant == 0) {
            bad.layout.stride_bytes = 8;
        }
        if (variant == 1) {
            bad.pixels = data + 1;
        }
        if (variant == 2) {
            bad.capacity_bytes = 7;
        }
        if (variant == 3) {
            bad.layout.byte_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
        }
        if (variant == 4) {
            internal = false;
        }
        assert(api.submit(api.context, &bad, &area, 10).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
        internal = true;
        assert(lcd.draws == 0);
    }
    cache_result = ESP_FAIL;
    assert(api.submit(api.context, &surface, &area, 10).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
    assert(lcd.draws == 0);
    cache_result = ESP_ERR_NOT_SUPPORTED; /* Valid non-cacheable internal RAM. */
    assert(api.submit(api.context, &surface, &area, 10).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    cache_result = ESP_OK;
    assert(api.submit(api.context, &surface, &area, 11).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    assert(lcd.x0 == 3 && lcd.y0 == 5 && lcd.x1 == 5 && lcd.y1 == 7);
    assert(api.submit(api.context, &surface, &area, 12).result == ESP_ERR_NO_MEM);
    assert(api.quiesce(api.context, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_ERR_INVALID_STATE);
    complete(&lcd);
    assert(api.quiesce(api.context, 0) == ESP_ERR_TIMEOUT);
    complete(&lcd);
    assert(sink.count == 2 && sink.cookies[0] == 10 && sink.cookies[1] == 11);
    lcd.detach_fail = true;
    assert(api.quiesce(api.context, 0) == ESP_FAIL && lcd.done);
    lcd.detach_fail = false;
    assert(api.quiesce(api.context, 0) == ESP_OK && !lcd.done);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
}

static void early_and_uncertain(void)
{
    for (int early = 0; early < 2; ++early) {
        struct fake_lcd lcd = { .early = early, .draw_result = ESP_FAIL };
        esp_display_present_panel_adapter_t api;
        esp_display_present_lcd_io_t *adapter = make(&lcd, 1, &api);
        sink_t sink = {0};
        assert(api.attach(api.context, sink_event, &sink) == ESP_OK);
        _Alignas(64) uint8_t data[64] = {0};
        esp_display_present_surface_t s = { data, sizeof(data), {2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE} };
        esp_display_present_rect_t a = {0, 0, 2, 2};
        assert(api.submit(api.context, &s, &a, 99).acceptance == ESP_DISPLAY_PRESENT_TX_UNCERTAIN);
        assert(api.submit(api.context, &s, &a, 100).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
        if (!early) {
            assert(api.quiesce(api.context, 1) == ESP_ERR_TIMEOUT);
            assert(esp_display_present_lcd_io_destroy(adapter) == ESP_ERR_INVALID_STATE);
            complete(&lcd); /* A late final chunk eventually establishes safety. */
        }
        assert(sink.count == 1 && sink.cookies[0] == 99);
        assert(api.quiesce(api.context, 0) == ESP_OK);
        assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
    }
    struct fake_lcd lcd = {.attach_fail = true};
    esp_display_present_panel_adapter_t api;
    esp_display_present_lcd_io_t *adapter = make(&lcd, 1, &api);
    sink_t sink = {0};
    assert(api.attach(api.context, sink_event, &sink) == ESP_FAIL && !lcd.done);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
}

static void *completion_thread(void *user)
{
    complete(user);
    return NULL;
}

static void callback_exit_barrier(void)
{
    struct fake_lcd lcd = {0};
    esp_display_present_panel_adapter_t api;
    esp_display_present_lcd_io_t *adapter = make(&lcd, 1, &api);
    sink_t sink = { .lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .block = true };
    assert(api.attach(api.context, sink_event, &sink) == ESP_OK);
    _Alignas(64) uint8_t data[64] = {0};
    esp_display_present_surface_t s = { data, sizeof(data), {2, 2, 4, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE} };
    esp_display_present_rect_t a = {0, 0, 2, 2};
    assert(api.submit(api.context, &s, &a, 50).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    pthread_t worker;
    assert(pthread_create(&worker, NULL, completion_thread, &lcd) == 0);
    pthread_mutex_lock(&sink.lock);
    while (!sink.entered) {
        pthread_cond_wait(&sink.changed, &sink.lock);
    }
    pthread_mutex_unlock(&sink.lock);
    assert(api.quiesce(api.context, 0) == ESP_ERR_TIMEOUT); /* Queue is empty, callback is not. */
    assert(lcd.done);
    pthread_mutex_lock(&sink.lock);
    sink.leave = true;
    pthread_cond_signal(&sink.changed);
    pthread_mutex_unlock(&sink.lock);
    assert(pthread_join(worker, NULL) == 0);
    assert(api.quiesce(api.context, 0) == ESP_OK);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
    pthread_cond_destroy(&sink.changed);
    pthread_mutex_destroy(&sink.lock);
}

static void gram_integration(void)
{
    struct fake_lcd lcd = {0};
    esp_display_present_panel_adapter_t api;
    esp_display_present_lcd_io_t *adapter = make(&lcd, 2, &api);
    esp_display_present_backend_config_t c = { .width = 4, .height = 4, .tile_rows = 2,
                                               .buffer_count = 2, .max_frames = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = api
                                             };
    present_backend_t *g;
    const present_backend_ops_t *ops = &present_region_backend;
    assert(ops->create(&c, &g) == ESP_OK);
    esp_display_present_frame_request_t request = {0};
    esp_display_present_frame_t f;
    assert(ops->begin(g, &request, &f) == ESP_OK);
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    for (size_t i = 0; i < 16; ++i) {
        ((uint8_t *)lease.surface.pixels)[i] = (uint8_t)i;
    }
    esp_display_present_region_submit_t sub = {.logical_area = {1, 2, 2, 2}, .stride_bytes = 8};
    assert(ops->submit_region_ex(g, f, lease.id, &sub) == ESP_OK);
    const uint8_t expected[] = {0, 1, 2, 3, 8, 9, 10, 11};
    assert(memcmp(lcd.bytes, expected, sizeof(expected)) == 0 && lcd.x0 == 1 && lcd.y0 == 2);
    assert(ops->end(g, f, NULL) == ESP_OK);
    complete(&lcd);
    /* Partial driver failure must propagate through backend's sticky fault. */
    assert(ops->begin(g, &request, &f) == ESP_OK);
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    memset(lease.surface.pixels, 0, 16);
    lcd.draw_result = ESP_FAIL;
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    assert(ops->end(g, f, NULL) == ESP_OK);
    assert(ops->stop(g, 0) == ESP_ERR_TIMEOUT);
    complete(&lcd);
    esp_display_present_backend_fault_t fault;
    assert(ops->get_fault(g, &fault) == ESP_OK && fault.result == ESP_FAIL && fault.frame.value == f.value);
    assert(ops->stop(g, 0) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
}

static void external_dma(void)
{
    struct fake_lcd lcd = {0};
    esp_display_present_lcd_io_config_t cfg = {.panel = &lcd, .io = &lcd, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .max_inflight = 1, .psram_dma_direct = true};
    esp_display_present_lcd_io_t *adapter;
    assert(esp_display_present_lcd_io_create(&cfg, &adapter) == ESP_OK);
    esp_display_present_panel_adapter_t api;
    assert(esp_display_present_lcd_io_get_interface(adapter, &api) == ESP_OK);
    sink_t sink = {0};
    assert(api.attach(api.context, sink_event, &sink) == ESP_OK);
    _Alignas(64) uint8_t pixels[64] = {0};
    esp_display_present_surface_t surface = {pixels, 64, {8, 4, 16, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}};
    esp_display_present_rect_t area = {0, 0, 8, 4};
    internal = false;
    assert(api.submit(api.context, &surface, &area, 123).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    complete(&lcd);
    dma_capable = false;
    assert(api.submit(api.context, &surface, &area, 124).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
    internal = dma_capable = true;
    assert(api.quiesce(api.context, 0) == ESP_OK);
    assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
}

typedef struct {
    esp_display_present_panel_adapter_t api;
    esp_display_present_surface_t surface;
    esp_display_present_rect_t area;
    esp_display_present_tx_result_t result;
    unsigned completions;
    esp_err_t completion_result;
} staged_test_t;
static bool staged_event(void *context, const esp_display_present_backend_event_t *event, bool isr)
{
    (void)isr;
    staged_test_t *test = context;
    assert(event->transaction_cookie == 91 && event->kind == ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE);
    ++test->completions;
    test->completion_result = event->result;
    return false;
}
static void *staged_submit(void *context)
{
    staged_test_t *test = context;
    test->result = test->api.submit(test->api.context, &test->surface, &test->area, 91);
    return NULL;
}
static void staged_transfer(void)
{
    for (unsigned path = 0; path < 3; ++path) {
        struct fake_lcd lcd = {.staged = true, .early = path == 0};
        esp_display_present_lcd_io_config_t config = {.panel = &lcd, .io = &lcd,
                                                      .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .max_inflight = 2, .staging_bytes = 64,
                                                      .staging_timeout_ms = path == 2 ? 1 : 1000
                                                     };
        esp_display_present_lcd_io_t *adapter;
        assert(esp_display_present_lcd_io_create(&config, &adapter) == ESP_OK);
        staged_test_t test = {0};
        assert(esp_display_present_lcd_io_get_interface(adapter, &test.api) == ESP_OK);
        assert(test.api.attach(test.api.context, staged_event, &test) == ESP_OK);
        _Alignas(64) uint8_t source[192];
        for (unsigned i = 0; i < 192; ++i) {
            source[i] = i;
        }
        test.surface = (esp_display_present_surface_t) {
            source, sizeof(source), {8, 12, 16, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}
        };
        test.area = (esp_display_present_rect_t) {
            0, 0, 8, 12
        };
        internal = dma_capable = false; /* CPU-readable source, never direct DMA. */
        pthread_t worker;
        assert(!pthread_create(&worker, NULL, staged_submit, &test));
        if (path == 1) {
            for (unsigned band = 1; band <= 3; ++band) {
                pthread_mutex_lock(&stage_lock);
                while (lcd.draws < band) {
                    pthread_cond_wait(&stage_changed, &stage_lock);
                }
                assert(lcd.pending_source != source && lcd.pending == 1);
                assert(!memcmp(lcd.pending_source, source + (band - 1) * 64, 64));
                pthread_mutex_unlock(&stage_lock);
                complete(&lcd); /* Delayed DMA read only here, before reuse. */
            }
        }
        assert(!pthread_join(worker, NULL));
        if (path == 2) {
            assert(test.result.acceptance == ESP_DISPLAY_PRESENT_TX_UNCERTAIN && test.result.result == ESP_ERR_TIMEOUT);
            assert(test.completions == 0 && lcd.pending == 1);
            assert(test.api.quiesce(test.api.context, 0) == ESP_ERR_TIMEOUT);
            assert(esp_display_present_lcd_io_destroy(adapter) == ESP_ERR_INVALID_STATE);
            assert(!memcmp(lcd.pending_source, source, 64));
            complete(&lcd);
            assert(test.completions == 1 && test.completion_result == ESP_ERR_TIMEOUT);
            assert(lcd.history_bytes == 64);
        } else {
            assert(test.result.acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED && test.result.result == ESP_OK);
            assert(test.completions == 1 && test.completion_result == ESP_OK);
            assert(lcd.draws == 3 && lcd.history_bytes == sizeof(source));
            assert(!memcmp(lcd.history, source, sizeof(source)));
        }
        assert(test.api.quiesce(test.api.context, 100) == ESP_OK);
        assert(esp_display_present_lcd_io_destroy(adapter) == ESP_OK);
        internal = dma_capable = true;
    }
}
int main(void)
{
    external_dma();
    staged_transfer();
    fifo_and_validation();
    early_and_uncertain();
    callback_exit_barrier();
    gram_integration();
    puts("PASS SPI adapter: FIFO cookies, early callbacks, cache/layout rejection, uncertain errors, detach barrier, GRAM packing");
    return 0;
}
