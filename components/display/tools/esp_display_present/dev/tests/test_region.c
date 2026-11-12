/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    esp_display_present_panel_event_fn callback;
    void *user;
    esp_display_present_tx_result_t response;
    bool early, isr, abort_on_stop, attach_fail;
    uint64_t pending[32], last_cookie;
    size_t count, submits;
    uint8_t last_pixels[128];
    esp_display_present_rect_t last_area;
    size_t last_size;
} panel_t;

static const present_backend_ops_t *ops = &present_region_backend;
static const esp_display_present_rect_t area = { 0, 0, 4, 4 };

static esp_err_t attach(void *ctx, esp_display_present_panel_event_fn callback, void *user)
{
    panel_t *p = ctx;
    if (p->attach_fail) {
        return ESP_FAIL;
    }
    p->callback = callback;
    p->user = user;
    return ESP_OK;
}

static void emit(panel_t *p, uint64_t cookie, esp_display_present_event_kind_t kind, esp_err_t result)
{
    esp_display_present_backend_event_t event = { .kind = kind, .transaction_cookie = cookie, .result = result };
    assert(p->callback);
    (void)p->callback(p->user, &event, p->isr);
}

static esp_display_present_tx_result_t submit(void *ctx, const esp_display_present_surface_t *surface,
                                              const esp_display_present_rect_t *rect, uint64_t cookie)
{
    panel_t *p = ctx;
    assert(rect->width == surface->layout.width && rect->height == surface->layout.height);
    size_t size = surface->layout.stride_bytes * surface->layout.height;
    assert(size <= sizeof(p->last_pixels));
    memcpy(p->last_pixels, surface->pixels, size);
    p->last_area = *rect;
    p->last_size = size;
    p->last_cookie = cookie;
    ++p->submits;
    if (p->early) {
        emit(p, cookie, ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, ESP_OK);
    } else if (p->response.acceptance != ESP_DISPLAY_PRESENT_TX_REJECTED) {
        assert(p->count < 32);
        p->pending[p->count++] = cookie;
    }
    return p->response;
}

static void complete(panel_t *p)
{
    assert(p->count);
    uint64_t cookie = p->pending[0];
    --p->count;
    memmove(p->pending, p->pending + 1, p->count * sizeof(p->pending[0]));
    emit(p, cookie, ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, ESP_OK);
}

static esp_err_t quiesce(void *ctx, uint32_t timeout)
{
    (void)timeout;
    panel_t *p = ctx;
    if (p->count && !p->abort_on_stop) {
        return ESP_ERR_TIMEOUT;
    }
    p->count = 0;
    p->callback = NULL;
    p->user = NULL;
    return ESP_OK;
}

static esp_display_present_backend_config_t config(panel_t *p, bool swap)
{
    return (esp_display_present_backend_config_t) {
        .width = 4, .height = 4, .tile_rows = 2,
        .buffer_count = 1, .max_frames = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
        .panel_order = swap ? ESP_DISPLAY_PRESENT_ORDER_SWAPPED : ESP_DISPLAY_PRESENT_ORDER_NATIVE,
        .panel = { p, attach, submit, quiesce }
    };
}

static present_backend_t *create(panel_t *p, bool swap)
{
    present_backend_t *g;
    esp_display_present_backend_config_t c = config(p, swap);
    assert(ops->create(&c, &g) == ESP_OK);
    return g;
}

static esp_display_present_frame_t begin(present_backend_t *g)
{
    esp_display_present_frame_t f;
    esp_display_present_frame_request_t r = { .full_redraw = true };
    assert(ops->begin(g, &r, &f) == ESP_OK);
    return f;
}

static esp_display_present_lease_t acquire(present_backend_t *g, esp_display_present_frame_t f)
{
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, &area, 0, &lease) == ESP_OK);
    assert(lease.logical_area.height == 2);
    uint8_t *bytes = lease.surface.pixels;
    for (size_t i = 0; i < 16; ++i) {
        bytes[i] = (uint8_t)i;
    }
    return lease;
}

static esp_display_present_receipt_t end(present_backend_t *g, esp_display_present_frame_t f)
{
    esp_display_present_receipt_t r;
    assert(ops->end(g, f, &r) == ESP_OK);
    return r;
}

static void release(present_backend_t *g, esp_display_present_receipt_t r, esp_err_t result)
{
    esp_display_present_frame_status_t status;
    assert(ops->wait(g, r, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_OK);
    assert(status.terminal && status.src_released && !status.present_confirmed && status.result == result);
    assert(ops->release_receipt(g, r) == ESP_OK);
    assert(ops->query(g, r, &status) == ESP_ERR_INVALID_STATE);
}

static void destroy(present_backend_t *g)
{
    assert(ops->stop(g, 0) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
}

static panel_t normal_panel(void)
{
    return (panel_t) {
        .response = { ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK }
    };
}

static void immediate_and_retry(void)
{
    for (int isr = 0; isr < 2; ++isr) {
        panel_t p = normal_panel();
        p.early = true;
        p.isr = isr;
        present_backend_t *g = create(&p, true);
        esp_display_present_backend_caps_t caps;
        assert(ops->get_caps(g, &caps) == ESP_OK && caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
        assert(!caps.supports_present_confirmation);
        assert(ops->destroy(g) == ESP_ERR_INVALID_STATE);
        esp_display_present_frame_t f = begin(g);
        esp_display_present_lease_t lease = acquire(g, f);
        esp_display_present_receipt_t r;
        assert(ops->end(g, f, &r) == ESP_ERR_INVALID_STATE && r.value == 0);
        assert(ops->submit_region(g, f, lease.id) == ESP_OK);
        for (size_t i = 0; i < 16; ++i) {
            assert(p.last_pixels[i] == (i ^ 1));
        }
        assert(ops->submit_region(g, f, lease.id) == ESP_ERR_INVALID_STATE);
        r = end(g, f);
        esp_display_present_frame_status_t status;
        assert(ops->wait(g, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status) == ESP_ERR_NOT_SUPPORTED);
        release(g, r, ESP_OK);

        p.early = false;
        p.response = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_NO_MEM
        };
        f = begin(g);
        lease = acquire(g, f);
        for (int retry = 0; retry < 3; ++retry) {
            assert(ops->submit_region(g, f, lease.id) == ESP_ERR_NO_MEM);
            for (size_t i = 0; i < 16; ++i) {
                assert(((uint8_t *)lease.surface.pixels)[i] == i);
                assert(p.last_pixels[i] == (i ^ 1));
            }
        }
        p.early = true;
        p.response = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
        };
        assert(ops->submit_region(g, f, lease.id) == ESP_OK);
        release(g, end(g, f), ESP_OK);
        destroy(g);
    }
}

static void delayed_and_stale(void)
{
    panel_t p = normal_panel();
    present_backend_t *g = create(&p, false);
    esp_display_present_frame_t first = begin(g);
    esp_display_present_lease_t lease = acquire(g, first);
    assert(ops->submit_region(g, first, lease.id) == ESP_OK);
    esp_display_present_receipt_t r1 = end(g, first);
    uint64_t old_cookie = p.last_cookie;
    esp_display_present_frame_status_t status;
    assert(ops->wait(g, r1, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 2, &status) == ESP_ERR_TIMEOUT);
    assert(ops->release_receipt(g, r1) == ESP_ERR_INVALID_STATE);
    esp_display_present_frame_t second = begin(g);
    assert(ops->acquire(g, second, &area, 0, &lease) == ESP_ERR_TIMEOUT);
    emit(&p, UINT64_MAX, ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, ESP_OK);
    assert(ops->acquire(g, second, &area, 0, &lease) == ESP_ERR_TIMEOUT);
    complete(&p);
    lease = acquire(g, second);
    assert(ops->submit_region(g, second, lease.id) == ESP_OK);
    esp_display_present_receipt_t r2 = end(g, second);
    emit(&p, old_cookie, ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, ESP_OK);
    assert(ops->wait(g, r2, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
    esp_display_present_frame_request_t request = {0};
    esp_display_present_frame_t third;
    assert(ops->begin(g, &request, &third) == ESP_ERR_NO_MEM && third.value == 0);
    complete(&p);
    release(g, r1, ESP_OK);
    release(g, r2, ESP_OK);
    destroy(g);
}

static void faults_and_uncertain(void)
{
    for (int early = 0; early < 2; ++early) {
        panel_t p = normal_panel();
        p.early = early;
        p.response = (esp_display_present_tx_result_t) {
            early ? ESP_DISPLAY_PRESENT_TX_REJECTED : ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
        };
        present_backend_t *g = create(&p, false);
        esp_display_present_frame_t f = begin(g);
        esp_display_present_lease_t lease = acquire(g, f);
        assert(ops->submit_region(g, f, lease.id) == ESP_OK);
        assert(ops->cancel_region(g, f, lease.id) == ESP_ERR_INVALID_STATE);
        esp_display_present_receipt_t r = end(g, f);
        if (!early) {
            emit(&p, p.last_cookie, ESP_DISPLAY_PRESENT_EVENT_FAULT, ESP_FAIL);
            esp_display_present_frame_status_t status;
            assert(ops->wait(g, r, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
            assert(status.result == ESP_FAIL && !status.src_released);
            complete(&p);
        }
        release(g, r, ESP_FAIL);
        destroy(g);
    }
}

static void cancel_and_stop(void)
{
    panel_t p = normal_panel();
    present_backend_t *g = create(&p, false);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease = acquire(g, f);
    assert(ops->cancel_region(g, f, lease.id) == ESP_OK);
    assert(ops->cancel_region(g, f, lease.id) == ESP_ERR_INVALID_STATE);
    lease = acquire(g, f);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    assert(ops->cancel_frame(g, f, 0) == ESP_ERR_TIMEOUT);
    esp_display_present_lease_t unused;
    assert(ops->acquire(g, f, &area, 0, &unused) == ESP_ERR_INVALID_STATE);
    complete(&p);
    assert(ops->cancel_frame(g, f, 0) == ESP_OK);
    f = begin(g);
    lease = acquire(g, f);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    esp_display_present_receipt_t r = end(g, f);
    assert(ops->stop(g, 0) == ESP_ERR_TIMEOUT);
    assert(ops->destroy(g) == ESP_ERR_INVALID_STATE);
    esp_display_present_frame_request_t request = {0};
    assert(ops->begin(g, &request, &f) == ESP_ERR_INVALID_STATE);
    complete(&p);
    assert(ops->stop(g, 0) == ESP_OK);
    assert(ops->destroy(g) == ESP_ERR_INVALID_STATE);
    release(g, r, ESP_OK);
    assert(ops->destroy(g) == ESP_OK);

    p = normal_panel();
    p.abort_on_stop = true;
    g = create(&p, false);
    f = begin(g);
    lease = acquire(g, f);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    r = end(g, f);
    assert(ops->stop(g, 0) == ESP_OK);
    release(g, r, ESP_ERR_INVALID_STATE);
    destroy(g);
}

static void *complete_later(void *arg)
{
    const struct timespec delay = { .tv_nsec = 3000000 };
    nanosleep(&delay, NULL);
    complete(arg);
    return NULL;
}

static void threaded_completion_and_instances(void)
{
    panel_t p = normal_panel(), other = normal_panel();
    present_backend_t *a = create(&p, false), *b = create(&other, false);
    esp_display_present_frame_t fa = begin(a), fb = begin(b);
    esp_display_present_lease_t la = acquire(a, fa), lb = acquire(b, fb);
    assert(ops->submit_region(b, fb, la.id) == ESP_ERR_INVALID_STATE);
    assert(ops->submit_region(a, fa, la.id) == ESP_OK);
    esp_display_present_receipt_t r = end(a, fa);
    esp_display_present_frame_status_t status;
    assert(ops->query(b, r, &status) == ESP_ERR_INVALID_STATE);
    pthread_t worker;
    assert(pthread_create(&worker, NULL, complete_later, &p) == 0);
    assert(ops->wait(a, r, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 1000, &status) == ESP_OK);
    assert(pthread_join(worker, NULL) == 0);
    release(a, r, ESP_OK);
    assert(ops->cancel_region(b, fb, lb.id) == ESP_OK);
    assert(ops->cancel_frame(b, fb, 0) == ESP_OK);
    destroy(a);
    destroy(b);
}

static void invalid_configuration(void)
{
    panel_t p = normal_panel();
    esp_display_present_backend_config_t c = config(&p, false);
    present_backend_t *g = NULL;
    c.buffer_count = 0;
    assert(ops->create(&c, &g) == ESP_ERR_INVALID_ARG && !g);
    c = config(&p, false);
    c.format = ESP_DISPLAY_PRESENT_FORMAT_RGB888;
    c.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    assert(ops->create(&c, &g) == ESP_ERR_INVALID_ARG && !g);
    /* Shared constraints keep their plan-side error code at direct create. */
    c = config(&p, false);
    c.rotation = (esp_display_present_engine_rotation_t)4;
    assert(ops->create(&c, &g) == ESP_ERR_INVALID_ARG && !g);
    c = config(&p, false);
    c.max_frames = 33;
    assert(ops->create(&c, &g) == ESP_ERR_INVALID_ARG && !g);
    c = config(&p, false);
    c.draw_order = (esp_display_present_byte_order_t)9;
    assert(ops->create(&c, &g) == ESP_ERR_INVALID_ARG && !g);
    /* Layer-specific couplings keep their own divergent error code here. */
    c = config(&p, false);
    c.tx_staging = true;
    assert(ops->create(&c, &g) == ESP_ERR_NOT_SUPPORTED && !g);
    c = config(&p, false);
    c.async_copy = true;
    c.in_place = true;
    assert(ops->create(&c, &g) == ESP_ERR_NOT_SUPPORTED && !g);
    c = config(&p, false);
    p.attach_fail = true;
    assert(ops->create(&c, &g) == ESP_FAIL && !g && !p.callback);
}

static void multi_tile_frame(void)
{
    panel_t p = normal_panel();
    esp_display_present_backend_config_t c = config(&p, false);
    c.buffer_count = 2;
    present_backend_t *g;
    assert(ops->create(&c, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t first = acquire(g, f), second = acquire(g, f);
    assert(first.surface.pixels != second.surface.pixels);
    assert(ops->submit_region(g, f, first.id) == ESP_OK);
    p.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_NO_MEM
    };
    assert(ops->submit_region(g, f, second.id) == ESP_ERR_NO_MEM);
    complete(&p); /* A completed tile cannot seal a still-building frame. */
    esp_display_present_receipt_t r;
    assert(ops->end(g, f, &r) == ESP_ERR_INVALID_STATE);
    p.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
    assert(ops->submit_region(g, f, second.id) == ESP_OK);
    first = acquire(g, f);
    assert(ops->submit_region(g, f, first.id) == ESP_OK);
    r = end(g, f);
    complete(&p);
    esp_display_present_frame_status_t status;
    assert(ops->query(g, r, &status) == ESP_OK && !status.terminal);
    assert(ops->release_receipt(g, r) == ESP_ERR_INVALID_STATE);
    complete(&p);
    release(g, r, ESP_OK);
    destroy(g);
}

static void extended_contracts(void)
{
    /* 1. Fire-and-forget end(NULL) auto-recycles frame records beyond max_frames. */
    panel_t p = normal_panel();
    present_backend_t *g = create(&p, false);
    void *draw_bufs[2] = {0};
    size_t draw_count = 0, draw_bytes = 0;
    assert(ops->get_draw_buffers(g, draw_bufs, 2, &draw_count, &draw_bytes) == ESP_OK);
    assert(draw_count == 1 && draw_bufs[0] != NULL && draw_bytes == 16);

    for (int i = 0; i < 6; ++i) {
        esp_display_present_frame_t f = begin(g);
        esp_display_present_lease_t lease;
        /* acquire with NULL area returns full-tile scratch buffer. */
        assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
        assert(lease.logical_area.width == 4 && lease.logical_area.height == 2);
        uint8_t *bytes = lease.surface.pixels;
        for (size_t k = 0; k < 16; ++k) {
            bytes[k] = (uint8_t)(k + i);
        }
        /* Submit a 2x2 sub-rectangle with padded stride = 8 bytes (4 pixels wide). */
        esp_display_present_region_submit_t sub = {
            .logical_area = { 1, 0, 2, 2 },
            .stride_bytes = 8,
        };
        assert(ops->submit_region_ex(g, f, lease.id, &sub) == ESP_OK);
        assert(p.last_area.x == 1 && p.last_area.y == 0 && p.last_size == 8);
        const size_t indices[8] = {0, 1, 2, 3, 8, 9, 10, 11};
        for (size_t k = 0; k < 8; ++k) {
            assert(p.last_pixels[k] == (uint8_t)(indices[k] + i));
        }
        assert(ops->end(g, f, NULL) == ESP_OK);
        assert(ops->cancel_frame(g, f, 0) == ESP_ERR_INVALID_STATE);
        complete(&p);
    }

    /* 2. cancel_frame timeout followed by background ISR completion self-heals
     *    and allows idempotent cancel_frame retry as well as new begin(). */
    esp_display_present_frame_t cf = begin(g);
    esp_display_present_lease_t cl = acquire(g, cf);
    assert(ops->submit_region(g, cf, cl.id) == ESP_OK);
    assert(ops->cancel_frame(g, cf, 0) == ESP_ERR_TIMEOUT);
    complete(&p); /* Background ISR finishes the last in-flight slot. */
    assert(ops->cancel_frame(g, cf, 0) == ESP_OK);
    assert(ops->cancel_frame(g, cf, 0) == ESP_OK);
    /* A brand new frame can now begin without being wedged. */
    esp_display_present_frame_t nf = begin(g);
    assert(ops->cancel_frame(g, cf, 0) == ESP_OK);
    assert(ops->end(g, nf, NULL) == ESP_OK);
    nf = begin(g);
    assert(ops->cancel_frame(g, nf, 0) == ESP_OK);
    assert(ops->cancel_frame(g, nf, 0) == ESP_OK);
    assert(ops->cancel_frame(g, cf, 0) == ESP_ERR_INVALID_STATE);
    destroy(g);

    /* 3. In-place byte-order swap and automatic rollback on driver rejection. */
    p = normal_panel();
    esp_display_present_backend_config_t cfg = config(&p, true);
    cfg.in_place = true;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease = acquire(g, f);
    p.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_NO_MEM
    };
    assert(ops->submit_region(g, f, lease.id) == ESP_ERR_NO_MEM);
    /* Caller's lease pixels must be restored to draw_order after rejection! */
    for (size_t i = 0; i < 16; ++i) {
        assert(((uint8_t *)lease.surface.pixels)[i] == i);
    }
    p.early = true;
    p.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    for (size_t i = 0; i < 16; ++i) {
        assert(p.last_pixels[i] == (i ^ 1));
    }
    assert(ops->end(g, f, NULL) == ESP_OK);
    destroy(g);
}

static void retained_fault_and_packing(void)
{
    for (int inplace = 0; inplace < 2; ++inplace) {
        panel_t p = normal_panel();
        esp_display_present_backend_config_t cfg = config(&p, true);
        cfg.in_place = inplace;
        present_backend_t *g;
        assert(ops->create(&cfg, &g) == ESP_OK);
        esp_display_present_frame_t f = begin(g);
        esp_display_present_lease_t lease = acquire(g, f);
        esp_display_present_region_submit_t sub = { .logical_area = {2, 1, 2, 2}, .stride_bytes = 8 };
        p.response = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_ERR_NO_MEM
        };
        assert(ops->submit_region_ex(g, f, lease.id, &sub) == ESP_ERR_NO_MEM);
        for (size_t i = 0; i < 16; ++i) {
            assert(((uint8_t *)lease.surface.pixels)[i] == i);
        }
        /* Rejection must preserve acquired metadata as well as source bytes. */
        p.response = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
        };
        assert(ops->submit_region(g, f, lease.id) == ESP_OK);
        assert(p.last_area.x == 0 && p.last_area.width == 4 && p.last_size == 16);
        assert(ops->end(g, f, NULL) == ESP_OK);
        complete(&p);
        f = begin(g);
        lease = acquire(g, f);
        assert(ops->submit_region_ex(g, f, lease.id, &sub) == ESP_OK);
        const uint8_t expected[] = {1, 0, 3, 2, 9, 8, 11, 10};
        assert(p.last_size == sizeof(expected) && memcmp(p.last_pixels, expected, sizeof(expected)) == 0);
        uint64_t failed_cookie = p.last_cookie;
        assert(ops->end(g, f, NULL) == ESP_OK);
        emit(&p, failed_cookie, ESP_DISPLAY_PRESENT_EVENT_FAULT, ESP_FAIL);
        complete(&p); /* Discards the frame record, not its failure evidence. */
        esp_display_present_backend_fault_t fault;
        assert(ops->get_fault(g, &fault) == ESP_OK);
        assert(fault.result == ESP_FAIL && fault.frame.value == f.value && fault.transaction_cookie == failed_cookie);
        esp_display_present_frame_t later;
        esp_display_present_frame_request_t request = {0};
        assert(ops->begin(g, &request, &later) == ESP_ERR_INVALID_STATE);
        assert(later.value == 0);
        assert(ops->get_fault(g, &fault) == ESP_OK && fault.frame.value == f.value);
        destroy(g);
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

static void queued_region(unsigned mode)
{
    copy_gate_t gate = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .fail = mode == 5};
    panel_t panel = {.response = {ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK}};
    esp_display_present_backend_config_t cfg = config(&panel, false);
    cfg.async_copy = true;
    cfg.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    cfg.copy_execute = gated_copy;
    cfg.copy_user = &gate;
    cfg.on_progress = copied;
    cfg.progress_user = &gate;
    present_backend_t *g;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    memset(lease.surface.pixels, 0x6a, lease.surface.capacity_bytes);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    pthread_mutex_lock(&gate.mutex);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_ERR_TIMEOUT);
    assert(present_region_backend.service(g) == ESP_OK && panel.submits == 0);
    esp_display_present_receipt_t receipt = {0};
    if (mode == 3) {
        assert(ops->cancel_frame(g, f, 0) == ESP_ERR_TIMEOUT);
    } else {
        assert(ops->end(g, f, &receipt) == ESP_OK);
    }
    if (mode == 4) {
        assert(ops->stop(g, 0) == ESP_ERR_TIMEOUT);
        assert(ops->destroy(g) == ESP_ERR_INVALID_STATE);
    }
    pthread_mutex_lock(&gate.mutex);
    gate.release = true;
    pthread_cond_broadcast(&gate.changed);
    while (!gate.completed) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    if (mode == 1)
        panel.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL
    };
    if (mode == 2)
        panel.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_UNCERTAIN, ESP_FAIL
    };
    assert(present_region_backend.service(g) == (mode == 1 ? ESP_FAIL : ESP_OK));
    if (mode == 1) {
        esp_display_present_backend_fault_t fault;
        assert(ops->get_fault(g, &fault) == ESP_OK && fault.result == ESP_OK);
        panel.response = (esp_display_present_tx_result_t) {
            ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
        };
        assert(present_region_backend.service(g) == ESP_OK);
    }
    if (mode < 3) {
        assert(panel.count == 1);
        for (size_t i = 0; i < 16; ++i) {
            assert(panel.last_pixels[i] == 0x6a);
        }
        complete(&panel);
    } else {
        assert(panel.submits == 0);
    }
    if (mode == 3) {
        assert(ops->cancel_frame(g, f, 0) == ESP_OK);
    }
    if (receipt.value) {
        esp_display_present_frame_status_t status;
        assert(ops->query(g, receipt, &status) == ESP_OK && status.terminal);
        assert(status.result == ((mode == 2 || mode == 5) ? ESP_FAIL : mode == 4 ? ESP_ERR_INVALID_STATE : ESP_OK));
        assert(ops->release_receipt(g, receipt) == ESP_OK);
    }
    assert(ops->stop(g, 1000) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.mutex);
}

static void queued_packing(void)
{
    panel_t panel = {.response = {ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK}, .early = true};
    esp_display_present_backend_config_t cfg = config(&panel, true);
    cfg.async_copy = true;
    present_backend_t *g;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    const uint8_t source[12] = {1, 2, 3, 4, 0xaa, 0xbb, 5, 6, 7, 8, 0xcc, 0xdd};
    memcpy(lease.surface.pixels, source, sizeof(source));
    esp_display_present_region_submit_t sub = {{1, 1, 2, 2}, 6};
    assert(ops->submit_region_ex(g, f, lease.id, &sub) == ESP_OK);
    esp_display_present_receipt_t receipt;
    assert(ops->end(g, f, &receipt) == ESP_OK);
    esp_display_present_frame_status_t status;
    assert(ops->wait(g, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 1000, &status) == ESP_OK);
    assert(status.result == ESP_OK);
    const uint8_t expected[8] = {2, 1, 4, 3, 6, 5, 8, 7};
    assert(panel.last_size == sizeof(expected) && !memcmp(panel.last_pixels, expected, sizeof(expected)));
    assert(ops->release_receipt(g, receipt) == ESP_OK);
    assert(ops->stop(g, 1000) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
}

static void queued_fifo(void)
{
    copy_gate_t gate = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    panel_t panel = {.response = {ESP_DISPLAY_PRESENT_TX_REJECTED, ESP_FAIL}};
    esp_display_present_backend_config_t cfg = config(&panel, false);
    cfg.buffer_count = 2;
    cfg.async_copy = true;
    cfg.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    cfg.on_progress = copied;
    cfg.progress_user = &gate;
    present_backend_t *g;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    /* Acquire both before submitting, so acquire's pump cannot send either. */
    esp_display_present_lease_t leases[2];
    for (size_t i = 0; i < 2; ++i) {
        assert(ops->acquire(g, f, NULL, 0, &leases[i]) == ESP_OK);
    }
    for (size_t i = 0; i < 2; ++i) {
        memset(leases[i].surface.pixels, 0x20 + i, leases[i].surface.capacity_bytes);
        assert(ops->submit_region(g, f, leases[i].id) == ESP_OK);
    }
    esp_display_present_receipt_t receipt;
    assert(ops->end(g, f, &receipt) == ESP_OK);
    pthread_mutex_lock(&gate.mutex);
    while (gate.completed < 2) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    assert(present_region_backend.service(g) == ESP_FAIL);
    assert(panel.submits == 1 && panel.last_pixels[0] == 0x20);
    panel.response = (esp_display_present_tx_result_t) {
        ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK
    };
    assert(present_region_backend.service(g) == ESP_OK);
    assert(panel.submits == 3 && panel.count == 2 && panel.last_pixels[0] == 0x21);
    complete(&panel);
    complete(&panel);
    assert(ops->release_receipt(g, receipt) == ESP_OK);
    assert(ops->stop(g, 1000) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.mutex);
}
static esp_err_t fail_if_copy(void *arg, const esp_display_present_copy_request_t *request)
{
    (void)arg;
    (void)request;
    assert(0);
    return ESP_FAIL;
}

static void direct_async_holds_draw(void)
{
    panel_t panel = {.response = {ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK}};
    esp_display_present_backend_config_t cfg = config(&panel, false);
    cfg.async_copy = true;
    cfg.copy_execute = fail_if_copy;
    present_backend_t *g;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    void *pixels = lease.surface.pixels;
    memset(pixels, 0x5a, lease.surface.capacity_bytes);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    assert(panel.submits == 1);
    for (size_t i = 0; i < 16; ++i) {
        assert(panel.last_pixels[i] == 0x5a);
    }
    esp_display_present_lease_t again;
    assert(ops->acquire(g, f, NULL, 0, &again) == ESP_ERR_TIMEOUT);
    complete(&panel);
    assert(ops->acquire(g, f, NULL, 0, &again) == ESP_OK);
    assert(again.surface.pixels == pixels);
    assert(ops->cancel_region(g, f, again.id) == ESP_OK);
    assert(ops->cancel_frame(g, f, 0) == ESP_OK);
    assert(ops->stop(g, 1000) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
}

static void draw_reused_while_tx_inflight(void)
{
    copy_gate_t gate = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    panel_t panel = {.response = {ESP_DISPLAY_PRESENT_TX_ACCEPTED, ESP_OK}};
    esp_display_present_backend_config_t cfg = config(&panel, false);
    cfg.panel_order = ESP_DISPLAY_PRESENT_ORDER_SWAPPED;
    cfg.async_copy = true;
    cfg.copy_execute = gated_copy;
    cfg.copy_user = &gate;
    cfg.on_progress = copied;
    cfg.progress_user = &gate;
    present_backend_t *g;
    assert(ops->create(&cfg, &g) == ESP_OK);
    esp_display_present_frame_t f = begin(g);
    esp_display_present_lease_t lease;
    assert(ops->acquire(g, f, NULL, 0, &lease) == ESP_OK);
    void *pixels = lease.surface.pixels;
    memset(pixels, 0x11, lease.surface.capacity_bytes);
    assert(ops->submit_region(g, f, lease.id) == ESP_OK);
    pthread_mutex_lock(&gate.mutex);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    esp_display_present_lease_t again;
    assert(ops->acquire(g, f, NULL, 0, &again) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&gate.mutex);
    gate.release = true;
    pthread_cond_broadcast(&gate.changed);
    while (!gate.completed) {
        pthread_cond_wait(&gate.changed, &gate.mutex);
    }
    pthread_mutex_unlock(&gate.mutex);
    assert(present_region_backend.service(g) == ESP_OK);
    assert(panel.submits == 1);
    for (size_t i = 0; i < 16; ++i) {
        assert(panel.last_pixels[i] == 0x11);
    }
    assert(ops->acquire(g, f, NULL, 0, &again) == ESP_OK);
    assert(again.surface.pixels == pixels);
    memset(pixels, 0x22, lease.surface.capacity_bytes);
    for (size_t i = 0; i < 16; ++i) {
        assert(panel.last_pixels[i] == 0x11);
    }
    assert(ops->submit_region(g, f, again.id) == ESP_ERR_NO_MEM);
    assert(ops->cancel_region(g, f, again.id) == ESP_OK);
    esp_display_present_receipt_t receipt;
    assert(ops->end(g, f, &receipt) == ESP_OK);
    complete(&panel);
    esp_display_present_frame_status_t status;
    assert(ops->query(g, receipt, &status) == ESP_OK && status.terminal && status.result == ESP_OK);
    assert(ops->release_receipt(g, receipt) == ESP_OK);
    assert(ops->stop(g, 1000) == ESP_OK);
    assert(ops->destroy(g) == ESP_OK);
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.mutex);
}

int main(void)
{
    for (unsigned mode = 0; mode < 6; ++mode) {
        queued_region(mode);
    }
    queued_packing();
    queued_fifo();
    direct_async_holds_draw();
    draw_reused_while_tx_inflight();
    immediate_and_retry();
    delayed_and_stale();
    faults_and_uncertain();
    cancel_and_stop();
    threaded_completion_and_instances();
    invalid_configuration();
    multi_tile_frame();
    extended_contracts();
    retained_fault_and_packing();
    puts("PASS REGION: early/late callbacks, retry, stale events, bounded records, faults, cancel/stop, cross-instance ownership");
    return 0;
}
