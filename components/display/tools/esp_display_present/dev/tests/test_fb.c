/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "scan_panel.h"
#include "../../src/platform/present_os.h"
#include "core/present_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define OK(x) assert((x) == ESP_OK)
static const present_backend_ops_t *ops = &present_fb_backend;
static esp_display_present_backend_config_t config(scan_panel_t *p, size_t count, esp_display_present_render_contract_t contract)
{
    return (esp_display_present_backend_config_t) {
        .width = 4, .height = 4, .tile_rows = 2,
        .buffer_count = count, .max_frames = 8, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
        .render_contract = contract, .panel = scan_interface(p), .active_writes = count == 1
    };
}

static esp_display_present_frame_t begin(present_backend_t *b)
{
    esp_display_present_frame_t frame;
    esp_display_present_frame_request_t request = {0};
    OK(ops->begin(b, &request, &frame));
    return frame;
}

static void write_rect(present_backend_t *b, esp_display_present_frame_t f, esp_display_present_rect_t area, uint16_t color)
{
    esp_display_present_backend_caps_t caps;
    OK(ops->get_caps(b, &caps));
    for (uint32_t y = 0; y < area.height;) {
        esp_display_present_lease_t lease;
        OK(ops->acquire(b, f, NULL, 0, &lease));
        uint32_t rows = caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && area.height - y > 2 ? 2 : area.height - y;
        size_t stride = caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? area.width * 2 : lease.surface.layout.stride_bytes;
        for (uint32_t row = 0; row < rows; row++) {
            for (uint32_t x = 0; x < area.width; x++) {
                size_t offset = caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? row * stride + x * 2 :
                                (area.y + y + row) * stride + (area.x + x) * 2;
                memcpy((uint8_t *)lease.surface.pixels + offset, &color, 2);
            }
        }
        esp_display_present_region_submit_t sub = {{area.x, area.y + y, area.width, rows}, stride};
        OK(ops->submit_region_ex(b, f, lease.id, &sub));
        y += rows;
    }
}

static esp_display_present_receipt_t full(present_backend_t *b, uint16_t color)
{
    esp_display_present_frame_t f = begin(b);
    write_rect(b, f, (esp_display_present_rect_t) {
        0, 0, 4, 4
    }, color);
    esp_display_present_receipt_t receipt;
    OK(ops->end(b, f, &receipt));
    OK(present_fb_backend.service(b));
    return receipt;
}

static void finish(present_backend_t *b, scan_panel_t *p, esp_display_present_receipt_t r)
{
    scan_confirm(p);
    esp_display_present_frame_status_t status;
    OK(ops->wait(b, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status));
    assert(status.present_confirmed && status.terminal && status.result == ESP_OK);
    OK(ops->release_receipt(b, r));
}

static void grid(void)
{
    for (size_t count = 1; count <= 3; count++) {
        for (int mode = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION; mode <= ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT; mode++) {
            scan_panel_t panel = {0};
            esp_display_present_backend_config_t cfg = config(&panel, count, mode);
            present_backend_t *b;
            OK(ops->create(&cfg, &b));
            esp_display_present_backend_caps_t caps;
            OK(ops->get_caps(b, &caps));
            assert(caps.writes_active_scanout == (count == 1));
            esp_display_present_receipt_t first = full(b, 0x1234);
            esp_display_present_backend_event_t false_done = {.kind = ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE, .transaction_cookie = panel.cookie};
            ops->on_event(b, &false_done);
            esp_display_present_frame_status_t status;
            assert(ops->wait(b, first, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
            finish(b, &panel, first);
            void *scanning = panel.current.pixels;
            esp_display_present_frame_t f = begin(b);
            esp_display_present_lease_t lease;
            OK(ops->acquire(b, f, NULL, 0, &lease));
            if (mode != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
                assert((lease.surface.pixels == scanning) == (count == 1));
            }
            OK(ops->cancel_region(b, f, lease.id));
            if (mode == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL)
                write_rect(b, f, (esp_display_present_rect_t) {
                0, 0, 4, 4
            }, 0xabcd);
            else
                write_rect(b, f, (esp_display_present_rect_t) {
                1, 1, 2, 2
            }, 0xabcd);
            if (count > 1) {
                uint16_t value;
                memcpy(&value, scanning, 2);
                assert(value == 0x1234); /* Active scan source was never drawn into. */
            }
            esp_display_present_receipt_t second;
            OK(ops->end(b, f, &second));
            OK(present_fb_backend.service(b));
            finish(b, &panel, second);
            for (unsigned y = 0; y < 4; y++) {
                for (unsigned x = 0; x < 4; x++) {
                    uint16_t value;
                    memcpy(&value, (const uint8_t *)panel.current.pixels + (y * 4 + x) * 2, 2);
                    uint16_t expected = mode == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL || (x >= 1 && x < 3 && y >= 1 && y < 3) ? 0xabcd : 0x1234;
                    assert(value == expected);
                }
            }
            panel.stop_timeout = true;
            assert(ops->stop(b, 0) == ESP_ERR_TIMEOUT); /* Even a terminal receipt leaves live scanout. */
            assert(ops->destroy(b) == ESP_ERR_INVALID_STATE);
            panel.stop_timeout = false;
            OK(ops->stop(b, 0));
            OK(ops->destroy(b));
        }
    }
}

static void pipeline(void)
{
    scan_panel_t p = {0};
    esp_display_present_backend_config_t cfg = config(&p, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    /* Frame 1: becomes active SCANNING buffer (Slot 0). */
    esp_display_present_receipt_t first = full(b, 0x1111);
    finish(b, &p, first);
    void *scanning_fb = p.current.pixels;

    /* Frame 2: sealed and submitted to panel -> PENDING switch (Slot 1).
     * While Slot 0 is SCANNING and Slot 1 is PENDING switch confirmation,
     * begin Frame 3 on Slot 2 and verify all 3 buffers are simultaneously
     * in distinct pipeline stages (SCANNING + PENDING + DRAWING). */
    esp_display_present_receipt_t second = full(b, 0x2222);
    void *pending_fb = p.pending.pixels;
    assert(pending_fb && pending_fb != scanning_fb);

    esp_display_present_frame_t third = begin(b);
    esp_display_present_lease_t third_lease;
    OK(ops->acquire(b, third, NULL, 0, &third_lease));
    void *drawing_fb = third_lease.surface.pixels;
    assert(drawing_fb && drawing_fb != scanning_fb && drawing_fb != pending_fb);
    OK(ops->cancel_region(b, third, third_lease.id));

    write_rect(b, third, (esp_display_present_rect_t) {
        0, 0, 1, 1
    }, 0x3333);
    /* Active scanout (0x1111) and pending switch (0x2222) are untouched while drawing Frame 3. */
    uint16_t check_scan = 0, check_pend = 0;
    memcpy(&check_scan, scanning_fb, 2);
    memcpy(&check_pend, pending_fb, 2);
    assert(check_scan == 0x1111 && check_pend == 0x2222);

    esp_display_present_receipt_t receipt;
    OK(ops->end(b, third, &receipt));

    /* All 3 buffers are now occupied (SCANNING + PENDING + READY): 4th frame hits backpressure. */
    esp_display_present_frame_request_t request = {0};
    esp_display_present_frame_t blocked;
    assert(ops->begin(b, &request, &blocked) == ESP_ERR_NO_MEM);
    assert(p.submissions == 2 && p.current.pixels == scanning_fb);

    /* Confirming Frame 2 retires Slot 0 (SCANNING -> FREE) and promotes Slot 1 to SCANNING;
     * service() then submits queued Frame 3 (Slot 2: READY -> PENDING). */
    finish(b, &p, second);
    assert(p.submissions == 3 && p.current.pixels == pending_fb);

    /* Because Slot 0 is now FREE, a 4th frame can immediately begin & draw while
     * Slot 1 is SCANNING and Slot 2 is PENDING! */
    esp_display_present_frame_t fourth = begin(b);
    esp_display_present_lease_t fourth_lease;
    OK(ops->acquire(b, fourth, NULL, 0, &fourth_lease));
    assert(fourth_lease.surface.pixels == scanning_fb); /* Reuses retired Slot 0! */
    OK(ops->cancel_region(b, fourth, fourth_lease.id));
    OK(ops->cancel_frame(b, fourth, 0));

    finish(b, &p, receipt);
    uint16_t value;
    memcpy(&value, p.current.pixels, 2);
    assert(value == 0x3333);
    memcpy(&value, (uint8_t *)p.current.pixels + 2, 2);
    assert(value == 0x2222); /* Repair used latest sealed frame, not stale scanout. */
    third = begin(b);
    write_rect(b, third, (esp_display_present_rect_t) {
        0, 0, 1, 1
    }, 0xeeee);
    OK(ops->cancel_frame(b, third, 0));
    OK(ops->cancel_frame(b, third, 0));
    memcpy(&value, p.current.pixels, 2);
    assert(value == 0x3333);
    OK(ops->stop(b, 0));
    OK(ops->destroy(b));
}

static void errors_and_external(void)
{
    for (unsigned mode = 0; mode < 3; mode++) {
        scan_panel_t p = {.uncertain = mode == 0, .immediate = mode == 1, .early_reject = mode == 1};
        esp_display_present_backend_config_t cfg = config(&p, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
        present_backend_t *b;
        OK(ops->create(&cfg, &b));
        esp_display_present_receipt_t r = full(b, 7);
        if (mode == 2) {
            esp_display_present_backend_event_t fault_event = {.kind = ESP_DISPLAY_PRESENT_EVENT_FAULT, .transaction_cookie = p.cookie, .result = ESP_FAIL};
            ops->on_event(b, &fault_event);
        }
        p.stop_timeout = true;
        assert(ops->stop(b, 0) == ESP_ERR_TIMEOUT);
        p.stop_timeout = false;
        OK(ops->stop(b, 0));
        esp_display_present_frame_status_t status;
        OK(ops->query(b, r, &status));
        assert(status.terminal && status.result != ESP_OK && !status.present_confirmed);
        OK(ops->release_receipt(b, r));
        OK(ops->destroy(b));
    }
    scan_panel_t p = {.reject = true};
    esp_display_present_backend_config_t cfg = config(&p, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
    void *external[2] = {aligned_alloc(64, 64), aligned_alloc(64, 64)};
    assert(external[0] && external[1]);
    cfg.ext_bufs = external;
    cfg.ext_bytes = 64;
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_t f = begin(b);
    write_rect(b, f, (esp_display_present_rect_t) {
        0, 0, 4, 4
    }, 8);
    esp_display_present_receipt_t r;
    OK(ops->end(b, f, &r));
    assert(present_fb_backend.service(b) == ESP_FAIL);
    p.reject = false;
    OK(present_fb_backend.service(b));
    finish(b, &p, r);
    OK(ops->stop(b, 0));
    OK(ops->destroy(b));
    memset(external[0], 0xaa, 64); /* Still owned by caller. */
    free(external[0]);
    free(external[1]);
}

static void named_modes(void)
{
    const esp_display_present_present_mode_t modes[] = {ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL,
                                                        ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_ENGINE_MODE_AUTO
                                                       };
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); i++) {
        scan_panel_t panel = {0};
        esp_display_present_engine_config_t cfg = {.backend = config(&panel, 3, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION),
                                                   .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = modes[i]
                                                  };
        cfg.backend.active_writes = true;
        esp_display_present_engine_t *p;
        OK(esp_display_present_engine_create(&cfg, &p));
        esp_display_present_backend_caps_t caps;
        OK(esp_display_present_engine_get_caps(p, &caps));
        assert(caps.supports_present_confirmation);
        esp_display_present_render_contract_t expected = modes[i] == ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT ? ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT :
                                                         (modes[i] == ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL || modes[i] == ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL ? ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL : ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
        assert(caps.contract == expected);
        assert(caps.writes_active_scanout == (modes[i] == ESP_DISPLAY_PRESENT_ENGINE_MODE_NONE));
        OK(esp_display_present_engine_stop(p, 0));
        OK(esp_display_present_engine_destroy(p));
    }
}
typedef struct {
    present_backend_t *backend;
    uint64_t cookie;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool start;
} concurrent_event_t;
static void *confirm_thread(void *context)
{
    concurrent_event_t *event = context;
    pthread_mutex_lock(&event->mutex);
    while (!event->start) {
        pthread_cond_wait(&event->condition, &event->mutex);
    }
    pthread_mutex_unlock(&event->mutex);
    esp_display_present_backend_event_t completion = {.kind = ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED, .transaction_cookie = event->cookie + 1};
    for (unsigned i = 0; i < 50; i++) {
        (void)ops->on_event_isr(event->backend, &completion);
    }
    completion.transaction_cookie = event->cookie;
    (void)ops->on_event_isr(event->backend, &completion);
    return NULL;
}

static void concurrent_confirmation(void)
{
    scan_panel_t panel = {0};
    esp_display_present_backend_config_t cfg = config(&panel, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_receipt_t r = full(b, 0xbeef);
    concurrent_event_t event = {.backend = b, .cookie = panel.cookie};
    assert(pthread_mutex_init(&event.mutex, NULL) == 0);
    assert(pthread_cond_init(&event.condition, NULL) == 0);
    /* Hardware activation/retirement has happened; deliver only its event on
     * another thread. Panel model remains owned by the test's producer task. */
    panel.current = panel.pending;
    panel.pending = (esp_display_present_surface_t) {
        0
    };
    panel.cookie = 0;
    pthread_t thread;
    assert(pthread_create(&thread, NULL, confirm_thread, &event) == 0);
    pthread_mutex_lock(&event.mutex);
    event.start = true;
    pthread_cond_broadcast(&event.condition);
    pthread_mutex_unlock(&event.mutex);
    esp_display_present_frame_status_t status;
    OK(ops->wait(b, r, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 1000, &status));
    assert(status.present_confirmed);
    assert(pthread_join(thread, NULL) == 0);
    OK(ops->release_receipt(b, r));
    OK(ops->stop(b, 0));
    OK(ops->destroy(b));
    pthread_cond_destroy(&event.condition);
    pthread_mutex_destroy(&event.mutex);
}

static void validation(void)
{
    scan_panel_t p = {0};
    present_backend_t *b;
    esp_display_present_backend_config_t cfg = config(&p, 1, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
    cfg.active_writes = false;
    assert(ops->create(&cfg, &b) == ESP_ERR_INVALID_ARG);
    void *memory = aligned_alloc(64, 128);
    assert(memory);
    void *buffers[2] = {memory, memory};
    cfg = config(&p, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
    cfg.ext_bufs = buffers;
    cfg.ext_bytes = 64;
    assert(ops->create(&cfg, &b) == ESP_ERR_INVALID_ARG);
    buffers[1] = (uint8_t *)memory + 1;
    assert(ops->create(&cfg, &b) == ESP_ERR_INVALID_ARG);
    buffers[1] = (uint8_t *)memory + 64;
    esp_display_present_engine_config_t pc = {.backend = cfg, .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_TRIPLE_FULL};
    esp_display_present_engine_t *presenter;
    assert(esp_display_present_engine_create(&pc, &presenter) == ESP_ERR_INVALID_ARG);
    free(memory);
    cfg = config(&p, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_t f = begin(b);
    write_rect(b, f, (esp_display_present_rect_t) {
        0, 0, 1, 1
    }, 0x1111);
    esp_display_present_frame_coverage_t claimed = {.full_coverage = true};
    assert(ops->end_ex(b, f, &claimed, NULL) == ESP_ERR_INVALID_STATE);
    OK(ops->cancel_frame(b, f, 0));
    OK(ops->stop(b, 0));
    OK(ops->destroy(b));
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

static void asynchronous_partition(unsigned mode)
{
    copy_gate_t g = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .fail = mode == 3};
    scan_panel_t panel = {0};
    esp_display_present_backend_config_t cfg = config(&panel, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
    cfg.tile_rows = 4;
    cfg.async_copy = true;
    cfg.copy_execute = gated_copy;
    cfg.copy_user = &g;
    cfg.on_progress = copied;
    cfg.progress_user = &g;
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_t f = begin(b);
    esp_display_present_lease_t lease;
    OK(ops->acquire(b, f, NULL, 0, &lease));
    memset(lease.surface.pixels, 0x5a, 32);
    esp_display_present_region_submit_t sub = {.logical_area = {0, 0, 4, mode == 4 ? 2 : 4}, .stride_bytes = 8};
    OK(ops->submit_region_ex(b, f, lease.id, &sub));
    pthread_mutex_lock(&g.mutex);
    while (!g.entered) {
        pthread_cond_wait(&g.changed, &g.mutex);
    }
    pthread_mutex_unlock(&g.mutex);
    assert(ops->acquire(b, f, NULL, 0, &lease) == ESP_ERR_TIMEOUT);
    esp_display_present_receipt_t receipt = {0};
    if (mode == 1) {
        assert(ops->cancel_frame(b, f, 0) == ESP_ERR_TIMEOUT);
        assert(ops->acquire(b, f, NULL, 0, &lease) == ESP_ERR_INVALID_STATE);
    } else {
        OK(ops->end(b, f, mode == 5 ? NULL : &receipt));
        esp_display_present_frame_t next;
        esp_display_present_frame_request_t req = {0};
        assert(ops->begin(b, &req, &next) == ESP_ERR_NO_MEM);
        OK(present_fb_backend.service(b));
        assert(panel.submissions == 0);
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
        OK(present_fb_backend.service(b));
        if (mode == 1) {
            OK(ops->cancel_frame(b, f, 0));
        }
        if (mode == 0 || mode == 5) {
            assert(panel.cookie);
            scan_confirm(&panel);
            OK(present_fb_backend.service(b));
            for (size_t i = 0; i < 32; ++i) {
                assert(panel.pixels[i] == 0x5a);
            }
        } else {
            assert(panel.submissions == 0);
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

static void independent_tiles(void)
{
    scan_panel_t panel = {0};
    esp_display_present_backend_config_t cfg = config(&panel, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
    cfg.tile_rows = 2;
    cfg.tile_count = 2;
    present_backend_t *b;
    OK(ops->create(&cfg, &b));
    esp_display_present_frame_t f = begin(b);
    esp_display_present_lease_t first, second, blocked;
    OK(ops->acquire(b, f, NULL, 0, &first));
    OK(ops->acquire(b, f, NULL, 0, &second));
    assert(first.surface.pixels != second.surface.pixels);
    assert(ops->acquire(b, f, NULL, 0, &blocked) == ESP_ERR_TIMEOUT);
    assert(ops->end(b, f, NULL) == ESP_ERR_INVALID_STATE);
    memset(first.surface.pixels, 0x11, 16);
    memset(second.surface.pixels, 0x22, 16);
    esp_display_present_region_submit_t a = {.logical_area = {0, 0, 4, 2}}, z = {.logical_area = {0, 2, 4, 2}};
    OK(ops->submit_region_ex(b, f, first.id, &a));
    OK(ops->submit_region_ex(b, f, second.id, &z));
    esp_display_present_receipt_t receipt;
    OK(ops->end(b, f, &receipt));
    OK(present_fb_backend.service(b));
    scan_confirm(&panel);
    OK(present_fb_backend.service(b));
    for (unsigned i = 0; i < 16; i++) {
        assert(((uint16_t*)panel.current.pixels)[i] == (i < 8 ? 0x1111 : 0x2222));
    }
    OK(ops->release_receipt(b, receipt));
    OK(ops->stop(b, 0));
    OK(ops->destroy(b));
}

static void async_pipeline_handoff(size_t buffers, esp_display_present_render_contract_t contract)
{
    copy_gate_t g = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .release = true};
    scan_panel_t p = {0};
    void *ext[3] = {aligned_alloc(64, 64), aligned_alloc(64, 64), aligned_alloc(64, 64)};
    assert(ext[0] && ext[1] && ext[2]);
    esp_display_present_backend_config_t cfg = config(&p, buffers, contract);
    cfg.tile_rows = 2;
    cfg.tile_count = contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? 2 : 0;
    cfg.rotation = contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ? ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 : ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0;
    cfg.async_copy = true;
    cfg.on_progress = copied;
    cfg.progress_user = &g;
    cfg.ext_bufs = ext;
    cfg.ext_bytes = 64;
    present_backend_t *b;
    OK(ops->create(&cfg, &b));

    /* Helper to pump async copies to READY/PENDING. */
    esp_display_present_receipt_t r1, r2, r3;
    for (unsigned step = 1; step <= buffers; ++step) {
        esp_display_present_frame_t f = begin(b);
        uint16_t color = (uint16_t)(0x1111 * step);
        if (contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
            if (step == 1) {
                for (uint32_t y = 0; y < 4; y += 2) {
                    esp_display_present_lease_t l;
                    esp_display_present_rect_t band = {0, y, 4, 2};
                    while (ops->acquire(b, f, &band, 0, &l) == ESP_ERR_TIMEOUT) {
                        OK(present_fb_backend.service(b));
                        present_pause();
                    }
                    for (size_t i = 0; i < 8; ++i) {
                        ((uint16_t *)l.surface.pixels)[i] = color;
                    }
                    OK(ops->submit_region_ex(b, f, l.id, &(esp_display_present_region_submit_t) {
                        .logical_area = band
                    }));
                }
            } else {
                esp_display_present_lease_t l;
                esp_display_present_rect_t spot = {step - 2, 0, 1, 1};
                while (ops->acquire(b, f, &spot, 0, &l) == ESP_ERR_TIMEOUT) {
                    OK(present_fb_backend.service(b));
                    present_pause();
                }
                ((uint16_t *)l.surface.pixels)[0] = color;
                OK(ops->submit_region_ex(b, f, l.id, &(esp_display_present_region_submit_t) {
                    .logical_area = spot
                }));
            }
        } else {
            esp_display_present_lease_t l;
            while (ops->acquire(b, f, NULL, 0, &l) == ESP_ERR_TIMEOUT) {
                OK(present_fb_backend.service(b));
                present_pause();
            }
            if (step == 1 || contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL) {
                for (size_t i = 0; i < 16; ++i) {
                    ((uint16_t *)l.surface.pixels)[i] = color;
                }
                OK(ops->submit_region(b, f, l.id));
            } else {
                ((uint16_t *)l.surface.pixels)[step - 2] = color;
                esp_display_present_region_submit_t sub = {.logical_area = {step - 2, 0, 1, 1}, .stride_bytes = 8};
                OK(ops->submit_region_ex(b, f, l.id, &sub));
            }
        }
        esp_display_present_receipt_t *target = step == 1 ? &r1 : step == 2 ? &r2 : &r3;
        OK(ops->end(b, f, target));
        /* Pump until async tile/prefill/convert copies finish and the frame reaches PENDING or READY.
         * A full DIRECT prefill is already the baseline, so it does not start a second repair copy. */
        size_t settled = SIZE_MAX;
        for (unsigned spins = 0;; ++spins) {
            assert(spins < 10000);
            OK(present_fb_backend.service(b));
            if (step <= 2 && p.cookie) {
                break;
            }
            if (step == 3) {
                esp_display_present_frame_request_t probe_req = {0};
                esp_display_present_frame_t probe;
                assert(ops->begin(b, &probe_req, &probe) == ESP_ERR_NO_MEM);
                pthread_mutex_lock(&g.mutex);
                size_t done = g.completed;
                pthread_mutex_unlock(&g.mutex);
                if (done == settled) {
                    break;
                }
                settled = done;
            }
            present_pause();
        }
        if (step == 1) {
            assert(p.cookie);
            scan_confirm(&p);
            OK(present_fb_backend.service(b));
            esp_display_present_frame_status_t st;
            OK(ops->query(b, r1, &st));
            assert(st.terminal && st.present_confirmed);
            OK(ops->release_receipt(b, r1));
        }
    }

    /* At this point:
     * - ext[0] is SCANNING (Frame 1)
     * - ext[1] is PENDING switch (Frame 2)
     * - If buffers == 3, ext[2] is READY queued behind ext[1] (Frame 3).
     * Next begin() must hit backpressure (ESP_ERR_NO_MEM). */
    assert(p.current.pixels == ext[0] && p.pending.pixels == ext[1]);
    esp_display_present_frame_request_t req = {0};
    esp_display_present_frame_t blocked;
    assert(ops->begin(b, &req, &blocked) == ESP_ERR_NO_MEM);

    /* Confirm Frame 2: ext[0] retires, ext[1] becomes SCANNING; if buffers==3, ext[2] submits to PENDING once its async repair/convert finishes! */
    scan_confirm(&p);
    if (buffers == 3) {
        while (!p.cookie) {
            OK(present_fb_backend.service(b));
            present_pause();
        }
    } else {
        OK(present_fb_backend.service(b));
    }
    assert(p.current.pixels == ext[1]);
    esp_display_present_frame_status_t st;
    OK(ops->query(b, r2, &st));
    assert(st.terminal && st.present_confirmed);
    OK(ops->release_receipt(b, r2));

    if (buffers == 3) {
        assert(p.cookie && p.pending.pixels == ext[2]);
        /* While ext[1] is SCANNING and ext[2] is PENDING, retired ext[0] can begin & cancel mid-copy! */
        esp_display_present_frame_t f4 = begin(b);
        esp_display_present_lease_t l4;
        while (ops->acquire(b, f4, NULL, 0, &l4) == ESP_ERR_TIMEOUT) {
            OK(present_fb_backend.service(b));
        }
        OK(ops->cancel_region(b, f4, l4.id));
        OK(ops->cancel_frame(b, f4, 1000));

        scan_confirm(&p);
        OK(present_fb_backend.service(b));
        assert(p.current.pixels == ext[2]);
        OK(ops->query(b, r3, &st));
        assert(st.terminal && st.present_confirmed);
        OK(ops->release_receipt(b, r3));
        if (contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL) {
            const uint16_t *px = (const uint16_t *)p.current.pixels;
            assert(px[0] == 0x2222 && px[1] == 0x3333 && px[2] == 0x1111);
        }
    }

    OK(ops->stop(b, 1000));
    OK(ops->destroy(b));
    for (size_t i = 0; i < 3; ++i) {
        free(ext[i]);
    }
    pthread_cond_destroy(&g.changed);
    pthread_mutex_destroy(&g.mutex);
}

/* A promised full DIRECT redraw needs no previous-frame copy. Actual writes
 * still have to cover every pixel before the frame can be submitted. */
static void direct_full_redraw_skips_prefill(void)
{
    for (unsigned asynchronous = 0; asynchronous < 2; ++asynchronous) {
        scan_panel_t panel = {0};
        void *buffers[2] = {aligned_alloc(64, 64), aligned_alloc(64, 64)};
        assert(buffers[0] && buffers[1]);
        memset(buffers[0], 0xa5, 64);
        memset(buffers[1], 0x7b, 64);
        esp_display_present_backend_config_t cfg = config(&panel, 2, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
        cfg.ext_bufs = buffers;
        cfg.ext_bytes = 64;
        cfg.async_copy = asynchronous;
        present_backend_t *backend;
        OK(ops->create(&cfg, &backend));

        esp_display_present_receipt_t baseline = full(backend, 0x1111);
        finish(backend, &panel, baseline);

        esp_display_present_frame_request_t redraw = {.full_redraw = true};
        esp_display_present_frame_t frame;
        OK(ops->begin(backend, &redraw, &frame));
        esp_display_present_lease_t lease;
        /* This must not wait on an unnecessary baseline copy. */
        OK(ops->acquire(backend, frame, NULL, 0, &lease));
        assert(lease.surface.pixels == buffers[1]);
        for (unsigned i = 0; i < 16; ++i) {
            assert(((uint16_t *)lease.surface.pixels)[i] == 0x7b7b);
        }
        OK(ops->cancel_region(backend, frame, lease.id));
        write_rect(backend, frame, (esp_display_present_rect_t) {
            0, 0, 1, 1
        }, 0x3333);
        esp_display_present_receipt_t next = {0};
        assert(ops->end(backend, frame, &next) == ESP_ERR_INVALID_STATE);
        assert(!next.value && panel.submissions == 1);
        /* Rejected seal keeps the frame alive, so the producer can finish it. */
        write_rect(backend, frame, (esp_display_present_rect_t) {
            0, 0, 4, 4
        }, 0x2222);
        OK(ops->end(backend, frame, &next));
        OK(present_fb_backend.service(backend));
        finish(backend, &panel, next);
        for (unsigned i = 0; i < 16; ++i) {
            assert(((uint16_t *)panel.current.pixels)[i] == 0x2222);
        }

        /* A partial DIRECT request must still seed the new surface from the
         * prior image before handing it to a read-modify-write renderer. */
        esp_display_present_frame_request_t partial = {0};
        OK(ops->begin(backend, &partial, &frame));
        unsigned attempts = 0;
        esp_err_t error;
        while ((error = ops->acquire(backend, frame, NULL, 0, &lease)) == ESP_ERR_TIMEOUT) {
            assert(++attempts < 10000);
            OK(present_fb_backend.service(backend));
            present_pause();
        }
        OK(error);
        assert(lease.surface.pixels == buffers[0]);
        for (unsigned i = 0; i < 16; ++i) {
            assert(((uint16_t *)lease.surface.pixels)[i] == 0x2222);
        }
        ((uint16_t *)lease.surface.pixels)[0] = 0x4444;
        esp_display_present_region_submit_t update = {.logical_area = {0, 0, 1, 1}, .stride_bytes = 8};
        OK(ops->submit_region_ex(backend, frame, lease.id, &update));
        OK(ops->end(backend, frame, &next));
        attempts = 0;
        while (!panel.cookie) {
            assert(++attempts < 10000);
            OK(present_fb_backend.service(backend));
            present_pause();
        }
        finish(backend, &panel, next);
        assert(((uint16_t *)panel.current.pixels)[0] == 0x4444);
        for (unsigned i = 1; i < 16; ++i) {
            assert(((uint16_t *)panel.current.pixels)[i] == 0x2222);
        }
        OK(ops->stop(backend, UINT32_MAX));
        OK(ops->destroy(backend));
        free(buffers[0]);
        free(buffers[1]);
    }
}

int main(void)
{
    direct_full_redraw_skips_prefill();
    independent_tiles();
    for (size_t buffers = 2; buffers <= 3; ++buffers) {
        async_pipeline_handoff(buffers, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION);
        async_pipeline_handoff(buffers, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL);
        async_pipeline_handoff(buffers, ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT);
    }
    for (unsigned mode = 0; mode < 6; ++mode) {
        asynchronous_partition(mode);
    }
    grid();
    pipeline();
    errors_and_external();
    named_modes();
    concurrent_confirmation();
    validation();
    puts("FB contracts: single/double/triple, scanout retention, queued baseline and faults passed");
    return 0;
}
