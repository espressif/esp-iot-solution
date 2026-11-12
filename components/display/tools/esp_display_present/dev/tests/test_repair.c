/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "pixel/present_pixel_ops.h"
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "../../src/backend/present_te_composer.h"
#include "scan_panel.h"
#include "core/present_engine.h"
#include <pthread.h>
#include <stdio.h>
#define OK(x) assert((x)==ESP_OK)
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool block, entered;
    unsigned done;
    esp_err_t result;
} gate_t;
static esp_err_t execute(void *user, const esp_display_present_copy_request_t *r)
{
    gate_t *g = user;
    pthread_mutex_lock(&g->lock);
    g->entered = true;
    pthread_cond_broadcast(&g->changed);
    while (g->block) {
        pthread_cond_wait(&g->changed, &g->lock);
    }
    esp_err_t result = g->result;
    pthread_mutex_unlock(&g->lock);
    return result == ESP_OK ? esp_display_present_pixel_execute_cpu(r) : result;
}

static bool event(void *user, esp_display_present_copy_ticket_t t, const esp_display_present_copy_status_t *status, bool fault)
{
    (void)t;
    (void)status;
    (void)fault;
    gate_t *g = user;
    pthread_mutex_lock(&g->lock);
    g->done++;
    pthread_cond_broadcast(&g->changed);
    pthread_mutex_unlock(&g->lock);
    return false;
}

static bool progress(void *user, bool isr)
{
    assert(!isr);
    event(user, (esp_display_present_copy_ticket_t) {
        0
    }, NULL, false);
    return false;
}

static void wait_enter(gate_t *g)
{
    pthread_mutex_lock(&g->lock);
    while (!g->entered) {
        pthread_cond_wait(&g->changed, &g->lock);
    }
    pthread_mutex_unlock(&g->lock);
}

static void unblock(gate_t *g)
{
    pthread_mutex_lock(&g->lock);
    g->block = false;
    pthread_cond_broadcast(&g->changed);
    pthread_mutex_unlock(&g->lock);
}

static unsigned count(gate_t *g)
{
    pthread_mutex_lock(&g->lock);
    unsigned n = g->done;
    pthread_mutex_unlock(&g->lock);
    return n;
}

static void wait_done(gate_t *g, unsigned n)
{
    pthread_mutex_lock(&g->lock);
    while (g->done == n) {
        pthread_cond_wait(&g->changed, &g->lock);
    }
    pthread_mutex_unlock(&g->lock);
}

static void arm(gate_t *g)
{
    pthread_mutex_lock(&g->lock);
    g->block = true;
    g->entered = false;
    pthread_mutex_unlock(&g->lock);
}

static void te(bool cancel)
{
    gate_t g = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    present_te_composer_config_t c = {.width = 4, .height = 4, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .pixel = {.max_jobs = 1, .asynchronous = true, .execute = execute, .execute_user = &g, .on_event = event, .user = &g}};
    present_te_composer_t *b;
    OK(present_te_composer_create(&c, &b));
    present_te_frame_t f;
    uint16_t pixels[16];
    for (unsigned i = 0; i < 16; i++) {
        pixels[i] = i + 1;
    }
    esp_display_present_source_t src = {pixels, sizeof(pixels), {4, 4, 8, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}}, out;
    esp_display_present_rect_t full = {0, 0, 4, 4};
    OK(present_te_composer_begin(b, &f));
    OK(present_te_composer_write(b, f, &src, &full, 0, 0));
    OK(present_te_composer_seal(b, f));
    OK(present_te_composer_pin(b, f, &out));
    OK(present_te_composer_complete(b, f, ESP_OK));
    OK(present_te_composer_begin(b, &f));
    arm(&g);
    unsigned n = count(&g);
    assert(present_te_composer_seal(b, f) == ESP_ERR_TIMEOUT);
    wait_enter(&g);
    assert(present_te_composer_seal(b, f) == ESP_ERR_TIMEOUT);
    assert(present_te_composer_pin(b, f, &out) == ESP_ERR_INVALID_STATE);
    if (cancel) {
        assert(present_te_composer_cancel(b, f) == ESP_ERR_TIMEOUT);
        assert(present_te_composer_stop(b, 0) == ESP_ERR_TIMEOUT);
    }
    unblock(&g);
    wait_done(&g, n);
    if (cancel) {
        OK(present_te_composer_stop(b, UINT32_MAX));
        OK(present_te_composer_cancel(b, f));
    } else {
        for (;;) {
            n = count(&g);
            esp_err_t e = present_te_composer_seal(b, f);
            if (e == ESP_OK) {
                break;
            }
            assert(e == ESP_ERR_TIMEOUT);
            wait_done(&g, n);
        }
        OK(present_te_composer_pin(b, f, &out));
        assert(!memcmp(out.pixels, pixels, sizeof(pixels)));
        OK(present_te_composer_complete(b, f, ESP_OK));
        OK(present_te_composer_stop(b, UINT32_MAX));
    }
    OK(present_te_composer_destroy(b));
    pthread_cond_destroy(&g.changed);
    pthread_mutex_destroy(&g.lock);
}

static void fb(bool cancel)
{
    gate_t g = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    scan_panel_t panel = {0};
    const present_backend_ops_t *o = &present_fb_backend;
    esp_display_present_backend_config_t c = {.width = 4, .height = 4, .buffer_count = 2, .max_frames = 3, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT, .panel = scan_interface(&panel), .async_copy = true, .copy_execute = execute, .copy_user = &g, .on_progress = progress, .progress_user = &g};
    present_backend_t *b;
    OK(o->create(&c, &b));
    esp_display_present_frame_t f;
    esp_display_present_frame_request_t q = {0};
    esp_display_present_lease_t l;
    esp_display_present_receipt_t r;
    esp_display_present_frame_status_t status;
    OK(o->begin(b, &q, &f));
    OK(o->acquire(b, f, NULL, 0, &l));
    memset(l.surface.pixels, 0x11, 32);
    OK(o->submit_region(b, f, l.id));
    OK(o->end(b, f, &r));
    OK(present_fb_backend.service(b));
    scan_confirm(&panel);
    OK(o->wait(b, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status));
    OK(o->release_receipt(b, r));
    arm(&g);
    unsigned n = count(&g);
    OK(o->begin(b, &q, &f));
    wait_enter(&g);
    assert(o->acquire(b, f, NULL, 0, &l) == ESP_ERR_TIMEOUT);
    if (cancel) {
        assert(o->cancel_frame(b, f, 0) == ESP_ERR_TIMEOUT);
        assert(o->stop(b, 0) == ESP_ERR_TIMEOUT);
        /* Copy is still blocked; cancel_frame must not falsely succeed after stop(0) sets closing. */
        assert(o->cancel_frame(b, f, 0) == ESP_ERR_TIMEOUT);
        unblock(&g);
        wait_done(&g, n);
        OK(o->cancel_frame(b, f, 0));
        OK(o->stop(b, UINT32_MAX));
        OK(o->destroy(b));
        pthread_cond_destroy(&g.changed);
        pthread_mutex_destroy(&g.lock);
        return;
    }
    unblock(&g);
    wait_done(&g, n);
    OK(o->acquire(b, f, NULL, 0, &l));
    /* The prefill already holds the baseline. Only the submitted pixel changes;
     * unsubmitted prefilled pixels are not copied a second time. */
    ((uint16_t*)l.surface.pixels)[0] = 0x2222;
    esp_display_present_region_submit_t sub = {.logical_area = {0, 0, 1, 1}, .stride_bytes = 8};
    OK(o->submit_region_ex(b, f, l.id, &sub));
    unsigned copies = count(&g);
    OK(o->end(b, f, &r));
    OK(present_fb_backend.service(b));
    assert(count(&g) == copies);
    assert(panel.cookie);
    scan_confirm(&panel);
    OK(present_fb_backend.service(b));
    assert(((uint16_t*)panel.current.pixels)[0] == 0x2222);
    for (unsigned i = 1; i < 16; i++) {
        assert(((uint16_t*)panel.current.pixels)[i] == 0x1111);
    }
    OK(o->query(b, r, &status));
    assert(status.terminal);
    OK(o->release_receipt(b, r));
    OK(o->stop(b, UINT32_MAX));
    OK(o->destroy(b));
    pthread_cond_destroy(&g.changed);
    pthread_mutex_destroy(&g.lock);
}
/* Sealing a frame cannot release a source that the copy worker still reads.
 * Modes: success, accepted-copy failure, and stop timeout with retained access. */
static void fb_source_completion(unsigned mode)
{
    gate_t gate = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER,
                   .result = mode == 1 ? ESP_FAIL : ESP_OK
                  };
    scan_panel_t panel = {0};
    const present_backend_ops_t *ops = &present_fb_backend;
    esp_display_present_backend_config_t config = {
        .width = 4, .height = 4, .tile_rows = 4, .buffer_count = 2, .max_frames = 3,
        .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION,
        .panel = scan_interface(&panel), .async_copy = true,
        .copy_execute = execute, .copy_user = &gate, .on_progress = progress, .progress_user = &gate,
    };
    present_backend_t *backend;
    OK(ops->create(&config, &backend));
    esp_display_present_frame_t frame;
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(ops->begin(backend, &request, &frame));
    esp_display_present_lease_t lease;
    OK(ops->acquire(backend, frame, NULL, 0, &lease));
    memset(lease.surface.pixels, 0x5a, 32);
    arm(&gate);
    unsigned completed = count(&gate);
    OK(ops->submit_region(backend, frame, lease.id));
    wait_enter(&gate);
    esp_display_present_receipt_t receipt;
    OK(ops->end(backend, frame, &receipt));
    esp_display_present_frame_status_t status;
    OK(ops->query(backend, receipt, &status));
    assert(status.sealed && !status.src_released && !status.terminal && !status.present_confirmed);
    assert(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
    assert(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status) == ESP_ERR_TIMEOUT);
    assert(ops->release_receipt(backend, receipt) == ESP_ERR_INVALID_STATE);
    assert(panel.submissions == 0);
    if (mode == 2) {
        assert(ops->stop(backend, 0) == ESP_ERR_TIMEOUT);
        OK(ops->query(backend, receipt, &status));
        assert(!status.src_released && !status.terminal);
    }
    unblock(&gate);
    wait_done(&gate, completed);
    if (mode == 2) {
        OK(ops->stop(backend, UINT32_MAX));
    } else {
        OK(present_fb_backend.service(backend));
        if (mode == 0) {
            assert(panel.submissions == 1);
            OK(ops->query(backend, receipt, &status));
            assert(!status.src_released && !status.terminal);
            assert(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
            scan_confirm(&panel);
        } else {
            assert(panel.submissions == 0);
        }
    }
    OK(ops->wait(backend, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status));
    assert(status.terminal && status.src_released);
    assert(status.present_confirmed == (mode == 0));
    assert(status.result == (mode == 0 ? ESP_OK : mode == 1 ? ESP_FAIL : ESP_ERR_INVALID_STATE));
    if (mode != 0) {
        /* Source drain does not clear the accepted-operation sticky fault. */
        esp_display_present_backend_fault_t fault;
        OK(ops->get_fault(backend, &fault));
        assert(fault.result == status.result);
        assert(ops->begin(backend, &request, &frame) == ESP_ERR_INVALID_STATE);
    }
    OK(ops->release_receipt(backend, receipt));
    OK(ops->stop(backend, UINT32_MAX));
    OK(ops->destroy(backend));
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.lock);
}

static void presenter_source_completion(void)
{
    gate_t gate = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    scan_panel_t panel = {0};
    esp_display_present_engine_config_t config = {
        .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL,
        .backend = {
            .width = 4, .height = 4, .tile_rows = 4, .buffer_count = 2, .max_frames = 3,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = scan_interface(&panel), .async_copy = true,
            .copy_execute = execute, .copy_user = &gate, .on_progress = progress, .progress_user = &gate,
        },
    };
    esp_display_present_engine_t *presenter;
    OK(esp_display_present_engine_create(&config, &presenter));
    esp_display_present_frame_request_t request = {.full_redraw = true};
    OK(esp_display_present_engine_begin_next_frame(presenter, &request));
    esp_display_present_lease_t lease;
    OK(esp_display_present_engine_acquire_buffer(presenter, &lease));
    memset(lease.surface.pixels, 0x5a, 32);
    arm(&gate);
    unsigned completed = count(&gate);
    esp_display_present_engine_area_t area = {0, 0, 3, 3};
    OK(esp_display_present_engine_submit_buffer(presenter, &lease, &area, 8));
    wait_enter(&gate);
    esp_display_present_engine_receipt_t receipt;
    OK(esp_display_present_engine_commit_with_receipt(presenter, NULL, &receipt));
    esp_display_present_frame_status_t status;
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(!status.src_released && !status.terminal && !status.present_confirmed);
    assert(esp_display_present_engine_wait_frame(presenter, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_release_receipt(presenter, receipt) == ESP_ERR_INVALID_STATE);
    unblock(&gate);
    wait_done(&gate, completed);
    OK(esp_display_present_engine_service(presenter));
    assert(panel.cookie);
    OK(esp_display_present_engine_query_frame(presenter, receipt, &status));
    assert(!status.src_released && !status.terminal);
    scan_confirm(&panel);
    OK(esp_display_present_engine_service(presenter));
    OK(esp_display_present_engine_wait_frame(presenter, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status));
    assert(status.src_released && status.terminal && status.present_confirmed);
    OK(esp_display_present_engine_release_receipt(presenter, receipt));
    OK(esp_display_present_engine_stop(presenter, UINT32_MAX));
    OK(esp_display_present_engine_destroy(presenter));
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.lock);
}

int main(void)
{
    for (unsigned mode = 0; mode < 3; ++mode) {
        fb_source_completion(mode);
    }
    presenter_source_completion();
    te(false);
    te(true);
    fb(false);
    fb(true);
    /* Also cover stop(0) during post-seal async repair. */
    {
        gate_t g = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
        scan_panel_t panel = {0};
        const present_backend_ops_t *o = &present_fb_backend;
        esp_display_present_backend_config_t c = {.width = 4, .height = 4, .buffer_count = 2, .max_frames = 3, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT, .panel = scan_interface(&panel), .async_copy = true, .copy_execute = execute, .copy_user = &g, .on_progress = progress, .progress_user = &g};
        present_backend_t *b;
        OK(o->create(&c, &b));
        esp_display_present_frame_t f;
        esp_display_present_frame_request_t q = {0};
        esp_display_present_lease_t l;
        esp_display_present_receipt_t r;
        esp_display_present_frame_status_t status;
        OK(o->begin(b, &q, &f));
        OK(o->acquire(b, f, NULL, 0, &l));
        memset(l.surface.pixels, 0x11, 32);
        OK(o->submit_region(b, f, l.id));
        OK(o->end(b, f, &r));
        OK(present_fb_backend.service(b));
        scan_confirm(&panel);
        OK(o->wait(b, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status));
        OK(o->release_receipt(b, r));
        /* A full second publication makes the historical damage the whole frame.
         * The next frame seeds one corner, so repair still copies everything else
         * and stop must wait for that copy. */
        esp_display_present_frame_request_t redraw = {.full_redraw = true};
        OK(o->begin(b, &redraw, &f));
        OK(o->acquire(b, f, NULL, 0, &l));
        for (unsigned i = 0; i < 16; i++) {
            ((uint16_t *)l.surface.pixels)[i] = 0x1111;
        }
        ((uint16_t *)l.surface.pixels)[15] = 0x3333;
        OK(o->submit_region(b, f, l.id));
        OK(o->end(b, f, &r));
        OK(present_fb_backend.service(b));
        scan_confirm(&panel);
        OK(o->wait(b, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status));
        OK(o->release_receipt(b, r));
        esp_display_present_rect_t corner = {0, 0, 1, 1};
        esp_display_present_frame_request_t selective = {.damage = &corner, .damage_count = 1};
        OK(o->begin(b, &selective, &f));
        while (o->acquire(b, f, NULL, 0, &l) == ESP_ERR_TIMEOUT) {
            OK(present_fb_backend.service(b));
        }
        esp_display_present_region_submit_t sub = {.logical_area = {0, 0, 1, 1}, .stride_bytes = 8};
        OK(o->submit_region_ex(b, f, l.id, &sub));
        arm(&g);
        unsigned n = count(&g);
        OK(o->end(b, f, &r));
        wait_enter(&g);
        assert(o->stop(b, 0) == ESP_ERR_TIMEOUT);
        unblock(&g);
        wait_done(&g, n);
        OK(o->stop(b, UINT32_MAX));
        OK(o->query(b, r, &status));
        assert(status.terminal);
        OK(o->release_receipt(b, r));
        OK(o->destroy(b));
        pthread_cond_destroy(&g.changed);
        pthread_mutex_destroy(&g.lock);
    }
    puts("repair: gated TE spans, FB prefill/DIRECT, cancel and stop retention passed");
}
