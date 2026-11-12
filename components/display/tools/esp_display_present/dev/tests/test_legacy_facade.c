/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Both legacy suites share the same synchronized mock driver. */
#define esp_lcd_panel_draw_bitmap fixture_draw_bitmap
#include "core/present_control.h"
#include "core/present_session.h"
#include "support/legacy_panel.h"
#undef esp_lcd_panel_draw_bitmap
#include "core/present_engine.h"
#include "platform/present_os.h"
#include <time.h>
#include "config/present_geometry.h"
#ifdef LEGACY_WITH_LVGL
#include "esp_lv_present.h"
#endif
/* Inject failures at cross-module metadata/pixel allocation boundaries.
 * Explicit tile counts prevent the optional 2 -> 1 resource policy hiding OOM.
 * No rendering starts during the sweep; every failed create must detach. */
static unsigned allocation_target, allocation_index;
static unsigned allocation_kind; /* metadata, pixels, mutex, event, worker */
void *__real_present_alloc(size_t);
void *__wrap_present_alloc(size_t bytes)
{
    if (allocation_target && allocation_kind == 0 && ++allocation_index == allocation_target) {
        return NULL;
    }
    return __real_present_alloc(bytes);
}
void *__real_present_alloc_pixels_in(size_t, bool);
void *__wrap_present_alloc_pixels_in(size_t bytes, bool external)
{
    if (allocation_target && allocation_kind == 1 && ++allocation_index == allocation_target) {
        return NULL;
    }
    return __real_present_alloc_pixels_in(bytes, external);
}

present_mutex_t *__real_present_mutex_create(void);
present_mutex_t *__wrap_present_mutex_create(void)
{
    if (allocation_target && allocation_kind == 2 && ++allocation_index == allocation_target) {
        return NULL;
    }
    return __real_present_mutex_create();
}
present_os_event_t *__real_present_event_create(void);
present_os_event_t *__wrap_present_event_create(void)
{
    if (allocation_target && allocation_kind == 3 && ++allocation_index == allocation_target) {
        return NULL;
    }
    return __real_present_event_create();
}
present_worker_t *__real_present_worker_create(const char *, present_worker_entry_fn, void *, uint32_t, unsigned);
present_worker_t *__wrap_present_worker_create(const char *name, present_worker_entry_fn entry, void *user, uint32_t stack_bytes, unsigned priority)
{
    if (allocation_target && allocation_kind == 4 && ++allocation_index == allocation_target) {
        return NULL;
    }
    assert(strcmp(name, "present_pixel") == 0 || strcmp(name, "present_backend") == 0);
    return __real_present_worker_create(name, entry, user, stack_bytes, priority);
}

static unsigned injected_snapshot_failures, successful_receipt_releases;
static esp_err_t injected_snapshot_error = ESP_FAIL;
esp_err_t __real_present_presenter_control_get_status_snapshot(esp_display_present_engine_t *, esp_display_present_engine_status_t *, bool *);
esp_err_t __wrap_present_presenter_control_get_status_snapshot(esp_display_present_engine_t *presenter, esp_display_present_engine_status_t *out, bool *valid)
{
    esp_err_t error = __real_present_presenter_control_get_status_snapshot(presenter, out, valid);
    if (injected_snapshot_failures && *valid) {
        --injected_snapshot_failures;
        out->first_error = injected_snapshot_error;
        return injected_snapshot_error;
    }
    return error;
}

static unsigned release_failures;
static struct fake_lcd *early_tracking_panel;
static unsigned early_tracking_calls;

esp_err_t __real_present_presenter_control_release(esp_display_present_engine_t *, esp_display_present_engine_receipt_t);
esp_err_t __wrap_present_presenter_control_release(esp_display_present_engine_t *presenter, esp_display_present_engine_receipt_t receipt)
{
    if (release_failures) {
        --release_failures;
        return ESP_FAIL;
    }
    esp_err_t error = __real_present_presenter_control_release(presenter, receipt);
    if (error == ESP_OK) {
        ++successful_receipt_releases;
    }
    return error;
}

esp_err_t __real_present_presenter_control_query_frame(esp_display_present_engine_t *, esp_display_present_engine_receipt_t, esp_display_present_frame_status_t *);
esp_err_t __wrap_present_presenter_control_query_frame(esp_display_present_engine_t *presenter, esp_display_present_engine_receipt_t receipt, esp_display_present_frame_status_t *out)
{
    if (early_tracking_panel) {
        struct fake_lcd *panel = early_tracking_panel;
        early_tracking_panel = NULL;
        pthread_mutex_lock(&panel->lock);
        while (panel->calls < early_tracking_calls) {
            pthread_cond_wait(&panel->changed, &panel->lock);
        }
        pthread_mutex_unlock(&panel->lock);
        /* Sender holds the core gate through submit; passing it ensures armed. */
        esp_display_present_engine_status_t status;
        OK(present_presenter_control_get_status(presenter, &status));
        boundary(panel);
        boundary(panel);
        esp_err_t error = __real_present_presenter_control_query_frame(presenter, receipt, out);
        assert(error == ESP_OK && out->terminal && out->present_confirmed);
        return error;
    }
    return __real_present_presenter_control_query_frame(presenter, receipt, out);
}

static bool immediate;
static uint16_t image[32];
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x, int y, int ex, int ey, const void *source)
{
    if (!panel->io) {
        return fixture_draw_bitmap(panel, x, y, ex, ey, source);
    }
    unsigned width = panel->physical_width ? panel->physical_width : 8;
    unsigned height = panel->physical_height ? panel->physical_height : 4;
    assert(x >= 0 && y >= 0 && (unsigned)ex <= width && (unsigned)ey <= height);
    panel->source = source;
    ++panel->calls;
    const uint16_t *pixels = source;
    for (int row = y; row < ey; ++row) {
        for (int col = x; col < ex; ++col) {
            image[row * width + col] = *pixels++;
        }
    }
    if (immediate) {
        (void)panel->io(panel, NULL, panel->user);
    }
    return ESP_OK;
}
static esp_display_presenter_config_t io_config(struct fake_lcd *panel)
{
    esp_display_presenter_config_t cfg = config(panel, false);
    cfg.target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    cfg.target.hw.io = panel;
    cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_NONE;
    cfg.transfer_timeout_ms = 20;
    cfg.pipeline_acquire_timeout_ms = 10;
    return cfg;
}
static void start_frame(esp_display_presenter_t *p, bool scroll, bool expect_full)
{
    esp_display_present_area_t damage = {2, 1, 3, 2}, out[4];
    esp_display_present_surface_request_t request = {.dirty_areas = &damage, .dirty_area_count = 1, .has_scroll = scroll,
                                                     .scroll_viewport = {0, 0, 7, 3}, .scroll_dy = 1
                                                    };
    size_t count;
    bool full;
    OK(esp_display_presenter_begin_next_frame(p, &request, out, 4, &count, &full));
    assert(full == expect_full && count == (full ? 0u : 1u));
    if (!full) {
        assert(out[0].x1 == 2 && out[0].y1 == 1 && out[0].x2 == 3);
    }
}
static esp_display_presenter_buffer_t draw(esp_display_presenter_t *p, unsigned value)
{
    esp_display_presenter_buffer_t buffer;
    uint64_t start = present_now_ms();
    for (;;) {
        esp_err_t error = esp_display_presenter_acquire_buffer(p, &buffer);
        if (error == ESP_OK) {
            break;
        }
        assert(error == ESP_ERR_TIMEOUT && !present_expired(start, 1000));
        present_pause();
    }
    size_t rows;
    OK(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 16, 4, &rows));
    assert(rows == 4 && buffer.capacity_bytes >= 64);
    for (unsigned i = 0; i < 32; ++i) {
        ((uint16_t *)buffer.surface.pixels)[i] = value;
    }
    esp_display_present_area_t area = {0, 0, 7, 3};
    OK(esp_display_presenter_submit_buffer(p, &buffer, &area, 16));
    assert(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 16, 4, &rows) == ESP_ERR_INVALID_STATE);
    return buffer;
}
static void *control_query(void *user)
{
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(user, &status));
    assert(status.last_submitted_frame == 1 && status.completed_transfer_frame == 0);
    esp_display_presenter_state_t state;
    OK(esp_display_presenter_get_state(user, &state));
    assert(state == ESP_DISPLAY_PRESENTER_STATE_OPEN);
    return NULL;
}
static void delete_eventually(esp_display_presenter_t *p)
{
    uint64_t start = present_now_ms();
    for (;;) {
        esp_err_t error = esp_display_presenter_delete(p);
        if (error == ESP_OK) {
            return;
        }
        assert(error == ESP_ERR_INVALID_STATE); /* Closing join may outlive 1 ms. */
        assert(!present_expired(start, 1000));
        present_pause();
    }
}
static void *control_stop(void *user)
{
    uint64_t start = present_now_ms();
    for (;;) {
        esp_err_t error = esp_display_presenter_stop(user);
        if (error == ESP_OK) {
            break;
        }
        assert(error == ESP_ERR_INVALID_STATE && !present_expired(start, 1000));
        present_pause();
    }
    return NULL;
}
static void *control_delete(void *user)
{
    delete_eventually(user);
    return NULL;
}
static void leases_and_stop(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    unavailable = true;
    esp_display_presenter_config_t cfg = io_config(&panel);
    esp_display_presenter_t *p;
    OK(esp_display_presenter_create(&cfg, &p));
    esp_display_presenter_caps_t caps;
    OK(esp_display_presenter_get_caps(p, &caps));
    assert(caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION && caps.width == 8 && !caps.previous_surface_readable);
    size_t alignment;
    OK(esp_display_presenter_get_buffer_alignment(p, &alignment));
    assert(alignment == 64);
    esp_display_presenter_drawbuf_t pool;
    OK(esp_display_presenter_get_drawbuf(p, &pool));
    assert(pool.count == 2 && pool.bytes >= 64);
    start_frame(p, false, true);
    immediate = false;
    esp_display_presenter_buffer_t old = draw(p, 0x4567);
    esp_display_presenter_frame_receipt_t receipt;
    assert(esp_display_presenter_commit_frame_with_receipt(p, NULL, true, &receipt) == ESP_ERR_NOT_SUPPORTED && !receipt.frame_id);
    OK(esp_display_presenter_commit_frame_with_receipt(p, NULL, false, &receipt));
    assert(receipt.frame_id == 1 && receipt.generation == 1 && !receipt.display_tracked);
    pthread_t reader;
    assert(!pthread_create(&reader, NULL, control_query, p));
    assert(!pthread_join(reader, NULL));
    assert(esp_display_presenter_quiesce(p, 1) == ESP_ERR_TIMEOUT);
    (void)panel.io(&panel, NULL, panel.user);
    OK(esp_display_presenter_quiesce(p, 100));
    start_frame(p, false, false);
    esp_display_presenter_buffer_t current;
    OK(esp_display_presenter_acquire_buffer(p, &current));
    assert(current.lease_id != old.lease_id);
    esp_display_present_area_t full = {0, 0, 7, 3};
    assert(esp_display_presenter_submit_buffer(p, &old, &full, 16) == ESP_ERR_INVALID_STATE);
    assert(esp_display_presenter_stop(p) == ESP_ERR_INVALID_STATE); /* Cannot confiscate lease. */
    esp_display_presenter_state_t state = ESP_DISPLAY_PRESENTER_STATE_STOPPED;
    OK(esp_display_presenter_get_state(p, &state));
    assert(state == ESP_DISPLAY_PRESENTER_STATE_OPEN);
    esp_display_presenter_cancel_frame(p);
    assert(!pthread_create(&reader, NULL, control_stop, p));
    assert(!pthread_join(reader, NULL));
    assert(esp_display_presenter_rebind_producer(p) == ESP_ERR_INVALID_STATE);
    assert(!pthread_create(&reader, NULL, control_delete, p));
    assert(!pthread_join(reader, NULL));
    free_panel(&panel);
}
static void rotation_and_fault(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    esp_display_presenter_t *p;
    OK(esp_display_presenter_create(&cfg, &p));
    immediate = true;
    start_frame(p, false, true);
    (void)draw(p, 0x1111);
    OK(esp_display_presenter_commit_frame(p, NULL));
    start_frame(p, false, false);
    esp_display_presenter_cancel_frame(p);
    OK(esp_display_presenter_set_rotation(p, ESP_DISPLAY_PRESENT_ROTATE_180));
    int32_t x = 0, y = 0;
    OK(esp_display_presenter_map_point(p, &x, &y));
    assert(x == 7 && y == 3);
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.generation == 2 && status.last_submitted_frame == 1);
    assert(esp_display_presenter_cancel_frame_tracking(p, 1, 1) == ESP_ERR_NOT_FOUND);
    assert(esp_display_presenter_cancel_frame_tracking(p, 1, 0) == ESP_ERR_INVALID_ARG);
    delete_eventually(p);
    free_panel(&panel);

    /* An in-place byte-swap instance really lacks a rotated destination:
     * keep the backend rejection and the old successful zero-degree path. */
    init_panel(&panel);
    cfg = io_config(&panel);
    cfg.target.hw.swap_bytes = true;
    OK(esp_display_presenter_create(&cfg, &p));
    assert(esp_display_presenter_set_rotation(p, ESP_DISPLAY_PRESENT_ROTATE_180) == ESP_ERR_NOT_SUPPORTED);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.generation == 1);
    delete_eventually(p);
    free_panel(&panel);

    init_panel(&panel);
    cfg = io_config(&panel);
    cfg.transfer_timeout_ms = 1;
    OK(esp_display_presenter_create(&cfg, &p));
    immediate = false;
    start_frame(p, false, true);
    (void)draw(p, 0x9876);
    OK(esp_display_presenter_commit_frame(p, NULL));
    /* Deliberate deadline test, not event-order coordination. */
    uint64_t start = present_now_ms();
    while (!present_expired(start, 2)) {
        present_pause();
    }
    esp_display_present_fault_reason_t reason;
    OK(esp_display_presenter_get_fault_reason(p, &reason));
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT);
    esp_err_t fault_error = ESP_OK;
    OK(esp_display_presenter_get_fault_error(p, &fault_error));
    assert(fault_error == ESP_ERR_TIMEOUT);
    assert(esp_display_presenter_quiesce(p, 100) == ESP_ERR_INVALID_STATE);
    assert(esp_display_presenter_rebind_producer(p) == ESP_ERR_INVALID_STATE);
    assert(esp_display_presenter_delete(p) == ESP_ERR_INVALID_STATE);
    assert(panel.io && panel.source); /* Storage/registration retained. */
    (void)panel.io(&panel, NULL, panel.user);
    delete_eventually(p);
    free_panel(&panel);
}
/* A void cancel revokes the producer immediately, while accepted async reads
 * retain their source until actual completion. No sleeps coordinate the copy. */
static void cancel_pending_transfer(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    cfg.pipeline_acquire_timeout_ms = 1;
    cfg.transfer_timeout_ms = 1000;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    start_frame(presenter, false, true);
    immediate = false;
    esp_display_presenter_buffer_t old = draw(presenter, 0x8ccc);
    const uint16_t *dma = panel.source;
    esp_display_presenter_cancel_frame(presenter);
    esp_display_presenter_cancel_frame(presenter); /* Idempotent, no new submit. */
    assert(panel.calls == 1 && panel.io);
    size_t count;
    bool full;
    assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
    for (unsigned i = 0; i < 32; ++i) {
        assert(dma[i] == 0x8ccc);
    }
    (void)panel.io(&panel, NULL, panel.user); /* Only actual completion drains. */
    OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
    assert(full && !count);
    esp_display_presenter_buffer_t next;
    OK(esp_display_presenter_acquire_buffer(presenter, &next));
    assert(next.lease_id != old.lease_id);
    esp_display_presenter_cancel_frame(presenter); /* Held, not submitted. */
    OK(esp_display_presenter_quiesce(presenter, 100));
    assert(panel.calls == 1);
    delete_eventually(presenter);
    free_panel(&panel);
}
static void *control_cancel_drain(void *context)
{
    OK(esp_display_presenter_quiesce(context, 1000));
    return NULL;
}
static void cancel_pending_copy(void)
{
    for (unsigned path = 0; path < 3; ++path) {
        bool closing = path == 2;
        struct fake_lcd panel;
        init_panel(&panel);
        unavailable = false;
        esp_display_presenter_config_t cfg = config(&panel, false);
        cfg.pipeline_acquire_timeout_ms = 1;
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        size_t count;
        bool full;
        assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
        boundary(&panel);
        boundary(&panel);
        OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
        pthread_mutex_lock(&executor_gate);
        executor_hold = true;
        executor_entered = false;
        pthread_mutex_unlock(&executor_gate);
        esp_display_presenter_buffer_t old = draw(presenter, 0x6aaa);
        pthread_mutex_lock(&executor_gate);
        while (!executor_entered) {
            pthread_cond_wait(&executor_changed, &executor_gate);
        }
        pthread_mutex_unlock(&executor_gate);
        esp_display_presenter_cancel_frame(presenter);
        esp_display_present_area_t area = {0, 0, 7, 3};
        assert(esp_display_presenter_submit_buffer(presenter, &old, &area, 16) == ESP_ERR_INVALID_STATE);
        size_t rows;
        assert(old.resolve_rows(old.resolve_rows_ctx, old.lease_id, 16, 4, &rows) == ESP_ERR_INVALID_STATE);
        assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
        assert(!count && !full); /* Not INVALID_STATE: cancellation is admission work. */
        esp_display_present_fault_reason_t reason;
        OK(esp_display_presenter_get_fault_reason(presenter, &reason));
        assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
        for (unsigned i = 0; i < 32; ++i) {
            assert(((const uint16_t *)old.surface.pixels)[i] == 0x6aaa);
        }
        if (closing) {
            assert(esp_display_presenter_delete(presenter) == ESP_ERR_INVALID_STATE);
            assert(panel.source && panel.dsi); /* Bootstrap/source registrations retained. */
        }
        pthread_mutex_lock(&executor_gate);
        executor_hold = false;
        pthread_cond_broadcast(&executor_changed);
        pthread_mutex_unlock(&executor_gate);
        if (!closing) {
            if (path == 1) {
                pthread_t control;
                assert(!pthread_create(&control, NULL, control_cancel_drain, presenter));
                assert(!pthread_join(control, NULL));
                OK(esp_display_presenter_rebind_producer(presenter));
            }
            uint64_t start = present_now_ms();
            esp_err_t error;
            do {
                error = esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full);
                assert(error == ESP_OK || (error == ESP_ERR_TIMEOUT && !present_expired(start, 1000)));
            } while (error == ESP_ERR_TIMEOUT);
            assert(full && !count);
            esp_display_presenter_buffer_t next = draw(presenter, 0x7bbb);
            assert(next.lease_id != old.lease_id);
            esp_display_presenter_cancel_frame(presenter);
            OK(esp_display_presenter_quiesce(presenter, 1000));
        }
        delete_eventually(presenter);
        free_panel(&panel);
        unavailable = true;
    }
}
typedef struct {
    esp_display_present_engine_t *core;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool held, release;
} gate_fixture_t;

static void *hold_core_gate(void *context)
{
    gate_fixture_t *fixture = context;
    present_presenter_test_gate_lock(fixture->core);
    pthread_mutex_lock(&fixture->lock);
    fixture->held = true;
    pthread_cond_broadcast(&fixture->changed);
    while (!fixture->release) {
        pthread_cond_wait(&fixture->changed, &fixture->lock);
    }
    pthread_mutex_unlock(&fixture->lock);
    present_presenter_test_gate_unlock(fixture->core);
    return NULL;
}

static void cancel_before_core_gate(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    esp_display_present_engine_t *core;
    OK(present_session_bind(presenter, &core));
    present_session_unbind(presenter);
    start_frame(presenter, false, true);
    esp_display_presenter_buffer_t held;
    OK(esp_display_presenter_acquire_buffer(presenter, &held));
    gate_fixture_t fixture = {.core = core, .lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, hold_core_gate, &fixture));
    pthread_mutex_lock(&fixture.lock);
    while (!fixture.held) {
        pthread_cond_wait(&fixture.changed, &fixture.lock);
    }
    pthread_mutex_unlock(&fixture.lock);
    esp_display_presenter_cancel_frame(presenter); /* First zero-budget attempt cannot take gate. */
    size_t rows;
    assert(held.resolve_rows(held.resolve_rows_ctx, held.lease_id, 16, 4, &rows) == ESP_ERR_INVALID_STATE);
    pthread_mutex_lock(&fixture.lock);
    fixture.release = true;
    pthread_cond_broadcast(&fixture.changed);
    pthread_mutex_unlock(&fixture.lock);
    assert(!pthread_join(thread, NULL));
    size_t count;
    bool full;
    OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
    assert(full && !count);
    esp_display_presenter_cancel_frame(presenter);
    esp_display_present_fault_reason_t reason;
    OK(esp_display_presenter_get_fault_reason(presenter, &reason));
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
    delete_eventually(presenter);
    assert(!pthread_mutex_destroy(&fixture.lock));
    assert(!pthread_cond_destroy(&fixture.changed));
    free_panel(&panel);
}

static void raw_vsync(struct fake_lcd *panel)
{
    pthread_mutex_lock(&panel->lock);
    dpi_cb_t callback = panel->dsi_vsync;
    void *user = panel->user;
    pthread_mutex_unlock(&panel->lock);
    assert(callback);
    (void)callback(panel, NULL, user);
}
static void finish_scanout(struct fake_lcd *panel, esp_display_presenter_t *p, unsigned expected)
{
    pthread_mutex_lock(&panel->lock);
    while (panel->calls < expected) {
        pthread_cond_wait(&panel->changed, &panel->lock);
    }
    pthread_mutex_unlock(&panel->lock);
    /* Status passes the core gate: panel.submit has returned and is armed. */
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(p, &status));
    boundary(panel);
    boundary(panel);
}
static void framebuffer_tracking(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = config(&panel, false);
    cfg.pipeline_acquire_timeout_ms = 1;
    esp_display_presenter_t *p;
    OK(esp_display_presenter_create(&cfg, &p));
    esp_display_present_surface_request_t req = {0};
    esp_display_present_area_t area;
    size_t count;
    bool full;
    /* Initial source must be established before the first producer lease. */
    assert(esp_display_presenter_begin_next_frame(p, &req, &area, 1, &count, &full) == ESP_ERR_TIMEOUT);
    assert(!count && panel.calls == 1);
    boundary(&panel);
    boundary(&panel);
    start_frame(p, false, true);
    (void)draw(p, 0x3333);
    esp_display_presenter_frame_receipt_t receipt;
    OK(esp_display_presenter_commit_frame_with_receipt(p, NULL, true, &receipt));
    assert(receipt.display_tracked && receipt.frame_id == 1);
    finish_scanout(&panel, p, 2);
    esp_display_presenter_frame_status_t status;
    release_failures = 1;
    assert(esp_display_presenter_get_frame_status(p, &status) == ESP_FAIL);
    assert(status.completed_present_frame == 1 && !status.confirmed_present_frame);
    raw_vsync(&panel); /* Retry must retain the original fence observation. */
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(esp_display_presenter_begin_next_frame(p, &req, &area, 1, &count, &full) == ESP_ERR_TIMEOUT);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(!status.confirmed_present_frame);
    raw_vsync(&panel);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.confirmed_present_frame == 1);
    start_frame(p, false, false);
    (void)draw(p, 0x4444);
    OK(esp_display_presenter_commit_frame_with_receipt(p, NULL, true, &receipt));
    OK(esp_display_presenter_cancel_frame_tracking(p, receipt.frame_id, receipt.generation));
    finish_scanout(&panel, p, 3);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.completed_present_frame == 2 && status.confirmed_present_frame == 1);
    /* Canceled observation must not arm a stale confirmation for frame 3. */
    raw_vsync(&panel);
    raw_vsync(&panel);
    start_frame(p, false, false);
    (void)draw(p, 0x5555);
    OK(esp_display_presenter_commit_frame_with_receipt(p, NULL, true, &receipt));
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.confirmed_present_frame == 1);
    finish_scanout(&panel, p, 4);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.completed_present_frame == 3 && status.confirmed_present_frame == 1);
    raw_vsync(&panel);
    raw_vsync(&panel);
    assert(esp_display_presenter_cancel_frame_tracking(p, receipt.frame_id, receipt.generation) == ESP_ERR_NOT_FOUND);
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.confirmed_present_frame == 3);
    delete_eventually(p);
    free_panel(&panel);
}
/* The scanout may confirm between commit and the facade's first receipt query. */
static void tracking_early_terminal(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = config(&panel, false);
    cfg.pipeline_acquire_timeout_ms = 1;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    size_t count;
    bool full;
    assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
    boundary(&panel);
    boundary(&panel);
    start_frame(presenter, false, true);
    (void)draw(presenter, 0x1234);
    early_tracking_panel = &panel;
    early_tracking_calls = 2;
    esp_display_presenter_frame_receipt_t receipt;
    OK(esp_display_presenter_commit_frame_with_receipt(presenter, NULL, true, &receipt));
    assert(!early_tracking_panel && receipt.frame_id == 1);
    esp_display_presenter_frame_status_t status;
    raw_vsync(&panel);
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(status.confirmed_present_frame == 0);
    raw_vsync(&panel);
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(status.confirmed_present_frame == 1);
    delete_eventually(presenter);
    free_panel(&panel);
}
/* An accepted commit keeps its frame ID even if observation reports a fault.
 * VSYNC is only an observer: it cannot release an unfinished scanout. */
static void faulted_tracking_snapshot(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = config(&panel, false);
    cfg.pipeline_acquire_timeout_ms = 1;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    size_t count;
    bool full;
    assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
    boundary(&panel);
    boundary(&panel);
    start_frame(presenter, false, true);
    (void)draw(presenter, 0x5678);
    unsigned released_before = successful_receipt_releases;
    injected_snapshot_failures = 1;
    esp_display_presenter_frame_receipt_t receipt;
    OK(esp_display_presenter_commit_frame_with_receipt(presenter, NULL, true, &receipt));
    assert(receipt.frame_id == 1 && receipt.display_tracked);
    esp_display_present_fault_reason_t reason;
    OK(esp_display_presenter_get_fault_reason(presenter, &reason));
    /* Injected first_error is ESP_FAIL, not a deadline: it must not be
     * reported as a transfer timeout, and the original error stays visible. */
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED);
    esp_err_t fault_error = ESP_OK;
    OK(esp_display_presenter_get_fault_error(presenter, &fault_error));
    assert(fault_error == ESP_FAIL);
    raw_vsync(&panel);
    raw_vsync(&panel);
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(status.confirmed_present_frame == 0 && successful_receipt_releases == released_before);
    finish_scanout(&panel, presenter, 2);
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(successful_receipt_releases == released_before + 1 && status.confirmed_present_frame == 0);
    raw_vsync(&panel);
    raw_vsync(&panel);
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(status.confirmed_present_frame == 0);
    delete_eventually(presenter);
    free_panel(&panel);
}
/* Formal fault-classification matrix. The snapshot wrapper injects the
 * engine first_error directly, so each row exercises only the facade
 * classification and original-error retention, not a backend path.
 * Contract: ESP_ERR_TIMEOUT is the only TRANSFER_TIMEOUT; every other
 * accepted-operation error is TRANSFER_FAILED with the error preserved.
 * PRODUCER_PROTOCOL never comes from this path; it is latched only by the
 * facade's own protocol checks (see STATE_MACHINE.md). */
static void fault_classification_matrix(void)
{
    static const struct {
        esp_err_t injected;
        esp_display_present_fault_reason_t reason;
    } cases[] = {
        {ESP_OK, ESP_DISPLAY_PRESENT_FAULT_NONE},
        {ESP_ERR_TIMEOUT, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT},
        {ESP_FAIL, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED},
        {ESP_ERR_NO_MEM, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED},
        {ESP_ERR_NOT_SUPPORTED, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED},
        {ESP_ERR_INVALID_STATE, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct fake_lcd panel;
        init_panel(&panel);
        esp_display_presenter_config_t cfg = config(&panel, false);
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        esp_display_present_fault_reason_t reason = ESP_DISPLAY_PRESENT_FAULT_TRANSFER_FAILED;
        esp_err_t fault_error = ESP_FAIL;
        OK(esp_display_presenter_get_fault_reason(presenter, &reason));
        assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
        OK(esp_display_presenter_get_fault_error(presenter, &fault_error));
        assert(fault_error == ESP_OK);
        if (cases[i].injected != ESP_OK) {
            injected_snapshot_error = cases[i].injected;
            injected_snapshot_failures = 1;
            OK(esp_display_presenter_get_fault_reason(presenter, &reason));
            assert(reason == cases[i].reason);
            OK(esp_display_presenter_get_fault_error(presenter, &fault_error));
            assert(fault_error == cases[i].injected);
            /* The first fault is latched: a later healthy observation must
             * not clear or reclassify it. */
            OK(esp_display_presenter_get_fault_reason(presenter, &reason));
            assert(reason == cases[i].reason);
        }
        injected_snapshot_error = ESP_FAIL;
        delete_eventually(presenter);
        free_panel(&panel);
    }
}

/* Fault after receipt reclamation must prevent the already-armed watermark. */
static void fault_after_tracking_fence(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = config(&panel, false);
    cfg.pipeline_acquire_timeout_ms = 1;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    size_t count;
    bool full;
    assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
    boundary(&panel);
    boundary(&panel);
    start_frame(presenter, false, true);
    (void)draw(presenter, 0x9876);
    esp_display_presenter_frame_receipt_t receipt;
    OK(esp_display_presenter_commit_frame_with_receipt(presenter, NULL, true, &receipt));
    finish_scanout(&panel, presenter, 2);
    esp_display_presenter_frame_status_t status;
    unsigned released_before = successful_receipt_releases;
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(successful_receipt_releases == released_before + 1);
    raw_vsync(&panel);
    injected_snapshot_failures = 1;
    assert(esp_display_presenter_get_frame_status(presenter, &status) == ESP_FAIL);
    raw_vsync(&panel);
    OK(esp_display_presenter_get_frame_status(presenter, &status));
    assert(status.confirmed_present_frame == 0);
    delete_eventually(presenter);
    free_panel(&panel);
}
static void mode_matrix(void)
{
    const esp_display_present_mode_t modes[] = {
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL,
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL,
        ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL, ESP_DISPLAY_PRESENT_MODE_AUTO,
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        struct fake_lcd panel;
        init_panel(&panel);
        esp_display_presenter_config_t cfg = config(&panel, false);
        cfg.target.fb.mode = modes[i];
        cfg.pipeline_acquire_timeout_ms = 1;
        uint8_t buffers = 0;
        OK(esp_display_present_get_required_frame_buffer_count(&cfg.target, &buffers));
        assert(buffers == (i == 1 || i == 4 || i == 5 ? 3 : 2));
        esp_display_presenter_t *p;
        OK(esp_display_presenter_create(&cfg, &p));
        esp_display_present_surface_request_t request = {0};
        esp_display_present_area_t area;
        size_t count;
        bool full;
        assert(esp_display_presenter_begin_next_frame(p, &request, &area, 1, &count, &full) == ESP_ERR_TIMEOUT);
        boundary(&panel);
        boundary(&panel);
        start_frame(p, false, true);
        (void)draw(p, 0x6666);
        OK(esp_display_presenter_commit_frame(p, NULL));
        finish_scanout(&panel, p, 2);
        OK(esp_display_presenter_quiesce(p, 100));
        for (unsigned pixel = 0; pixel < 32; ++pixel) {
            assert(((const uint16_t *)panel.source)[pixel] == 0x6666);
        }
        esp_display_present_surface_request_t scroll = {.has_scroll = true,
                                                        .scroll_viewport = {0, 0, 7, 3}, .scroll_dy = 1
                                                       };
        if (i < 3) { /* FULL / DIRECT: a safe full raster, never a fake scroll copy. */
            OK(esp_display_presenter_begin_next_frame(p, &scroll, NULL, 0, &count, &full));
            assert(full && count == 0);
        } else {
            assert(esp_display_presenter_begin_next_frame(p, &scroll, NULL, 0, &count, &full) == ESP_ERR_NOT_SUPPORTED);
            assert(!full && !count);
            start_frame(p, false, false);
        }
        (void)draw(p, 0x8888);
        OK(esp_display_presenter_commit_frame(p, NULL));
        finish_scanout(&panel, p, 3);
        OK(esp_display_presenter_quiesce(p, 100));
        for (unsigned pixel = 0; pixel < 32; ++pixel) {
            assert(((const uint16_t *)panel.source)[pixel] == 0x8888);
        }
        delete_eventually(p);
        free_panel(&panel);
    }
    esp_display_present_target_config_t target = {0};
    uint8_t buffers = 99;
    assert(esp_display_present_get_required_frame_buffer_count(&target, &buffers) == ESP_ERR_NOT_SUPPORTED && !buffers);
    struct fake_lcd query_panel = {0};
    target.hw.io = &query_panel; /* AUTO with IO is a V1-successful query path. */
    OK(esp_display_present_get_required_frame_buffer_count(&target, &buffers));
    assert(!buffers);
    target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    OK(esp_display_present_get_required_frame_buffer_count(&target, &buffers));
    assert(!buffers);
    target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
    target.hw.te_enabled = false;
    assert(esp_display_present_get_required_frame_buffer_count(&target, &buffers) == ESP_ERR_NOT_SUPPORTED && !buffers);
    target.hw.te_enabled = true;
    OK(esp_display_present_get_required_frame_buffer_count(&target, &buffers));
    assert(!buffers); /* Pure query; TE creation is still explicitly unsupported. */
}
static void expect_plan_error(esp_display_presenter_t *presenter,
                              const esp_display_present_surface_request_t *request,
                              size_t capacity, esp_err_t expected)
{
    esp_display_present_area_t areas[2];
    size_t count = 99;
    bool full = true;
    assert(esp_display_presenter_begin_next_frame(presenter, request, areas, capacity, &count, &full) == expected);
    assert(count == 0 && !full);
}
static void frame_plan_contracts(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    cfg.max_damage_areas = 2;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    esp_display_present_area_t damage[3] = {{0, 0, 1, 1}, {2, 1, 3, 2}, {4, 2, 5, 3}};
    esp_display_present_surface_request_t request = {.dirty_areas = damage, .dirty_area_count = 1};
    /* First-frame fallback does not hide malformed input. */
    damage[0].x1 = -1;
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    damage[0] = (esp_display_present_area_t) {
        0, 0, 8, 3
    };
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    damage[0] = (esp_display_present_area_t) {
        3, 0, 1, 1
    };
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    damage[0] = (esp_display_present_area_t) {
        0, 0, 1, 1
    };
    request.dirty_area_count = 3;
    expect_plan_error(presenter, &request, 3, ESP_ERR_INVALID_ARG);
    request.dirty_area_count = 2;
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    request.dirty_area_count = 0;
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    request.dirty_areas = NULL;
    request.dirty_area_count = 1;
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    request = (esp_display_present_surface_request_t) {
        .has_scroll = true, .scroll_viewport = {0, 0, 7, 3}
    };
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG); /* zero delta */
    request.scroll_dy = 1;
    request.scroll_viewport.y2 = 4;
    expect_plan_error(presenter, &request, 1, ESP_ERR_INVALID_ARG);
    request.scroll_viewport.y2 = 3;
    expect_plan_error(presenter, &request, 1, ESP_ERR_NOT_SUPPORTED); /* PARTITION */
    assert(!panel.calls);
    size_t count;
    bool full;
    OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
    assert(full && count == 0);
    immediate = true;
    (void)draw(presenter, 0x7777);
    OK(esp_display_presenter_commit_frame(presenter, NULL));

    const uint16_t alignments[] = {0, 1, 2, 3, 4, 7, 65535};
    uint32_t random = 17;
    for (unsigned trial = 0; trial < 700; ++trial) {
        random = random * 1664525u + 1013904223u;
        unsigned x = random % 8, y = (random >> 4) % 4;
        damage[0] = (esp_display_present_area_t) {
            x, y, x + (random >> 8) % (8 - x), y + (random >> 16) % (4 - y)
        };
        request = (esp_display_present_surface_request_t) {
            .dirty_areas = damage, .dirty_area_count = 1,
            .render_alignment = {alignments[trial % 7], alignments[(trial / 7) % 7],
                                 alignments[(trial / 49) % 7], alignments[(trial / 13) % 7]
                                }
        };
        esp_display_present_area_t expected, actual;
        OK(esp_display_present_geometry_align_area((esp_display_present_size_t) {
            8, 4
        },
        damage, &request.render_alignment, &expected));
        OK(esp_display_presenter_begin_next_frame(presenter, &request, &actual, 1, &count, &full));
        bool whole = damage[0].x1 == 0 && damage[0].y1 == 0 && damage[0].x2 == 7 && damage[0].y2 == 3;
        assert(full == whole && count == (whole ? 0u : 1u));
        if (!whole) {
            assert(!memcmp(&actual, &expected, sizeof(actual)));
        }
        (void)draw(presenter, 0x7777);
        OK(esp_display_presenter_commit_frame(presenter, NULL));
    }
    delete_eventually(presenter);
    free_panel(&panel);
}
static void resolve_alignment_contract(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    const uint16_t alignments[] = {3, 7};
    for (size_t i = 0; i < 2; ++i) {
        esp_display_present_surface_request_t request = {.render_alignment.height_pixels = alignments[i]};
        size_t count;
        bool full;
        OK(esp_display_presenter_begin_next_frame(presenter, &request, NULL, 0, &count, &full));
        assert(!count && full);
        esp_display_presenter_buffer_t buffer;
        OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
        size_t rows = 99;
        esp_err_t error = buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 16, 10, &rows);
        assert(error == (i == 0 ? ESP_OK : ESP_ERR_INVALID_SIZE));
        assert(rows == (i == 0 ? 3u : 0u));
        OK(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 16, 2, &rows));
        assert(rows == 2); /* A final, fully requested band is not rounded down. */
        esp_display_presenter_cancel_frame(presenter);
    }
    delete_eventually(presenter);
    free_panel(&panel);
    init_panel(&panel);
    cfg = io_config(&panel);
    cfg.width = 20000;
    OK(esp_display_presenter_create(&cfg, &presenter));
    size_t count;
    bool full;
    OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
    esp_display_presenter_buffer_t buffer;
    OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    size_t rows = 99;
    assert(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 2, 65536, &rows) == ESP_ERR_INVALID_SIZE && rows == 0);
    OK(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 2, 65535, &rows));
    assert(rows == 65535); /* Capacity, not preferred full-width shape, controls scratch rows. */
    esp_display_presenter_cancel_frame(presenter);
    delete_eventually(presenter);
    free_panel(&panel);
}
/* Extension paths that V1 rejected: compare every physical pixel, including
 * a second frame after a same-axis runtime change. The IO callback copies the
 * payload before completing; no test reads a buffer after it is returned. */
static void direct_dirty_contract(void)
{
    unavailable = false; /* Gate the real asynchronous prefill below. */
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = config(&panel, false);
    cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT;
    cfg.pipeline_acquire_timeout_ms = 1;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    esp_display_presenter_caps_t caps;
    OK(esp_display_presenter_get_caps(presenter, &caps));
    assert(caps.previous_surface_readable && !caps.surface_retains_content && caps.supports_coverage_areas && !caps.drawbuf_bytes);
    size_t count;
    bool full;
    assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
    boundary(&panel);
    boundary(&panel);
    start_frame(presenter, false, true);
    (void)draw(presenter, 0x1111);
    OK(esp_display_presenter_commit_frame(presenter, NULL));
    finish_scanout(&panel, presenter, 2);
    OK(esp_display_presenter_quiesce(presenter, 100));
    esp_display_present_area_t dirty = {2, 1, 3, 2}, out;
    esp_display_present_surface_request_t request = {.dirty_areas = &dirty, .dirty_area_count = 1};
    OK(esp_display_presenter_begin_next_frame(presenter, &request, &out, 1, &count, &full));
    assert(!full && count == 1 && !memcmp(&out, &dirty, sizeof(out)));
    esp_display_presenter_buffer_t buffer;
    /* A successful begin may have started asynchronous DIRECT prefill.
     * Its 1 ms admission budget is allowed to expire before the worker runs. */
    uint64_t acquire_start = present_now_ms();
    for (;;) {
        esp_err_t error = esp_display_presenter_acquire_buffer(presenter, &buffer);
        if (error == ESP_OK) {
            break;
        }
        assert(error == ESP_ERR_TIMEOUT && !present_expired(acquire_start, 1000));
    }
    for (unsigned i = 0; i < 32; ++i) {
        assert(((uint16_t*)buffer.surface.pixels)[i] == 0x1111);
    }
    for (unsigned y = 1; y < 3; ++y)
        for (unsigned x = 2; x < 4; ++x) {
            ((uint16_t*)buffer.surface.pixels)[y * 8 + x] = 0x2222;
        }
    OK(esp_display_presenter_submit_buffer(presenter, &buffer, &dirty, 16));
    esp_display_presenter_submit_t submit = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_AREAS, .areas = &dirty, .area_count = 1};
    OK(esp_display_presenter_commit_frame(presenter, &submit));
    finish_scanout(&panel, presenter, 3);
    OK(esp_display_presenter_quiesce(presenter, 100));
    for (unsigned y = 0; y < 4; ++y)
        for (unsigned x = 0; x < 8; ++x) {
            assert(((const uint16_t*)panel.source)[y * 8 + x] == ((y >= 1 && y < 3 && x >= 2 && x < 4) ? 0x2222 : 0x1111));
        }
    pthread_mutex_lock(&executor_gate);
    executor_hold = true;
    executor_entered = false;
    pthread_mutex_unlock(&executor_gate);
    OK(esp_display_presenter_begin_next_frame(presenter, &request, &out, 1, &count, &full));
    pthread_mutex_lock(&executor_gate);
    while (!executor_entered) {
        pthread_cond_wait(&executor_changed, &executor_gate);
    }
    pthread_mutex_unlock(&executor_gate);
    /* No lease exists, but DIRECT prefill still owns source and destination. */
    assert(esp_display_presenter_stop(presenter) == ESP_ERR_INVALID_STATE);
    esp_display_present_fault_reason_t reason;
    OK(esp_display_presenter_get_fault_reason(presenter, &reason));
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
    pthread_mutex_lock(&executor_gate);
    executor_hold = false;
    pthread_cond_broadcast(&executor_changed);
    pthread_mutex_unlock(&executor_gate);
    (void)control_stop(presenter);
    OK(esp_display_presenter_get_fault_reason(presenter, &reason));
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
    delete_eventually(presenter);
    free_panel(&panel);
}
static void gram_rotation_superset(void)
{
    immediate = true;
    for (unsigned variant = 0; variant < 8; ++variant) {
        unsigned r = variant % 4;
        struct fake_lcd panel;
        init_panel(&panel);
        panel.physical_width = r & 1 ? 4 : 8;
        panel.physical_height = r & 1 ? 8 : 4;
        esp_display_presenter_config_t cfg = io_config(&panel);
        cfg.target.hw.rotation = r * 90;
        cfg.target.drawbuf.in_psram = variant >= 4;
        uint8_t buffers = 99;
        OK(esp_display_present_get_required_frame_buffer_count(&cfg.target, &buffers));
        assert(!buffers);
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        for (unsigned pass = 0; pass < 4; ++pass) {
            unsigned rotation = ((r + pass) % 4) * 90;
            if (pass) {
                OK(esp_display_presenter_set_rotation(presenter, rotation));
            }
            unsigned width = pass & 1 ? 4 : 8;
            unsigned height = pass & 1 ? 8 : 4;
            esp_display_presenter_caps_t caps;
            OK(esp_display_presenter_get_caps(presenter, &caps));
            assert(caps.width == width && caps.height == height);
            size_t count;
            bool full;
            OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
            assert(full && !count);
            esp_display_presenter_buffer_t buffer;
            OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
            uint16_t expected[32] = {0};
            for (unsigned y = 0; y < height; ++y) {
                for (unsigned x = 0; x < width; ++x) {
                    uint16_t pixel = 0x100 + pass * 0x100 + y * width + x;
                    ((uint16_t *)buffer.surface.pixels)[y * width + x] = pixel;
                    unsigned px, py;
                    switch (rotation) {
                    case 90: px = height - 1 - y; py = x; break;
                    case 180: px = width - 1 - x; py = height - 1 - y; break;
                    case 270: px = y; py = width - 1 - x; break;
                    default: px = x; py = y; break;
                    }
                    expected[py * panel.physical_width + px] = pixel;
                }
            }
            esp_display_present_area_t area = {0, 0, width - 1, height - 1};
            OK(esp_display_presenter_submit_buffer(presenter, &buffer, &area, width * 2));
            OK(esp_display_presenter_commit_frame(presenter, NULL));
            OK(esp_display_presenter_quiesce(presenter, 1000));
            assert(!memcmp(image, expected, sizeof(expected)));
            if (cfg.target.drawbuf.in_psram) {
                assert(panel.source != buffer.surface.pixels);
            }
        }
        delete_eventually(presenter);
        free_panel(&panel);
    }
}
static void te_compatibility(void)
{
    for (unsigned variant = 0; variant < 8; ++variant) {
        unsigned rotation = (variant % 4) * 90;
        struct fake_lcd panel;
        init_panel(&panel);
        panel.psram_direct = variant >= 4; /* Authoritative direct path vs staging. */
        panel.physical_width = (variant & 1) ? 4 : 8;
        panel.physical_height = (variant & 1) ? 8 : 4;
        esp_display_presenter_config_t cfg = io_config(&panel);
        cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
        cfg.target.hw.te_enabled = true;
        cfg.target.hw.te_sync = (esp_display_present_te_sync_config_t) {
            .gpio_num = 6, .bus_freq_hz = 80000000, .data_lines = 4
        };
        cfg.target.hw.rotation = rotation;
        cfg.target.drawbuf.te_compose_buffers = variant >= 4 ? 2 : 1;
        cfg.target.drawbuf.in_psram = variant >= 4;
        cfg.transfer_timeout_ms = 1000;
        cfg.pipeline_acquire_timeout_ms = 1;
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        assert(!test_te); /* GPIO is armed only at prepare, with retryable ownership. */
        start_frame(presenter, false, true);
        assert(test_te);
        immediate = true;
        (void)draw(presenter, 0x1234);
        OK(esp_display_presenter_commit_frame(presenter, NULL));
        assert(esp_display_presenter_quiesce(presenter, 1) == ESP_ERR_TIMEOUT);
        assert(panel.calls == 0); /* Unproven window never sends. */
        pthread_mutex_lock(&test_te->lock);
        test_te->edge = 1000 + variant;
        test_te->allow = true;
        pthread_mutex_unlock(&test_te->lock);
        (void)test_te->wakeup(test_te->user, false);
        OK(esp_display_presenter_quiesce(presenter, 1000));
        assert(panel.calls >= 1);
        for (unsigned i = 0; i < 32; ++i) {
            assert(image[i] == 0x1234);
        }
        esp_display_presenter_frame_status_t status;
        OK(esp_display_presenter_get_frame_status(presenter, &status));
        assert(status.completed_transfer_frame == 1);
        te_destroy_failures = 1;
        uint64_t stop_start = present_now_ms();
        esp_err_t stop_error;
        do {
            stop_error = esp_display_presenter_stop(presenter);
            assert(stop_error == ESP_FAIL || (stop_error == ESP_ERR_INVALID_STATE && !present_expired(stop_start, 1000)));
        } while (stop_error == ESP_ERR_INVALID_STATE);
        assert(test_te); /* Failed GPIO/timer join retains its callback context. */
        delete_eventually(presenter);
        assert(!test_te);
        free_panel(&panel);
    }
}
static void rotation_reference(void)
{
    const esp_display_present_rotation_t rotations[] = {0, 90, 180, 270};
    const esp_display_present_mode_t modes[] = {
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL, ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL,
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL,
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT, ESP_DISPLAY_PRESENT_MODE_AUTO
    };
    for (unsigned variant = 0; variant < 2 * sizeof(modes) / sizeof(modes[0]); ++variant) {
        unsigned mode = variant % (sizeof(modes) / sizeof(modes[0]));
        bool rgb = variant >= sizeof(modes) / sizeof(modes[0]);
        for (unsigned r = 0; r < 4; ++r) {
            struct fake_lcd panel;
            init_panel(&panel);
            panel.physical_width = r & 1 ? 4 : 8;
            panel.physical_height = r & 1 ? 8 : 4;
            esp_display_presenter_config_t cfg = config(&panel, rgb);
            cfg.target.fb.mode = modes[mode];
            cfg.target.hw.rotation = rotations[r];
            uint8_t buffers;
            OK(esp_display_present_get_required_frame_buffer_count(&cfg.target, &buffers));
            assert(buffers == (modes[mode] == ESP_DISPLAY_PRESENT_MODE_TRIPLE_PARTIAL ||
                               modes[mode] == ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL ||
                               modes[mode] == ESP_DISPLAY_PRESENT_MODE_AUTO ? 3 : 2));
            cfg.pipeline_acquire_timeout_ms = 1;
            esp_display_presenter_t *presenter;
            OK(esp_display_presenter_create(&cfg, &presenter));
            size_t count;
            bool full;
            assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
            boundary(&panel);
            boundary(&panel);
            OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
            assert(full && !count);
            esp_display_presenter_buffer_t buffer;
            OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
            assert(buffer.surface.width == 8 && buffer.surface.height == 4);
            uint16_t expected[32] = {0};
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 8; ++x) {
                    unsigned index = y * 8 + x;
                    uint16_t pixel = 0x100 + index;
                    ((uint16_t *)buffer.surface.pixels)[index] = pixel;
                    esp_display_present_point_t logical = {x, y}, physical;
                    OK(esp_display_present_geometry_map_logical_point_to_physical(rotations[r],
                    (esp_display_present_size_t) {
                        8, 4
                    }, &logical, &physical));
                    expected[physical.y * panel.physical_width + physical.x] = pixel;
                    int32_t px = physical.x, py = physical.y;
                    OK(esp_display_presenter_map_point(presenter, &px, &py));
                    assert(px == x && py == y);
                }
            }
            esp_display_present_point_t outside = {-1, -1}, mapped;
            OK(esp_display_present_geometry_map_physical_point_to_logical(rotations[r],
            (esp_display_present_size_t) {
                8, 4
            }, &outside, &mapped));
            int32_t x = -1, y = -1;
            OK(esp_display_presenter_map_point(presenter, &x, &y));
            assert(x == mapped.x && y == mapped.y);
            esp_display_present_area_t area = {0, 0, 7, 3};
            OK(esp_display_presenter_submit_buffer(presenter, &buffer, &area, 16));
            OK(esp_display_presenter_commit_frame(presenter, NULL));
            finish_scanout(&panel, presenter, 2);
            OK(esp_display_presenter_quiesce(presenter, 100));
            assert(!memcmp(expected, panel.source, sizeof(expected)));
            esp_display_presenter_frame_status_t status;
            OK(esp_display_presenter_get_frame_status(presenter, &status));
            if (!r && (modes[mode] == ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL ||
                       modes[mode] == ESP_DISPLAY_PRESENT_MODE_TRIPLE_FULL ||
                       modes[mode] == ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT)) {
                /* Converting a direct physical pool into a logical canvas may
                 * allocate. Failure must preserve generation, axes and panel. */
                allocation_kind = 1;
                allocation_index = 0;
                allocation_target = 1;
                esp_err_t error = esp_display_presenter_set_rotation(presenter, ESP_DISPLAY_PRESENT_ROTATE_90);
                allocation_target = 0;
                assert(error == ESP_ERR_NO_MEM);
                esp_display_presenter_caps_t retained;
                OK(esp_display_presenter_get_caps(presenter, &retained));
                assert(retained.width == 8 && retained.height == 4);
                esp_display_presenter_frame_status_t retained_status;
                OK(esp_display_presenter_get_frame_status(presenter, &retained_status));
                assert(retained_status.generation == status.generation);
            }
            OK(esp_display_presenter_set_rotation(presenter, rotations[(r + 1) % 4]));
            /* Panel geometry never changes when the logical view rotates. */
            esp_display_presenter_caps_t rotated_caps;
            OK(esp_display_presenter_get_caps(presenter, &rotated_caps));
            assert(rotated_caps.width == 4 && rotated_caps.height == 8);
            OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
            assert(full && !count);
            OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
            assert(buffer.surface.width == 4 && buffer.surface.height == 8);
            for (int cy = 0; cy < 8; ++cy) {
                for (int cx = 0; cx < 4; ++cx) {
                    uint16_t pixel = 0x200 + cy * 4 + cx;
                    ((uint16_t *)buffer.surface.pixels)[cy * 4 + cx] = pixel;
                    esp_display_present_point_t logical = {cx, cy}, physical;
                    OK(esp_display_present_geometry_map_logical_point_to_physical(rotations[(r + 1) % 4],
                    (esp_display_present_size_t) {
                        4, 8
                    }, &logical, &physical));
                    expected[physical.y * panel.physical_width + physical.x] = pixel;
                }
            }
            esp_display_present_area_t rotated_area = {0, 0, 3, 7};
            OK(esp_display_presenter_submit_buffer(presenter, &buffer, &rotated_area, 8));
            OK(esp_display_presenter_commit_frame(presenter, NULL));
            finish_scanout(&panel, presenter, 3);
            OK(esp_display_presenter_quiesce(presenter, 100));
            assert(!memcmp(expected, panel.source, sizeof(expected)));
            esp_display_presenter_frame_status_t unchanged;
            OK(esp_display_presenter_get_frame_status(presenter, &unchanged));
            assert(status.generation + 1 == unchanged.generation);
            esp_display_presenter_caps_t caps;
            OK(esp_display_presenter_get_caps(presenter, &caps));
            assert(caps.width == 4 && caps.height == 8);
            /* A second quarter-turn restores the original logical shape. */
            OK(esp_display_presenter_set_rotation(presenter, rotations[(r + 2) % 4]));
            OK(esp_display_presenter_get_caps(presenter, &caps));
            assert(caps.width == 8 && caps.height == 4);
            OK(esp_display_presenter_get_frame_status(presenter, &unchanged));
            assert(unchanged.generation == status.generation + 2);
            OK(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full));
            assert(full && !count);
            OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
            memset(expected, 0, sizeof(expected));
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 8; ++px) {
                    uint16_t pixel = 0x300 + py * 8 + px;
                    ((uint16_t *)buffer.surface.pixels)[py * 8 + px] = pixel;
                    esp_display_present_point_t logical = {px, py}, physical;
                    OK(esp_display_present_geometry_map_logical_point_to_physical(rotations[(r + 2) % 4],
                    (esp_display_present_size_t) {
                        8, 4
                    }, &logical, &physical));
                    expected[physical.y * panel.physical_width + physical.x] = pixel;
                }
            }
            OK(esp_display_presenter_submit_buffer(presenter, &buffer, &area, 16));
            OK(esp_display_presenter_commit_frame(presenter, NULL));
            finish_scanout(&panel, presenter, 4);
            OK(esp_display_presenter_quiesce(presenter, 100));
            assert(!memcmp(expected, panel.source, sizeof(expected)));
            delete_eventually(presenter);
            free_panel(&panel);
        }
    }
}
#ifdef LEGACY_WITH_LVGL
static void count_legacy_flush(lv_event_t *event)
{
    unsigned *count = lv_event_get_user_data(event);
    ++*count;
}

static void legacy_fb_pixels(const uint16_t *pixels, bool changed)
{
    for (unsigned y = 0; y < 4; ++y) {
        for (unsigned x = 0; x < 8; ++x) {
            uint16_t expected = 0xffff;
            if (changed && x >= 1 && x < 3 && y < 2) {
                expected = 0xf800;
            }
            if (changed && x >= 5 && x < 7 && y >= 2) {
                expected = 0x001f;
            }
            assert(pixels[y * 8 + x] == expected);
        }
    }
}

static void legacy_fb_box(lv_obj_t *screen, int x, int y, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(screen);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, 2, 2);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
}

/* Exercise the stable API and delayed scanout, not just the native binding. */
static void legacy_lvgl_framebuffers(void)
{
    const esp_display_present_mode_t modes[] = {
        ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL, ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT
    };
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        struct fake_lcd panel;
        init_panel(&panel);
        esp_display_presenter_config_t cfg = config(&panel, false);
        cfg.target.fb.mode = modes[i];
        cfg.pipeline_acquire_timeout_ms = 1;
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        lv_display_t *display = NULL;
        assert(esp_lv_present_start(presenter, &display) == ESP_ERR_TIMEOUT);
        assert(!display && panel.calls == 1);
        boundary(&panel);
        boundary(&panel);
        OK(esp_lv_present_start(presenter, &display));
        unsigned flushes = 0;
        lv_display_add_event_cb(display, count_legacy_flush, LV_EVENT_FLUSH_START, &flushes);
        lv_obj_t *screen = lv_display_get_screen_active(display);
        lv_obj_remove_style_all(screen);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_refr_now(display);
        assert(esp_lv_present_get_frame_count() == 1);
        finish_scanout(&panel, presenter, 2);
        OK(esp_display_presenter_quiesce(presenter, 1000));
        const uint16_t *old = panel.source;
        assert(old);
        legacy_fb_pixels(old, false);
        uint16_t snapshot[32];
        memcpy(snapshot, old, sizeof(snapshot));
        legacy_fb_box(screen, 1, 0, 0xff0000);
        legacy_fb_box(screen, 5, 2, 0x0000ff);
        flushes = 0;
        lv_refr_now(display);
        assert(flushes >= (modes[i] == ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT ? 2u : 1u));
        assert(esp_lv_present_get_frame_count() == 2);
        pthread_mutex_lock(&panel.lock);
        while (panel.calls < 3) {
            pthread_cond_wait(&panel.changed, &panel.lock);
        }
        const uint16_t *new_pixels = panel.source;
        pthread_mutex_unlock(&panel.lock);
        assert(new_pixels && new_pixels != old);
        assert(!memcmp(old, snapshot, sizeof(snapshot)));
        assert(esp_display_presenter_quiesce(presenter, 1) == ESP_ERR_TIMEOUT);
        OK(esp_lv_present_stop()); /* The submitted FB outlives the binding. */
        assert(!memcmp(old, snapshot, sizeof(snapshot)));
        finish_scanout(&panel, presenter, 3);
        OK(esp_display_presenter_quiesce(presenter, 1000));
        legacy_fb_pixels(new_pixels, true);
        delete_eventually(presenter);
        free_panel(&panel);
        lv_deinit();
    }
}

static void lvgl_handoff(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    cfg.transfer_timeout_ms = 1000;
    esp_display_presenter_t *p;
    OK(esp_display_presenter_create(&cfg, &p));
    immediate = true;
    start_frame(p, false, true);
    (void)draw(p, 0x1234);
    OK(esp_display_presenter_commit_frame(p, NULL));
    lv_display_t *display;
    OK(esp_lv_present_start(p, &display));
    assert(display && esp_lv_present_start(p, &display) == ESP_ERR_INVALID_STATE);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    immediate = false;
    lv_refr_now(NULL);
    assert(esp_lv_present_get_frame_count() == 1);
    assert(esp_display_presenter_quiesce(p, 1) == ESP_ERR_TIMEOUT);
    assert(esp_lv_present_get_frame_count() == 1); /* Binding survives the drain timeout. */
    assert(panel.io && panel.source);
    (void)panel.io(&panel, NULL, panel.user);
    OK(esp_display_presenter_quiesce(p, 100)); /* Still bound, no open raster. */
    immediate = true;
    for (unsigned i = 0; i < 32; ++i) {
        assert(image[i] == 0xf800);
    }
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(status.last_submitted_frame == 2 && status.completed_transfer_frame == 2 && status.completed_present_frame == 2);
    assert(esp_display_presenter_rebind_producer(p) == ESP_ERR_INVALID_STATE);
    OK(esp_lv_present_stop());
    assert(!esp_lv_present_get_frame_count());
    OK(esp_display_presenter_rebind_producer(p));
    start_frame(p, false, true); /* Old renderer cannot assume LVGL's baseline. */
    (void)draw(p, 0x2222);
    esp_display_presenter_frame_receipt_t receipt;
    OK(esp_display_presenter_commit_frame_with_receipt(p, NULL, false, &receipt));
    assert(receipt.frame_id == 3);
    delete_eventually(p);
    free_panel(&panel);
    /* A 5-pixel RGB565 row is not LVGL's 4-byte stride. Zero-copy admission
     * must choose the safe copy binding, not fail or alias padded rows. */
    init_panel(&panel);
    cfg = io_config(&panel);
    cfg.width = 5;
    OK(esp_display_presenter_create(&cfg, &p));
    memset(image, 0, sizeof(image));
    OK(esp_lv_present_start(p, &display));
    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x00ff00), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(NULL);
    assert(esp_lv_present_get_frame_count() == 1);
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 8; ++col) {
            assert(image[row * 8 + col] == (col < 5 ? 0x07e0 : 0));
        }
    }
    OK(esp_lv_present_stop());
    delete_eventually(p);
    free_panel(&panel);
    lv_deinit();
}
#endif
static void large_damage_capacity(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t cfg = io_config(&panel);
    cfg.max_damage_areas = 128; /* GSP 1.5.1's actual default. */
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&cfg, &presenter));
    esp_display_presenter_caps_t caps;
    OK(esp_display_presenter_get_caps(presenter, &caps));
    assert(caps.max_damage_areas == 128);
    esp_display_present_area_t damage[128], output[128];
    for (size_t i = 0; i < 128; ++i) {
        damage[i] = (esp_display_present_area_t) {
            0, 0, 1, 1
        };
    }
    esp_display_present_surface_request_t request = {.dirty_areas = damage, .dirty_area_count = 128};
    size_t count;
    bool full;
    damage[127].x2 = 8;
    assert(esp_display_presenter_begin_next_frame(presenter, &request, output, 128, &count, &full) == ESP_ERR_INVALID_ARG);
    damage[127].x2 = 1;
    OK(esp_display_presenter_begin_next_frame(presenter, &request, output, 128, &count, &full));
    assert(full && count == 0);
    immediate = true;
    (void)draw(presenter, 0x1234);
    esp_display_presenter_submit_t submit = {.coverage = ESP_DISPLAY_PRESENT_COVERAGE_AREAS,
                                             .areas = damage, .area_count = 128
                                            };
    damage[127].y2 = 4;
    assert(esp_display_presenter_commit_frame(presenter, &submit) == ESP_ERR_INVALID_ARG);
    damage[127].y2 = 1;
    OK(esp_display_presenter_commit_frame(presenter, &submit));
    for (size_t i = 0; i < 32; ++i) {
        assert(image[i] == 0x1234);
    }
    OK(esp_display_presenter_begin_next_frame(presenter, &request, output, 128, &count, &full));
    assert(full && count == 0); /* Still bounded after the first frame. */
    esp_display_presenter_cancel_frame(presenter);
    delete_eventually(presenter);
    free_panel(&panel);
}
static void stop_unsealed(void)
{
    for (unsigned kind = 0; kind < 3; ++kind) {
        for (unsigned submitted = 0; submitted < 2; ++submitted) {
            struct fake_lcd panel;
            init_panel(&panel);
            esp_display_presenter_config_t cfg = kind == 2 ? config(&panel, false) : io_config(&panel);
            cfg.pipeline_acquire_timeout_ms = 100;
            if (kind == 1) {
                cfg.target.hw.te_enabled = true;
                cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
                cfg.target.hw.te_sync = (esp_display_present_te_sync_config_t) {
                    .gpio_num = 1, .bus_freq_hz = 8000000
                };
            }
            esp_display_presenter_t *presenter;
            OK(esp_display_presenter_create(&cfg, &presenter));
            if (kind == 2) {
                size_t count;
                bool full;
                assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_TIMEOUT);
                boundary(&panel);
                boundary(&panel);
            }
            start_frame(presenter, false, true);
            if (submitted) {
                immediate = true;
                (void)draw(presenter, 0x1234);
            }
            OK(esp_display_presenter_stop(presenter));
            esp_display_present_fault_reason_t reason;
            OK(esp_display_presenter_get_fault_reason(presenter, &reason));
            assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
            OK(esp_display_presenter_stop(presenter));
            OK(esp_display_presenter_delete(presenter));
            free_panel(&panel);
        }
    }
}
static esp_display_presenter_config_t lifecycle_config(struct fake_lcd *panel, unsigned profile)
{
    esp_display_presenter_config_t cfg = config(panel, profile == 3);
    cfg.target.drawbuf.buffers = 2;
    switch (profile) {
    case 0: /* Synchronous internal GRAM. */
    case 1: /* PSRAM GRAM with independent internal TX staging. */
    case 2: /* Rotated asynchronous GRAM. */
    case 5: /* TE: composed image, tile, copy worker and gate. */
        cfg.target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
        cfg.target.hw.io = panel;
        cfg.target.fb.mode = profile == 5 ? ESP_DISPLAY_PRESENT_MODE_TE_SYNC : ESP_DISPLAY_PRESENT_MODE_NONE;
        cfg.target.drawbuf.in_psram = profile == 1;
        if (profile == 2) {
            cfg.width = 4;
            cfg.height = 8;
            cfg.target.hw.rotation = ESP_DISPLAY_PRESENT_ROTATE_90;
        }
        if (profile == 5) {
            cfg.target.hw.te_enabled = true;
            cfg.target.hw.te_sync = (esp_display_present_te_sync_config_t) {
                .gpio_num = 1, .bus_freq_hz = 8000000
            };
            cfg.target.drawbuf.te_compose_buffers = 2;
        }
        break;
    case 3:
        cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_DOUBLE_FULL;
        break;
    case 4:
        cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_DOUBLE_DIRECT;
        break;
    }
    return cfg;
}
static void create_allocation_rollback(void)
{
    unsigned failures = 0;
    unavailable = true;
    for (unsigned profile = 0; profile < 6; ++profile) {
        for (unsigned kind = 0; kind < 5; ++kind) {
            bool complete = false;
            for (unsigned target = 1; target < 128; ++target) {
                struct fake_lcd panel;
                init_panel(&panel);
                esp_display_presenter_config_t cfg = lifecycle_config(&panel, profile);
                esp_display_presenter_t *p = NULL;
                allocation_index = 0;
                allocation_kind = kind;
                allocation_target = target;
                esp_err_t error = esp_display_presenter_create(&cfg, &p);
                unsigned reached = allocation_index;
                allocation_target = 0;
                if (error == ESP_OK) {
                    /* The first target beyond the allocation sequence succeeds. */
                    assert(reached < target && p);
                    OK(esp_display_presenter_stop(p));
                    OK(esp_display_presenter_stop(p));
                    OK(esp_display_presenter_delete(p));
                    complete = true;
                } else {
                    ++failures;
                    assert(error == ESP_ERR_NO_MEM && !p);
                    /* A failed create must not poison reuse of the same panel. */
                    assert(!panel.user && !panel.calls && !panel.deleted);
                    OK(esp_display_presenter_create(&cfg, &p));
                    OK(esp_display_presenter_delete(p));
                }
                assert(!panel.user && !panel.calls && !panel.deleted);
                free_panel(&panel);
                if (complete) {
                    break;
                }
            }
            assert(complete);
        }
    }
    printf("lifecycle: %u failed allocation/resource boundaries rolled back across 6 public configurations\n", failures);
}
static void delete_after_partial_resource_cleanup(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    unavailable = false;
    esp_display_presenter_config_t cfg = lifecycle_config(&panel, 4);
    esp_display_presenter_t *p;
    OK(esp_display_presenter_create(&cfg, &p));
    /* Stop/detach succeeds and engine is destroyed; executor teardown fails. */
    destroy_failure = 1;
    assert(esp_display_presenter_delete(p) == ESP_FAIL);
    assert(!panel.user && !panel.deleted);
    esp_display_presenter_state_t state;
    OK(esp_display_presenter_get_state(p, &state));
    assert(state == ESP_DISPLAY_PRESENTER_STATE_STOPPED);
    esp_display_presenter_frame_status_t status;
    OK(esp_display_presenter_get_frame_status(p, &status));
    assert(!status.accepting_callbacks);
    OK(esp_display_presenter_stop(p));
    OK(esp_display_presenter_delete(p));
    unavailable = true;
    free_panel(&panel);
}

static void public_lease_identity(void)
{
    struct fake_lcd panel;
    init_panel(&panel);
    esp_display_presenter_config_t input = config(&panel, false);
    input.target.hw.panel_type = ESP_DISPLAY_PRESENT_PANEL_IO;
    input.target.hw.io = &panel;
    input.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_NONE;
    input.target.drawbuf.buffers = 1;
    immediate = true;
    esp_display_presenter_t *presenter;
    OK(esp_display_presenter_create(&input, &presenter));
    start_frame(presenter, false, true);
    esp_display_presenter_buffer_t buffer;
    OK(esp_display_presenter_acquire_buffer(presenter, &buffer));
    memset(buffer.surface.pixels, 0x4a, buffer.capacity_bytes);
    esp_display_presenter_buffer_t forged = buffer;
    forged.surface.pixels = (uint8_t *)buffer.surface.pixels + 2;
    esp_display_present_area_t bad = {-1, 0, 7, 3}, area = {0, 0, 7, 3};
    assert(esp_display_presenter_submit_buffer(presenter, &forged, &bad, 16) == ESP_ERR_INVALID_STATE);
    assert(esp_display_presenter_submit_buffer(presenter, &buffer, &bad, 16) == ESP_ERR_INVALID_ARG);
    assert(esp_display_presenter_submit_buffer(presenter, &buffer, &area, 0) == ESP_ERR_INVALID_ARG);
    OK(esp_display_presenter_submit_buffer(presenter, &buffer, &area, 16));
    esp_display_presenter_buffer_t next;
    OK(esp_display_presenter_acquire_buffer(presenter, &next));
    assert(next.surface.pixels == buffer.surface.pixels && next.lease_id != buffer.lease_id);
    size_t rows = 99;
    assert(buffer.resolve_rows(buffer.resolve_rows_ctx, buffer.lease_id, 16, 4, &rows) == ESP_ERR_INVALID_STATE && !rows);
    assert(esp_display_presenter_submit_buffer(presenter, &buffer, &area, 16) == ESP_ERR_INVALID_STATE);
    OK(next.resolve_rows(next.resolve_rows_ctx, next.lease_id, 16, 4, &rows));
    esp_display_presenter_cancel_frame(presenter);
    assert(next.resolve_rows(next.resolve_rows_ctx, next.lease_id, 16, 4, &rows) == ESP_ERR_INVALID_STATE);
    esp_display_present_fault_reason_t reason;
    OK(esp_display_presenter_get_fault_reason(presenter, &reason));
    assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
    delete_eventually(presenter);
    free_panel(&panel);
}

/* A failed cross-task handoff must leave both layers with the old producer. */
struct handoff_attempt {
    esp_display_presenter_t *presenter;
    esp_err_t error;
    bool accepted;
};
static void *try_handoff(void *user)
{
    struct handoff_attempt *attempt = user;
    esp_display_present_engine_t *engine = (void *)1;
    attempt->error = present_session_bind(attempt->presenter, &engine);
    if (attempt->error == ESP_OK) {
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(engine, &status));
        attempt->accepted = true;
        present_session_unbind(attempt->presenter);
    } else {
        assert(!engine);
    }
    return NULL;
}
static void session_handoff_failures(void)
{
    for (unsigned variant = 0; variant < 3; ++variant) {
        struct fake_lcd panel;
        init_panel(&panel);
        esp_display_presenter_config_t cfg = io_config(&panel);
        cfg.pipeline_acquire_timeout_ms = 2;
        cfg.transfer_timeout_ms = 10000;
        if (variant == 0) {
            cfg.target.fb.mode = ESP_DISPLAY_PRESENT_MODE_TE_SYNC;
            cfg.target.hw.te_enabled = true;
            cfg.target.hw.te_sync = (esp_display_present_te_sync_config_t) {
                .gpio_num = 6, .bus_freq_hz = 80000000, .data_lines = 4
            };
        }
        esp_display_presenter_t *presenter;
        OK(esp_display_presenter_create(&cfg, &presenter));
        if (variant == 0) {
            te_create_failures = 1;
        } else if (variant == 1) {
            immediate = false;
            start_frame(presenter, false, true);
            (void)draw(presenter, 0x4567);
            OK(esp_display_presenter_commit_frame(presenter, NULL));
        }
        struct handoff_attempt attempt = {.presenter = presenter};
        pthread_t thread;
        assert(!pthread_create(&thread, NULL, try_handoff, &attempt));
        assert(!pthread_join(thread, NULL));
        if (variant == 2) {
            assert(attempt.error == ESP_OK && attempt.accepted);
            size_t count;
            bool full;
            assert(esp_display_presenter_begin_next_frame(presenter, NULL, NULL, 0, &count, &full) == ESP_ERR_INVALID_STATE);
            /* Successful unbind retains the new producer until explicit rebind. */
            OK(esp_display_presenter_rebind_producer(presenter));
        } else {
            assert(attempt.error == (variant == 0 ? ESP_ERR_NO_MEM : ESP_ERR_TIMEOUT));
            assert(!attempt.accepted);
            if (variant == 1) {
                assert(panel.io && panel.source);
                for (unsigned i = 0; i < 32; ++i) {
                    assert(((const uint16_t *)panel.source)[i] == 0x4567);
                }
                (void)panel.io(&panel, NULL, panel.user);
            }
            esp_display_present_fault_reason_t reason;
            OK(esp_display_presenter_get_fault_reason(presenter, &reason));
            assert(reason == ESP_DISPLAY_PRESENT_FAULT_NONE);
        }
        immediate = true;
        start_frame(presenter, false, variant != 1);
        esp_display_presenter_cancel_frame(presenter);
        delete_eventually(presenter);
        free_panel(&panel);
    }
}

/* Test-only rendezvous: STOP must reach the facade mutex while acquire is
 * inside its event wait. No sleeps establish ordering and no driver is blocked. */
static struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool enabled, waiting, stop_ready, stop_attempted;
    present_task_t producer, stopper;
} gram_race = { .lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER };

static void gram_wait_flag(const bool *flag)
{
    struct timespec deadline;
    assert(clock_gettime(CLOCK_REALTIME, &deadline) == 0);
    deadline.tv_sec += 2;
    while (!*flag) {
        assert(pthread_cond_timedwait(&gram_race.changed, &gram_race.lock, &deadline) == 0);
    }
}

bool __real_present_event_wait(present_os_event_t *, uint32_t);
bool __wrap_present_event_wait(present_os_event_t *event, uint32_t timeout)
{
    pthread_mutex_lock(&gram_race.lock);
    if (gram_race.enabled && present_same_task(gram_race.producer, present_current_task())) {
        gram_race.waiting = true;
        pthread_cond_broadcast(&gram_race.changed);
        gram_wait_flag(&gram_race.stop_attempted);
    }
    pthread_mutex_unlock(&gram_race.lock);
    return __real_present_event_wait(event, timeout);
}

bool __real_present_mutex_take(present_mutex_t *, uint32_t);
bool __wrap_present_mutex_take(present_mutex_t *mutex, uint32_t timeout)
{
    pthread_mutex_lock(&gram_race.lock);
    if (gram_race.enabled && gram_race.stop_ready &&
            present_same_task(gram_race.stopper, present_current_task())) {
        gram_race.stop_attempted = true;
        pthread_cond_broadcast(&gram_race.changed);
    }
    pthread_mutex_unlock(&gram_race.lock);
    return __real_present_mutex_take(mutex, timeout);
}

typedef struct {
    esp_display_presenter_t *presenter;
    esp_err_t result;
} gram_stop_attempt_t;

static void *stop_during_gram_acquire(void *user)
{
    gram_stop_attempt_t *attempt = user;
    pthread_mutex_lock(&gram_race.lock);
    gram_wait_flag(&gram_race.waiting);
    gram_race.stopper = present_current_task();
    gram_race.stop_ready = true;
    pthread_mutex_unlock(&gram_race.lock);
    attempt->result = esp_display_presenter_stop(attempt->presenter);
    return NULL;
}

/* Public API, unsealed frame, every tile still borrowed by accepted DMA.
 * An acquire-budget expiry is retryable; a physical-transfer expiry is sticky.
 * Both must leave the source and callback registration alive during close retries. */
static void gram_exhaustion_and_stop(void)
{
    for (unsigned tiles = 1; tiles <= 2; ++tiles) {
        for (unsigned transfer_fault = 0; transfer_fault < 2; ++transfer_fault) {
            for (unsigned concurrent_stop = 0; concurrent_stop < 2; ++concurrent_stop) {
                struct fake_lcd panel;
                init_panel(&panel);
                esp_display_presenter_config_t cfg = io_config(&panel);
                cfg.target.drawbuf.buffers = tiles;
                cfg.pipeline_acquire_timeout_ms = transfer_fault ? 200 : 5;
                cfg.transfer_timeout_ms = transfer_fault ? 20 : 60000;
                esp_display_presenter_t *presenter;
                OK(esp_display_presenter_create(&cfg, &presenter));
                immediate = false;
                start_frame(presenter, false, true);
                const uint16_t *sources[2];
                esp_display_presenter_buffer_t old[2];
                for (unsigned i = 0; i < tiles; ++i) {
                    old[i] = draw(presenter, 0x6a00 + i);
                    sources[i] = panel.source;
                }
                assert(panel.calls == tiles);
                if (tiles == 2) {
                    assert(sources[0] != sources[1]);
                }
                gram_stop_attempt_t attempt = {.presenter = presenter};
                pthread_t thread;
                if (concurrent_stop) {
                    pthread_mutex_lock(&gram_race.lock);
                    gram_race.producer = present_current_task();
                    gram_race.waiting = gram_race.stop_ready = gram_race.stop_attempted = false;
                    gram_race.enabled = true;
                    pthread_mutex_unlock(&gram_race.lock);
                    assert(!pthread_create(&thread, NULL, stop_during_gram_acquire, &attempt));
                }
                esp_display_presenter_buffer_t unused;
                memset(&unused, 0xa5, sizeof(unused));
                assert(esp_display_presenter_acquire_buffer(presenter, &unused) == ESP_ERR_TIMEOUT);
                assert(!unused.lease_id && !unused.surface.pixels && !unused.capacity_bytes);
                if (concurrent_stop) {
                    assert(!pthread_join(thread, NULL));
                    pthread_mutex_lock(&gram_race.lock);
                    assert(gram_race.waiting && gram_race.stop_attempted);
                    gram_race.enabled = false;
                    pthread_mutex_unlock(&gram_race.lock);
                    assert(attempt.result == ESP_ERR_INVALID_STATE);
                } else {
                    esp_display_presenter_state_t state;
                    OK(esp_display_presenter_get_state(presenter, &state));
                    assert(state == (transfer_fault ? ESP_DISPLAY_PRESENTER_STATE_FAULTED : ESP_DISPLAY_PRESENTER_STATE_OPEN));
                }
                esp_display_present_fault_reason_t reason;
                OK(esp_display_presenter_get_fault_reason(presenter, &reason));
                assert(reason == (transfer_fault ? ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT : ESP_DISPLAY_PRESENT_FAULT_NONE));
                esp_display_presenter_cancel_frame(presenter);
                esp_display_presenter_cancel_frame(presenter);
                const esp_display_present_area_t area = {0, 0, 7, 3};
                assert(esp_display_presenter_submit_buffer(presenter, &old[0], &area, 16) == ESP_ERR_INVALID_STATE);
                assert(esp_display_presenter_stop(presenter) == ESP_ERR_INVALID_STATE);
                assert(esp_display_presenter_delete(presenter) == ESP_ERR_INVALID_STATE);
                assert(panel.io && panel.source && panel.calls == tiles && !panel.detached);
                for (unsigned i = 0; i < tiles; ++i) {
                    for (unsigned pixel = 0; pixel < 32; ++pixel) {
                        assert(sources[i][pixel] == 0x6a00 + i);
                    }
                }
                /* Only genuine callbacks permit source retirement. They cannot
                 * clear a timeout or cause cancel/stop to submit another band. */
                for (unsigned i = 0; i < tiles; ++i) {
                    (void)panel.io(&panel, NULL, panel.user);
                }
                OK(esp_display_presenter_get_fault_reason(presenter, &reason));
                assert(reason == (transfer_fault ? ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT : ESP_DISPLAY_PRESENT_FAULT_NONE));
                assert(panel.calls == tiles);
                delete_eventually(presenter);
                free_panel(&panel);
            }
        }
    }
}

int main(void)
{
    gram_exhaustion_and_stop();
    session_handoff_failures();
    create_allocation_rollback();
    delete_after_partial_resource_cleanup();
    stop_unsealed();
    large_damage_capacity();
    leases_and_stop();
    public_lease_identity();
    rotation_and_fault();
    cancel_pending_copy();
    cancel_before_core_gate();
    cancel_pending_transfer();
    framebuffer_tracking();
    tracking_early_terminal();
    faulted_tracking_snapshot();
    fault_classification_matrix();
    fault_after_tracking_fence();
    mode_matrix();
    frame_plan_contracts();
    rotation_reference();
    direct_dirty_contract();
    gram_rotation_superset();
    te_compatibility();
    resolve_alignment_contract();
#ifdef LEGACY_WITH_LVGL
    lvgl_handoff();
    legacy_lvgl_framebuffers();
#endif
    puts("legacy: lease identity, control task, close/cancel, deadline, rotation, scroll and handoff passed");
}
