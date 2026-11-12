/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "test_panel.h"
#include "scan_panel.h"
#include <pthread.h>
#include <stdio.h>

static esp_display_present_engine_t *make(test_panel_t *panel, size_t frames)
{
    esp_display_present_engine_config_t config = { .backend = {
            .width = panel->width, .height = panel->height, .tile_rows = 2,
            .buffer_count = 2, .max_frames = frames, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
            .panel = test_panel_interface(panel)
        }
    };
    esp_display_present_engine_t *p;
    assert(esp_display_present_engine_create(&config, &p) == ESP_OK);
    return p;
}

static void begin(esp_display_present_engine_t *p)
{
    const esp_display_present_frame_request_t request = {0};
    assert(esp_display_present_engine_begin_next_frame(p, &request) == ESP_OK);
}

static void send(esp_display_present_engine_t *p)
{
    esp_display_present_lease_t lease;
    assert(esp_display_present_engine_acquire_buffer(p, &lease) == ESP_OK);
    memset(lease.surface.pixels, 0x5A, lease.surface.capacity_bytes);
    esp_display_present_engine_area_t area = {0, 0, 3, 1};
    assert(esp_display_present_engine_submit_buffer(p, &lease, &area, 8) == ESP_OK);
}

static void *foreign_task(void *user)
{
    esp_display_present_engine_status_t status;
    assert(esp_display_present_engine_get_status(user, &status) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_stop(user, 0) == ESP_ERR_INVALID_STATE);
    return NULL;
}

static void leases_and_ownership(void)
{
    test_panel_t a = {.width = 4, .height = 4, .bpp = 2, .immediate = true};
    test_panel_t b = {.width = 4, .height = 4, .bpp = 2, .immediate = true};
    esp_display_present_engine_t *p = make(&a, 2), *other = make(&b, 2);
    esp_display_present_lease_t lease, foreign, unused;
    assert(esp_display_present_engine_acquire_buffer(p, &lease) == ESP_ERR_INVALID_STATE);
    begin(p);
    begin(other);
    assert(esp_display_present_engine_acquire_buffer(p, &lease) == ESP_OK);
    assert(esp_display_present_engine_acquire_buffer(other, &foreign) == ESP_OK);
    assert(esp_display_present_engine_acquire_buffer(p, &unused) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_ERR_INVALID_STATE);
    uint32_t rows;
    assert(esp_display_present_engine_resolve_rows(p, &lease, 8, 9, &rows) == ESP_OK && rows == 2);
    assert(esp_display_present_engine_resolve_rows(p, &lease, 7, 2, &rows) == ESP_ERR_INVALID_SIZE);
    esp_display_present_engine_area_t area = {1, 1, 2, 2};
    assert(esp_display_present_engine_submit_buffer(p, &foreign, &area, 4) == ESP_ERR_INVALID_STATE);
    esp_display_present_engine_area_t bad = {-1, 0, 2, 2};
    assert(esp_display_present_engine_submit_buffer(p, &lease, &bad, 4) == ESP_ERR_INVALID_ARG);
    memset(lease.surface.pixels, 0xA3, 8);
    a.reject = true;
    assert(esp_display_present_engine_submit_buffer(p, &lease, &area, 4) == ESP_FAIL);
    a.reject = false;
    assert(esp_display_present_engine_submit_buffer(p, &lease, &area, 4) == ESP_OK);
    assert(esp_display_present_engine_submit_buffer(p, &lease, &area, 4) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    assert(a.pixels[(1 * 4 + 1) * 2] == 0xA3 && a.pixels[0] == 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, foreign_task, p) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(esp_display_present_engine_cancel_frame(other) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_stop(other, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
    assert(esp_display_present_engine_destroy(other) == ESP_OK);
}

static void receipts_cancel_and_stop(void)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_engine_t *p = make(&panel, 1);
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    const esp_display_present_frame_request_t request = {0};
    assert(esp_display_present_engine_begin_next_frame(p, &request) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_drain(p, 0) == ESP_ERR_TIMEOUT);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    /* drain is not stop: the next frame is still accepted. */
    begin(p);
    send(p);
    assert(esp_display_present_engine_cancel_frame(p) == ESP_ERR_TIMEOUT);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_cancel_frame(p) == ESP_OK);
    assert(esp_display_present_engine_cancel_frame(p) == ESP_OK);
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    esp_display_present_engine_status_t status;
    assert(esp_display_present_engine_get_status(p, &status) == ESP_OK);
    assert(status.state == ESP_DISPLAY_PRESENT_ENGINE_STOPPED && status.pending_frames == 0 &&
           status.committed_frames == 2 && status.completed_frames == 2);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
}

static void async_error_and_recycle(void)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2, .immediate = true};
    esp_display_present_engine_t *p = make(&panel, 1);
    for (int i = 0; i < 20; ++i) {
        begin(p);
        send(p);
        assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    }
    panel.immediate = false;
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    esp_display_present_backend_event_t event = {.kind = ESP_DISPLAY_PRESENT_EVENT_FAULT,
                                                 .transaction_cookie = panel.pending[0].cookie, .result = ESP_FAIL
                                                };
    panel.callback(panel.user, &event, false);
    assert(esp_display_present_engine_drain(p, 0) == ESP_ERR_TIMEOUT);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_FAIL);
    esp_display_present_frame_request_t request = {0};
    assert(esp_display_present_engine_begin_next_frame(p, &request) == ESP_FAIL);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
}

typedef struct {
    esp_display_present_engine_t *p;
    bool reject;
} handoff_t;
static void *new_producer(void *user)
{
    handoff_t *h = user;
    assert(esp_display_present_engine_rebind_producer(h->p, 1000) == (h->reject ? ESP_ERR_INVALID_STATE : ESP_OK));
    if (!h->reject) {
        esp_display_present_engine_status_t status;
        assert(esp_display_present_engine_get_status(h->p, &status) == ESP_OK);
        assert(status.pending_frames == 1); /* Sealed transfer survives handoff. */
    }
    return NULL;
}

static void handoff_and_policy(void)
{
    size_t count;
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_engine_t *p = make(&panel, 2);
    void *buffers[2];
    size_t bytes;
    assert(esp_display_present_engine_get_draw_buffers(p, buffers, 2, &count, &bytes) == ESP_OK);
    assert(count == 2 && bytes == 16 && buffers[0] != buffers[1]);
    begin(p);
    send(p);
    pthread_t thread;
    handoff_t handoff = {p, true};
    assert(pthread_create(&thread, NULL, new_producer, &handoff) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    handoff.reject = false;
    assert(pthread_create(&thread, NULL, new_producer, &handoff) == 0);
    assert(pthread_join(thread, NULL) == 0);
    esp_display_present_engine_status_t status;
    assert(esp_display_present_engine_get_status(p, &status) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_rebind_producer(p, 0) == ESP_OK);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
}

static void explicit_receipts(void)
{
    test_panel_t panel = {.width = 4, .height = 4, .bpp = 2};
    test_panel_t other_panel = {.width = 4, .height = 4, .bpp = 2};
    esp_display_present_engine_t *p = make(&panel, 2), *other = make(&other_panel, 2);
    esp_display_present_engine_receipt_t receipt;
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt) == ESP_OK);
    esp_display_present_frame_status_t status;
    assert(esp_display_present_engine_query_frame(other, receipt, &status) == ESP_ERR_INVALID_STATE);
    assert(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status) == ESP_ERR_NOT_SUPPORTED);
    assert(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_SOURCE_RELEASED, 0, &status) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_engine_release_receipt(p, receipt) == ESP_ERR_INVALID_STATE);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    assert(esp_display_present_engine_query_frame(p, receipt, &status) == ESP_OK && status.terminal && status.src_released && !status.present_confirmed);
    assert(esp_display_present_engine_release_receipt(p, receipt) == ESP_OK);
    assert(esp_display_present_engine_query_frame(p, receipt, &status) == ESP_ERR_INVALID_STATE);
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt) == ESP_OK);
    assert(esp_display_present_engine_cancel_tracking(p, receipt) == ESP_OK);
    assert(panel.count == 1); /* Observation cancellation never ends DMA. */
    assert(esp_display_present_engine_stop(p, 0) == ESP_ERR_TIMEOUT);
    test_panel_complete(&panel);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
    assert(esp_display_present_engine_stop(other, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(other) == ESP_OK);

    panel.immediate = true;
    p = make(&panel, 2);
    esp_display_present_engine_receipt_t receipts[32];
    for (size_t i = 0; i < 32; ++i) {
        begin(p);
        send(p);
        assert(esp_display_present_engine_commit_with_receipt(p, NULL, &receipts[i]) == ESP_OK);
    }
    begin(p);
    send(p);
    assert(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt) == ESP_ERR_NO_MEM && !receipt.value);
    /* Exhausted observers do not discard the still-open frame. */
    assert(esp_display_present_engine_commit_frame(p, NULL) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_ERR_INVALID_STATE);
    for (size_t i = 0; i < 32; ++i) {
        assert(esp_display_present_engine_release_receipt(p, receipts[i]) == ESP_OK);
    }
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
}

static void rotation_and_confirmation(void)
{
    scan_panel_t panel = {0};
    esp_display_present_engine_config_t config = {.backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL,
                                                  .backend = {.width = 4, .height = 2, .tile_rows = 2, .buffer_count = 2, .max_frames = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = scan_interface(&panel)}
                                                 };
    esp_display_present_engine_t *p;
    assert(esp_display_present_engine_create(&config, &p) == ESP_OK);
    begin(p);
    send(p);
    esp_display_present_engine_receipt_t receipt;
    assert(esp_display_present_engine_commit_with_receipt(p, NULL, &receipt) == ESP_OK);
    assert(esp_display_present_engine_set_rotation(p, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, 0) == ESP_ERR_TIMEOUT);
    esp_display_present_backend_caps_t caps;
    assert(esp_display_present_engine_get_caps(p, &caps) == ESP_OK && caps.draw_layout.width == 4);
    esp_display_present_frame_status_t status;
    assert(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status) == ESP_ERR_TIMEOUT);
    scan_confirm(&panel);
    assert(esp_display_present_engine_wait_frame(p, receipt, ESP_DISPLAY_PRESENT_COMPLETION_PRESENT_CONFIRMED, 0, &status) == ESP_OK && status.present_confirmed);
    assert(esp_display_present_engine_set_rotation(p, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, 0) == ESP_OK);
    assert(esp_display_present_engine_get_caps(p, &caps) == ESP_OK && caps.draw_layout.width == 2 && caps.draw_layout.height == 4);
    assert(esp_display_present_engine_query_frame(p, receipt, &status) == ESP_OK && status.present_confirmed);
    assert(esp_display_present_engine_release_receipt(p, receipt) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 1000) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
}

int main(void)
{
    rotation_and_confirmation();
    explicit_receipts();
    handoff_and_policy();
    leases_and_ownership();
    receipts_cancel_and_stop();
    async_error_and_recycle();
    puts("PASS presenter: call order, lease identity, producer task, bounded receipts, drain, cancel, stop, async faults");
    return 0;
}
