/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "../../src/platform/present_os.h"
#include "test_panel.h"
#include "platform/te/present_te_timer.h"
#include "backend/present_te_composer.h"
#include <pthread.h>
#include <time.h>
#include <stdio.h>

#define OK(call) assert((call) == ESP_OK)
static const present_backend_ops_t *ops = &present_te_backend;
static present_backend_t *inject_edge_on_pin;
static bool close_gate_on_pin, custom_gate_open;
static unsigned custom_gate_calls;
esp_err_t __real_present_te_composer_pin(present_te_composer_t *, present_te_frame_t, esp_display_present_source_t *);
esp_err_t __wrap_present_te_composer_pin(present_te_composer_t *composer, present_te_frame_t frame, esp_display_present_source_t *source)
{
    esp_err_t error = __real_present_te_composer_pin(composer, frame, source);
    if (error == ESP_OK && close_gate_on_pin) {
        close_gate_on_pin = false;
        custom_gate_open = false; /* Model preemption spanning the phase close. */
    }
    if (error == ESP_OK && inject_edge_on_pin) {
        present_backend_t *backend = inject_edge_on_pin;
        inject_edge_on_pin = NULL;
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        const esp_display_present_backend_event_t event = {
            .kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE,
            .timestamp_us = (uint64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000
        };
        (void)ops->on_event_isr(backend, &event);
    }
    return error;
}

typedef struct {
    test_panel_t panel;
    bool uncertain;
    bool early_reject;
    bool abort_on_stop;
} fixture_t;
static esp_display_present_tx_result_t submit(void *context, const esp_display_present_surface_t *surface,
                                              const esp_display_present_rect_t *area, uint64_t cookie)
{
    fixture_t *fixture = context;
    esp_display_present_tx_result_t result = test_panel_submit(&fixture->panel, surface, area, cookie);
    if (fixture->uncertain) {
        result = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
        };
    }
    if (fixture->early_reject) {
        result = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
        };
    }
    return result;
}

static esp_err_t stop(void *context, uint32_t timeout)
{
    fixture_t *fixture = context;
    if (fixture->abort_on_stop) {
        fixture->panel.count = 0; /* Simulated safe hardware abort, no callback. */
    }
    return test_panel_stop(&fixture->panel, timeout);
}

static uint64_t now_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static void edge(present_backend_t *backend)
{
    static _Thread_local uint64_t previous;
    uint64_t timestamp;
    do {
        timestamp = now_us();
    } while (timestamp <= previous);
    previous = timestamp;
    const esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE, .timestamp_us = timestamp};
    (void)ops->on_event_isr(backend, &event);
}

static present_backend_t *create_pool(fixture_t *fixture, size_t count)
{
    fixture->panel.width = 4;
    fixture->panel.height = 4;
    fixture->panel.bpp = 2;
    esp_display_present_backend_config_t config = {
        .compose_count = count, .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .draw_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE, .panel_order = ESP_DISPLAY_PRESENT_ORDER_NATIVE,
        .panel = {fixture, test_panel_attach, submit, stop},
        .te_window_us = 1000000, .te_budget_us = 1000,
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    return backend;
}

static present_backend_t *create(fixture_t *fixture)
{
    return create_pool(fixture, 0);
}

static esp_display_present_frame_t begin(present_backend_t *backend)
{
    esp_display_present_frame_t frame;
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(ops->begin(backend, &request, &frame));
    return frame;
}

static void write_area(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_rect_t area, uint16_t color)
{
    esp_display_present_lease_t lease;
    OK(ops->acquire(backend, frame, NULL, 0, &lease));
    size_t stride = area.width * 2 + 2; /* Padded rows into a narrow destination. */
    if (area.width == 4) {
        stride = 8;
    }
    for (uint32_t y = 0; y < area.height; y++) {
        for (uint32_t x = 0; x < area.width; x++) {
            memcpy((uint8_t *)lease.surface.pixels + y * stride + x * 2, &color, 2);
        }
    }
    esp_display_present_region_submit_t submission = {area, stride};
    OK(ops->submit_region_ex(backend, frame, lease.id, &submission));
    assert(ops->submit_region(backend, frame, lease.id) == ESP_ERR_INVALID_STATE);
}

static esp_display_present_receipt_t full(present_backend_t *backend, bool receipt)
{
    esp_display_present_frame_t frame = begin(backend);
    write_area(backend, frame, (esp_display_present_rect_t) {
        0, 0, 4, 2
    }, 0x1234);
    write_area(backend, frame, (esp_display_present_rect_t) {
        0, 2, 4, 2
    }, 0x1234);
    esp_display_present_receipt_t result = {0};
    OK(ops->end(backend, frame, receipt ? &result : NULL));
    return result;
}

static void destroy(present_backend_t *backend)
{
    OK(ops->stop(backend, 0));
    OK(ops->destroy(backend));
}

static void lifecycle(void)
{
    fixture_t fixture = {0};
    present_backend_t *backend = create(&fixture);
    esp_display_present_frame_t frame = begin(backend);
    esp_display_present_frame_coverage_t coverage = {.full_coverage = true};
    assert(ops->end_ex(backend, frame, &coverage, NULL) == ESP_ERR_INVALID_STATE);
    OK(ops->cancel_frame(backend, frame, 0));
    OK(ops->cancel_frame(backend, frame, 0));
    esp_display_present_receipt_t receipt = full(backend, true);
    edge(backend); /* ISR never directly sends. */
    assert(fixture.panel.submitted == 0);
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 1);
    esp_display_present_frame_status_t status;
    assert(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
    assert(!status.terminal);
    assert(ops->release_receipt(backend, receipt) == ESP_ERR_INVALID_STATE);
    esp_display_present_frame_request_t request = {0};
    assert(ops->begin(backend, &request, &frame) == ESP_ERR_NO_MEM);
    test_panel_complete(&fixture.panel);
    OK(ops->query(backend, receipt, &status));
    assert(status.terminal && status.result == ESP_OK && !status.present_confirmed);
    OK(ops->release_receipt(backend, receipt));
    frame = begin(backend);
    write_area(backend, frame, (esp_display_present_rect_t) {
        1, 1, 2, 2
    }, 0xabcd);
    OK(ops->end(backend, frame, NULL));
    OK(present_te_backend.service(backend)); /* Prior edge cannot release new frame. */
    assert(fixture.panel.count == 0);
    fixture.panel.reject = true;
    edge(backend);
    assert(present_te_backend.service(backend) == ESP_FAIL);
    fixture.panel.reject = false;
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 0); /* rejection consumes edge */
    edge(backend);
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 1);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(backend));
    for (unsigned y = 0; y < 4; y++) {
        for (unsigned x = 0; x < 4; x++) {
            uint16_t value;
            memcpy(&value, fixture.panel.pixels + (y * 4 + x) * 2, 2);
            assert(value == (x >= 1 && x <= 2 && y >= 1 && y <= 2 ? 0xabcd : 0x1234));
        }
    }
    /* Fire-and-forget records really recycle; early completions need no future ISR. */
    fixture.panel.immediate = true;
    for (unsigned i = 0; i < 20; i++) {
        (void)full(backend, false);
        edge(backend);
        OK(present_te_backend.service(backend));
    }
    destroy(backend);
}

static void channel_fault(void)
{
    fixture_t fixture = {0};
    present_backend_t *backend = create(&fixture);
    esp_display_present_receipt_t receipt = full(backend, true);
    esp_display_present_backend_event_t fault = {.kind = ESP_DISPLAY_PRESENT_EVENT_FAULT, .result = ESP_FAIL};
    ops->on_event(backend, &fault);
    edge(backend);
    esp_display_present_frame_status_t status;
    OK(ops->query(backend, receipt, &status));
    assert(status.terminal && status.result == ESP_FAIL && fixture.panel.submitted == 0);
    esp_display_present_frame_request_t request = {0};
    esp_display_present_frame_t frame;
    assert(ops->begin(backend, &request, &frame) == ESP_ERR_INVALID_STATE);
    OK(ops->release_receipt(backend, receipt));
    destroy(backend);
}

static void failures(void)
{
    for (unsigned mode = 0; mode < 4; mode++) {
        fixture_t fixture = {.uncertain = mode == 0, .early_reject = mode == 1,
                             .abort_on_stop = mode == 2
                            };
        fixture.panel.immediate = mode == 1;
        present_backend_t *backend = create(&fixture);
        esp_display_present_receipt_t receipt = full(backend, true);
        edge(backend);
        OK(present_te_backend.service(backend));
        if (mode == 3) {
            esp_display_present_backend_event_t fault = {.kind = ESP_DISPLAY_PRESENT_EVENT_FAULT, .result = ESP_FAIL,
                                                         .transaction_cookie = fixture.panel.pending[0].cookie
                                                        };
            ops->on_event(backend, &fault);
        }
        if (mode != 1 && mode != 2) {
            assert(ops->stop(backend, 0) == ESP_ERR_TIMEOUT);
            assert(ops->destroy(backend) == ESP_ERR_INVALID_STATE);
            test_panel_complete(&fixture.panel);
        }
        OK(ops->stop(backend, 0));
        esp_display_present_frame_status_t status;
        OK(ops->query(backend, receipt, &status));
        assert(status.terminal && status.result != ESP_OK);
        assert(ops->destroy(backend) == ESP_ERR_INVALID_STATE);
        esp_display_present_backend_fault_t fault;
        OK(ops->get_fault(backend, &fault));
        assert(fault.result != ESP_OK && fault.frame.value == receipt.value);
        OK(ops->release_receipt(backend, receipt));
        OK(ops->destroy(backend));
    }
}

static void ready_stop_and_receipts(void)
{
    fixture_t fixture = {0};
    present_backend_t *backend = create(&fixture);
    esp_display_present_receipt_t receipt = full(backend, true);
    edge(backend);
    OK(ops->stop(backend, 0)); /* Must not send READY while stopping. */
    assert(fixture.panel.submitted == 0);
    esp_display_present_frame_status_t status;
    OK(ops->query(backend, receipt, &status));
    assert(status.terminal && status.result != ESP_OK);
    OK(ops->release_receipt(backend, receipt));
    OK(ops->destroy(backend));
    fixture = (fixture_t) {
        0
    };
    fixture.panel.immediate = true;
    backend = create(&fixture);
    esp_display_present_receipt_t receipts[2];
    for (unsigned i = 0; i < 2; i++) {
        receipts[i] = full(backend, true);
        edge(backend);
        OK(present_te_backend.service(backend));
    }
    esp_display_present_frame_t frame;
    esp_display_present_frame_request_t request = {0};
    assert(ops->begin(backend, &request, &frame) == ESP_ERR_NO_MEM);
    OK(ops->release_receipt(backend, receipts[0]));
    frame = begin(backend);
    OK(ops->cancel_frame(backend, frame, 0));
    OK(ops->release_receipt(backend, receipts[1]));
    destroy(backend);
}
/* Callback thread races mailbox recording with task queries/stop. Test panel
 * fields themselves stay task-owned; event injection uses the public ISR API. */
typedef struct {
    present_backend_t *backend;
    uint64_t cookie;
} injection_t;
static void *inject(void *arg)
{
    injection_t *injection = arg;
    for (unsigned i = 0; i < 300; i++) {
        edge(injection->backend);
    }
    esp_display_present_backend_event_t done = {.kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, .transaction_cookie = injection->cookie};
    (void)ops->on_event_isr(injection->backend, &done);
    return NULL;
}

static void threaded_events(void)
{
    for (unsigned i = 0; i < 30; i++) {
        fixture_t fixture = {0};
        present_backend_t *backend = create(&fixture);
        esp_display_present_receipt_t receipt = full(backend, true);
        edge(backend);
        OK(present_te_backend.service(backend));
        assert(fixture.panel.count == 1);
        injection_t injection = {backend, fixture.panel.pending[0].cookie};
        /* Simulated DMA reads have ended before completion is sent. */
        fixture.panel.count = 0;
        pthread_t thread;
        assert(pthread_create(&thread, NULL, inject, &injection) == 0);
        esp_display_present_frame_status_t status;
        for (unsigned j = 0; j < 20; j++) {
            OK(ops->query(backend, receipt, &status));
        }
        OK(ops->stop(backend, 0));
        assert(pthread_join(thread, NULL) == 0); /* external TE producer exit barrier */
        OK(ops->query(backend, receipt, &status));
        assert(status.terminal);
        OK(ops->release_receipt(backend, receipt));
        OK(ops->destroy(backend));
    }
}

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool entered, release;
    size_t completed;
    bool fail;
} copy_gate_t;
static esp_err_t gated_copy(void *arg, const esp_display_present_copy_request_t *r)
{
    copy_gate_t *g = arg;
    pthread_mutex_lock(&g->mutex);
    g->entered = true;
    pthread_cond_broadcast(&g->changed);
    while (!g->release) {
        pthread_cond_wait(&g->changed, &g->mutex);
    }
    pthread_mutex_unlock(&g->mutex);
    if (g->fail) {
        return ESP_FAIL;
    }
    for (uint32_t y = 0; y < r->src_area.height; ++y) {
        memcpy((uint8_t *)r->dst.pixels + (r->dst_y + y) * r->dst.layout.stride_bytes + r->dst_x * 2,
               (const uint8_t *)r->src.pixels + (r->src_area.y + y) * r->src.layout.stride_bytes + r->src_area.x * 2,
               r->src_area.width * 2);
    }
    return ESP_OK;
}

static bool copied(void *arg, bool isr)
{
    assert(!isr);
    copy_gate_t *g = arg;
    pthread_mutex_lock(&g->mutex);
    g->completed++;
    pthread_cond_broadcast(&g->changed);
    pthread_mutex_unlock(&g->mutex);
    return false;
}

static void asynchronous_copy(unsigned mode)
{
    copy_gate_t g = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .fail = mode == 3};
    test_panel_t panel = {.width = 2, .height = 2, .bpp = 2};
    esp_display_present_backend_config_t cfg = {.width = 2, .height = 2, .tile_rows = 2, .buffer_count = 1, .max_frames = 2,
                                                .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = test_panel_interface(&panel), .te_window_us = 1000000,
                                                .te_budget_us = 1, .async_copy = true, .copy_execute = gated_copy, .copy_user = &g,
                                                .on_progress = copied, .progress_user = &g
                                               };
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_t f = begin(b);
    esp_display_present_lease_t lease;
    OK(ops->acquire(b, f, NULL, 0, &lease));
    memset(lease.surface.pixels, 0x5a, 8);
    OK(ops->submit_region(b, f, lease.id));
    pthread_mutex_lock(&g.mutex);
    while (!g.entered) {
        pthread_cond_wait(&g.changed, &g.mutex);
    }
    pthread_mutex_unlock(&g.mutex);
    assert(ops->acquire(b, f, NULL, 0, &lease) == ESP_ERR_TIMEOUT);
    esp_display_present_receipt_t receipt = {0};
    if (mode == 1) {
        assert(ops->cancel_frame(b, f, 0) == ESP_ERR_TIMEOUT);
    } else {
        OK(ops->end(b, f, &receipt));
        edge(b);
        OK(present_te_backend.service(b));
        assert(panel.submitted == 0); /* Never sends a partially copied frame. */
        if (mode == 2) {
            assert(ops->stop(b, 0) == ESP_ERR_TIMEOUT);
            assert(ops->destroy(b) == ESP_ERR_INVALID_STATE);
        }
    }
    pthread_mutex_lock(&g.mutex);
    g.release = true;
    pthread_cond_broadcast(&g.changed);
    while (!g.completed) {
        pthread_cond_wait(&g.changed, &g.mutex);
    }
    pthread_mutex_unlock(&g.mutex);
    if (mode == 2) {
        OK(ops->stop(b, UINT32_MAX));
    } else {
        OK(present_te_backend.service(b));
        assert(panel.submitted == 0); /* Old edge predates COPY_DRAIN -> READY. */
        if (mode == 1) {
            OK(ops->cancel_frame(b, f, 0));
        }
        if (mode == 0) {
            edge(b);
            OK(present_te_backend.service(b));
            assert(panel.count == 1);
            test_panel_complete(&panel);
            for (size_t i = 0; i < 8; ++i) {
                assert(panel.pixels[i] == 0x5a);
            }
        }
    }
    if (receipt.value) {
        esp_display_present_frame_status_t status;
        OK(ops->query(b, receipt, &status));
        assert(status.terminal);
        assert(status.result == (mode == 0 ? ESP_OK : mode == 3 ? ESP_FAIL : ESP_ERR_INVALID_STATE));
        OK(ops->release_receipt(b, receipt));
    }
    OK(ops->stop(b, UINT32_MAX));
    OK(ops->destroy(b));
    pthread_cond_destroy(&g.changed);
    pthread_mutex_destroy(&g.mutex);
}
/* A second image becoming READY must not discard an edge already usable by
 * the older READY frame, nor borrow that edge for the newer frame. */
static void ready_frames_keep_edge(void)
{
    fixture_t fixture = {0};
    present_backend_t *backend = create_pool(&fixture, 2);
    esp_display_present_receipt_t first = full(backend, true);
    uint64_t timestamp = now_us();
    const esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE, .timestamp_us = timestamp};
    (void)ops->on_event_isr(backend, &event);
    while (now_us() <= timestamp) {
    }
    esp_display_present_frame_t frame = begin(backend);
    write_area(backend, frame, (esp_display_present_rect_t) {
        0, 0, 4, 2
    }, 0x5678);
    write_area(backend, frame, (esp_display_present_rect_t) {
        0, 2, 4, 2
    }, 0x5678);
    esp_display_present_receipt_t second;
    OK(ops->end(backend, frame, &second));
    OK(present_te_backend.service(backend));
    assert(fixture.panel.submitted == 1);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(backend));
    assert(fixture.panel.submitted == 1);
    edge(backend);
    OK(present_te_backend.service(backend));
    assert(fixture.panel.submitted == 2);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(backend));
    OK(ops->release_receipt(backend, first));
    OK(ops->release_receipt(backend, second));
    destroy(backend);
}

static void compose_pool(size_t count)
{
    fixture_t fixture = {0};
    present_backend_t *b = create_pool(&fixture, count);
    esp_display_present_receipt_t first = full(b, true);
    edge(b);
    OK(present_te_backend.service(b));
    assert(fixture.panel.count == 1);
    esp_display_present_frame_t f;
    esp_display_present_frame_request_t req = {0};
    if (count == 1) {
        assert(ops->begin(b, &req, &f) == ESP_ERR_NO_MEM);
        test_panel_complete(&fixture.panel);
        OK(present_te_backend.service(b));
    }
    OK(ops->begin(b, &req, &f));
    write_area(b, f, (esp_display_present_rect_t) {
        1, 1, 1, 1
    }, 0x5678);
    esp_display_present_receipt_t second;
    OK(ops->end(b, f, &second));
    if (count == 2) {
        assert(ops->begin(b, &req, &f) == ESP_ERR_NO_MEM);
        OK(present_te_backend.service(b));
        assert(fixture.panel.submitted == 1);
        test_panel_complete(&fixture.panel); /* reads the original source NOW */
    }
    for (unsigned i = 0; i < 16; i++) {
        assert(((uint16_t*)fixture.panel.pixels)[i] == 0x1234);
    }
    edge(b);
    OK(present_te_backend.service(b));
    assert(fixture.panel.count == 1 && fixture.panel.submitted == 2);
    assert(ops->stop(b, 0) == ESP_ERR_TIMEOUT);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(b));
    for (unsigned i = 0; i < 16; i++) {
        assert(((uint16_t*)fixture.panel.pixels)[i] == (i == 5 ? 0x5678 : 0x1234));
    }
    esp_display_present_frame_status_t status;
    OK(ops->query(b, first, &status));
    assert(status.terminal && status.result == ESP_OK);
    OK(ops->release_receipt(b, first));
    OK(ops->query(b, second, &status));
    assert(status.terminal && status.result == ESP_OK);
    OK(ops->release_receipt(b, second));
    destroy(b);
}

static void compose_matrix_and_cancel(size_t compose_count, size_t tile_count)
{
    copy_gate_t g = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .release = true};
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_backend_config_t cfg = {.compose_count = compose_count, .width = 4, .height = 4, .tile_rows = 2,
                                                .buffer_count = tile_count, .max_frames = 3, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                .panel = test_panel_interface(&panel), .te_window_us = 1000000,
                                                .te_budget_us = 1, .async_copy = true, .copy_execute = gated_copy,
                                                .copy_user = &g, .on_progress = copied, .progress_user = &g
                                               };
    present_backend_t *b;
    OK(ops->create(&cfg, &b));

    /* Frame 1: full redraw establishes baseline 0x1111. */
    esp_display_present_frame_t f = begin(b);
    for (uint32_t y = 0; y < 4; y += 2) {
        esp_display_present_lease_t lease;
        esp_display_present_rect_t band = {0, y, 4, 2};
        for (;;) {
            esp_err_t e = ops->acquire(b, f, &band, 0, &lease);
            if (e == ESP_OK) {
                break;
            }
            assert(e == ESP_ERR_TIMEOUT);
            OK(present_te_backend.service(b));
            present_pause();
        }
        for (size_t i = 0; i < 8; ++i) {
            ((uint16_t *)lease.surface.pixels)[i] = 0x1111;
        }
        OK(ops->submit_region(b, f, lease.id));
    }
    esp_display_present_receipt_t r1;
    OK(ops->end(b, f, &r1));
    esp_display_present_frame_status_t st;
    for (;;) {
        edge(b);
        OK(present_te_backend.service(b));
        if (panel.count == 1) {
            test_panel_complete(&panel);
        }
        OK(ops->query(b, r1, &st));
        if (st.terminal) {
            break;
        }
        present_pause();
    }
    OK(ops->release_receipt(b, r1));

    /* Frame 2: block async copy in-flight, exercise tile_count exhaustion + cancel_frame(0) -> stop(0) -> cancel_frame(0). */
    f = begin(b);
    pthread_mutex_lock(&g.mutex);
    g.entered = false;
    g.release = false;
    g.completed = 0;
    pthread_mutex_unlock(&g.mutex);

    esp_display_present_lease_t l0, l1, l2;
    esp_display_present_rect_t b0 = {0, 0, 4, 2}, b1 = {0, 2, 4, 2};
    OK(ops->acquire(b, f, &b0, 0, &l0));
    for (size_t i = 0; i < 8; ++i) {
        ((uint16_t *)l0.surface.pixels)[i] = 0x9999;
    }
    OK(ops->submit_region(b, f, l0.id));
    pthread_mutex_lock(&g.mutex);
    while (!g.entered) {
        pthread_cond_wait(&g.changed, &g.mutex);
    }
    pthread_mutex_unlock(&g.mutex);

    if (tile_count == 1) {
        assert(ops->acquire(b, f, &b1, 0, &l1) == ESP_ERR_TIMEOUT);
    } else {
        OK(ops->acquire(b, f, &b1, 0, &l1));
        assert(l1.surface.pixels != l0.surface.pixels);
        assert(ops->acquire(b, f, &b1, 0, &l2) == ESP_ERR_TIMEOUT);
        OK(ops->cancel_region(b, f, l1.id));
    }

    assert(ops->cancel_frame(b, f, 0) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&g.mutex);
    g.release = true;
    pthread_cond_broadcast(&g.changed);
    while (!g.completed) {
        pthread_cond_wait(&g.changed, &g.mutex);
    }
    pthread_mutex_unlock(&g.mutex);
    OK(ops->cancel_frame(b, f, 1000));
    OK(ops->cancel_frame(b, f, 0)); /* Idempotent after cancel published. */

    /* Frame 3: after cancel, compose_count==1 invalidated its only buffer and MUST reject partial end();
     * compose_count==2 preserved baseline buffer and MUST allow partial end() and repair from 0x1111. */
    f = begin(b);
    esp_display_present_rect_t corner = {0, 0, 1, 1};
    OK(ops->acquire(b, f, &corner, 0, &l0));
    ((uint16_t *)l0.surface.pixels)[0] = 0x2222;
    OK(ops->submit_region(b, f, l0.id));
    for (;;) {
        OK(present_te_backend.service(b));
        pthread_mutex_lock(&g.mutex);
        size_t done = g.completed;
        pthread_mutex_unlock(&g.mutex);
        if (done >= 2) {
            break;
        }
        present_pause();
    }
    esp_display_present_receipt_t r3;
    if (compose_count == 1) {
        assert(ops->end(b, f, &r3) == ESP_ERR_INVALID_STATE);
        OK(ops->cancel_frame(b, f, 1000));
    } else {
        OK(ops->end(b, f, &r3));
        for (;;) {
            edge(b);
            OK(present_te_backend.service(b));
            if (panel.count == 1) {
                test_panel_complete(&panel);
            }
            OK(ops->query(b, r3, &st));
            if (st.terminal) {
                break;
            }
            present_pause();
        }
        assert(((uint16_t *)panel.pixels)[0] == 0x2222);
        for (size_t i = 1; i < 16; ++i) {
            assert(((uint16_t *)panel.pixels)[i] == 0x1111);
        }
        OK(ops->release_receipt(b, r3));
    }

    OK(ops->stop(b, 1000));
    OK(ops->destroy(b));
    pthread_cond_destroy(&g.changed);
    pthread_mutex_destroy(&g.mutex);
}

static void cancel_then_stop(void)
{
    copy_gate_t gate = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_backend_config_t cfg = {.compose_count = 1, .width = 4, .height = 4, .tile_rows = 2,
                                                .buffer_count = 1, .max_frames = 3, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                                                .panel = test_panel_interface(&panel), .te_window_us = 1000000,
                                                .te_budget_us = 1, .async_copy = true, .copy_execute = gated_copy,
                                                .copy_user = &gate, .on_progress = copied, .progress_user = &gate
                                               };
    present_backend_t *backend;
    OK(ops->create(&cfg, &backend));
    esp_display_present_frame_t frame = begin(backend);
    esp_display_present_rect_t area = {0, 0, 4, 2};
    esp_display_present_lease_t lease;
    OK(ops->acquire(backend, frame, &area, 0, &lease));
    memset(lease.surface.pixels, 0x12, 16);
    OK(ops->submit_region(backend, frame, lease.id));
    pthread_mutex_lock(&gate.mutex);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    assert(ops->cancel_frame(backend, frame, 0) == ESP_ERR_TIMEOUT);
    assert(ops->stop(backend, 0) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&gate.mutex);
    gate.release = true;
    pthread_cond_broadcast(&gate.changed);
    pthread_mutex_unlock(&gate.mutex);
    OK(ops->stop(backend, 1000));
    esp_display_present_backend_fault_t fault;
    OK(ops->get_fault(backend, &fault));
    assert(fault.result == ESP_OK && panel.submitted == 0);
    OK(ops->cancel_frame(backend, frame, 0));
    OK(ops->destroy(backend));
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.mutex);
}
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool entered, release;
} phase_fixture_t;

static bool phase_progress(void *user, bool isr)
{
    phase_fixture_t *fixture = user;
    assert(!isr);
    pthread_mutex_lock(&fixture->lock);
    fixture->entered = true;
    pthread_cond_broadcast(&fixture->changed);
    pthread_mutex_unlock(&fixture->lock);
    return false;
}

static void blocked_timer_callback(void *user)
{
    phase_fixture_t *fixture = user;
    pthread_mutex_lock(&fixture->lock);
    fixture->entered = true;
    pthread_cond_broadcast(&fixture->changed);
    while (!fixture->release) {
        pthread_cond_wait(&fixture->changed, &fixture->lock);
    }
    pthread_mutex_unlock(&fixture->lock);
}

static void wait_phase(phase_fixture_t *fixture)
{
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 3;
    pthread_mutex_lock(&fixture->lock);
    while (!fixture->entered) {
        assert(pthread_cond_timedwait(&fixture->changed, &fixture->lock, &deadline) == 0);
    }
    pthread_mutex_unlock(&fixture->lock);
}

static void phase_timer_lifetime(void)
{
    phase_fixture_t fixture = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    present_te_timer_t *timer;
    OK(present_te_timer_create(blocked_timer_callback, &fixture, &timer));
    OK(present_te_timer_arm(timer, now_us() + 1000));
    wait_phase(&fixture);
    assert(present_te_timer_destroy(timer, 0) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&fixture.lock);
    fixture.release = true;
    pthread_cond_broadcast(&fixture.changed);
    pthread_mutex_unlock(&fixture.lock);
    OK(present_te_timer_destroy(timer, 1000));
    pthread_cond_destroy(&fixture.changed);
    pthread_mutex_destroy(&fixture.lock);
}

static bool blocked_phase_progress(void *user, bool isr)
{
    assert(!isr);
    blocked_timer_callback(user);
    return false;
}

static void backend_phase_stop_retry(void)
{
    fixture_t fixture = {0};
    fixture.panel.width = fixture.panel.height = 4;
    fixture.panel.bpp = 2;
    phase_fixture_t progress = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {&fixture, test_panel_attach, submit, stop},
        .te_budget_us = 100000,
        .te_timing = {.period_us = 500000, .scan_us = 400000, .guard_us = 1000},
        .on_progress = blocked_phase_progress, .progress_user = &progress
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    esp_display_present_receipt_t receipt = full(backend, true);
    uint64_t current = now_us();
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE, .timestamp_us = current - 500000};
    (void)ops->on_event_isr(backend, &event);
    event.timestamp_us = current;
    (void)ops->on_event_isr(backend, &event);
    OK(present_te_backend.service(backend));
    wait_phase(&progress);
    assert(ops->stop(backend, 0) == ESP_ERR_TIMEOUT);
    assert(ops->destroy(backend) == ESP_ERR_INVALID_STATE);
    assert(fixture.panel.count == 0);
    pthread_mutex_lock(&progress.lock);
    progress.release = true;
    pthread_cond_broadcast(&progress.changed);
    pthread_mutex_unlock(&progress.lock);
    OK(ops->stop(backend, 1000));
    OK(ops->release_receipt(backend, receipt));
    OK(ops->destroy(backend));
    pthread_cond_destroy(&progress.changed);
    pthread_mutex_destroy(&progress.lock);
}

static esp_err_t unused_gate(void *user, size_t bytes, uint64_t after, uint64_t *edge_out)
{
    (void)user;
    (void)bytes;
    (void)after;
    (void)edge_out;
    return ESP_ERR_TIMEOUT;
}

static esp_err_t single_use_gate(void *user, size_t bytes, uint64_t after, uint64_t *edge_out)
{
    (void)user;
    (void)bytes;
    ++custom_gate_calls;
    if (!custom_gate_open) {
        return ESP_ERR_TIMEOUT;
    }
    *edge_out = now_us();
    return *edge_out > after ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* A phase may close during source pinning; the sole gate call must see it. */
static void gate_checked_after_pin(void)
{
    fixture_t fixture = {0};
    fixture.panel.width = fixture.panel.height = 4;
    fixture.panel.bpp = 2;
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {&fixture, test_panel_attach, submit, stop},
        .te_gate = {.admit = single_use_gate}
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    esp_display_present_receipt_t receipt = full(backend, true);
    custom_gate_open = true;
    custom_gate_calls = 0;
    close_gate_on_pin = true;
    OK(present_te_backend.service(backend));
    assert(!close_gate_on_pin && custom_gate_calls == 1 && fixture.panel.count == 0);
    custom_gate_open = true;
    OK(present_te_backend.service(backend));
    assert(custom_gate_calls == 2 && fixture.panel.count == 1);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(backend));
    OK(ops->release_receipt(backend, receipt));
    OK(ops->stop(backend, 1000));
    OK(ops->destroy(backend));
}

static void reject_mixed_policies(void)
{
    fixture_t fixture = {0};
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {&fixture, test_panel_attach, submit, stop},
        .te_gate = {.admit = unused_gate}, .te_budget_us = 100000,
        .te_timing = {.period_us = 500000, .scan_us = 400000, .guard_us = 1000}
    };
    present_backend_t *backend = NULL;
    assert(ops->create(&config, &backend) == ESP_ERR_INVALID_ARG && !backend);
}

/* An edge arriving during pin is the edge consumed by the final timing
 * check; a definite rejection must not reuse it on the next service. */
static void native_reject_during_edge_change(void)
{
    fixture_t fixture = {0};
    fixture.panel.width = fixture.panel.height = 4;
    fixture.panel.bpp = 2;
    fixture.panel.reject = true;
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {&fixture, test_panel_attach, submit, stop},
        .te_budget_us = 100000,
        .te_timing = {
            .period_us = 500000, .scan_us = 400000, .edge_to_scan_us = 150000,
            .guard_us = 10000, .jitter_us = 10000
        }
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    esp_display_present_receipt_t receipt = full(backend, true);
    uint64_t current = now_us();
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE, .timestamp_us = current - 998000};
    (void)ops->on_event_isr(backend, &event);
    event.timestamp_us = current - 498000;
    (void)ops->on_event_isr(backend, &event);
    inject_edge_on_pin = backend;
    assert(present_te_backend.service(backend) == ESP_FAIL);
    assert(!inject_edge_on_pin && fixture.panel.count == 0);
    fixture.panel.reject = false;
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 0); /* No new physical edge after rejection. */
    OK(ops->stop(backend, 1000));
    OK(ops->release_receipt(backend, receipt));
    OK(ops->destroy(backend));
}

static void native_phase_wakeup(void)
{
    fixture_t fixture = {0};
    fixture.panel.width = fixture.panel.height = 4;
    fixture.panel.bpp = 2;
    phase_fixture_t progress = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = {&fixture, test_panel_attach, submit, stop},
        .te_budget_us = 100000,
        .te_timing = {.period_us = 500000, .scan_us = 400000, .guard_us = 1000},
        .on_progress = phase_progress, .progress_user = &progress
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    esp_display_present_receipt_t receipt = full(backend, true);
    uint64_t current = now_us();
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_TE_EDGE, .timestamp_us = current - 500000};
    (void)ops->on_event_isr(backend, &event);
    event.timestamp_us = current;
    (void)ops->on_event_isr(backend, &event);
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 0); /* Outside the safe phase. */
    wait_phase(&progress); /* No new edge: timer must wake us. */
    OK(present_te_backend.service(backend));
    assert(fixture.panel.count == 1);
    test_panel_complete(&fixture.panel);
    OK(present_te_backend.service(backend));
    OK(ops->release_receipt(backend, receipt));
    OK(ops->stop(backend, 1000));
    OK(ops->destroy(backend));
    pthread_cond_destroy(&progress.changed);
    pthread_mutex_destroy(&progress.lock);
}

int main(void)
{
    gate_checked_after_pin();
    reject_mixed_policies();
    native_reject_during_edge_change();
    phase_timer_lifetime();
    native_phase_wakeup();
    backend_phase_stop_retry();
    ready_frames_keep_edge();
    cancel_then_stop();
    compose_pool(1);
    compose_pool(2);
    for (size_t compose_count = 1; compose_count <= 2; ++compose_count) {
        for (size_t tile_count = 1; tile_count <= 2; ++tile_count) {
            compose_matrix_and_cancel(compose_count, tile_count);
        }
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        asynchronous_copy(mode);
    }
    lifecycle();
    failures();
    channel_fault();
    ready_stop_and_receipts();
    threaded_events();
    puts("TE backend lifecycle, timing gate and callback races passed");
    return 0;
}
