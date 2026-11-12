/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "platform/lcd/present_lcd_fb_scanout.h"
#include "platform/lcd/present_lcd_rgb_info.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_cache.h"
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define OK(x) assert((x)==ESP_OK)
/* Model an ISR that fetched callback/user before unregister but had not
 * entered the adapter. active_callbacks cannot see it; the native join must. */
static dpi_cb_t late_dsi;
static rgb_cb_t late_rgb;
static void *late_user;
static esp_lcd_panel_handle_t late_panel;
static bool late_joined;
static unsigned barrier_calls;
static esp_err_t barrier_result = ESP_OK;
static struct fake_lcd *submit_barrier_panel;
static void boundary(struct fake_lcd *p);
esp_err_t __real_present_isr_barrier(uint32_t timeout);
esp_err_t __wrap_present_isr_barrier(uint32_t timeout)
{
    ++barrier_calls;
    if (submit_barrier_panel) {
        struct fake_lcd *panel = submit_barrier_panel;
        submit_barrier_panel = NULL;
        boundary(panel); /* ISR that sampled the old FB before submission. */
    }
    if (barrier_result != ESP_OK) {
        return barrier_result;
    }
    if (late_dsi) {
        (void)late_dsi(late_panel, NULL, late_user);
        late_dsi = NULL;
        late_joined = true;
    }
    if (late_rgb) {
        (void)late_rgb(late_panel, NULL, late_user);
        late_rgb = NULL;
        late_joined = true;
    }
    return __real_present_isr_barrier(timeout);
}
struct fake_lcd {
    void *buffers[3];
    dpi_cb_t dsi, dsi_vsync;
    rgb_cb_t rgb, rgb_vsync;
    void *user;
    bool early, fail, stop_timeout, deleted, detach_fail;
    bool link_early, board_info;
    unsigned board_queries;
    unsigned *link_published;
    size_t calls, buffer_count;
    bool no_bounce, no_retirement, link_retirement, one_shot, transformed, custom_bounce, one_fb;
    bool info_error, wrong_dimensions, wrong_format, wrong_size;
    const void *source;
};
esp_err_t present_lcd_rgb_get_board_info(esp_lcd_panel_handle_t panel, present_lcd_rgb_info_t *info)
{
    ++panel->board_queries;
    if (!panel->board_info) {
        return ESP_ERR_NOT_FOUND;
    }
    if (panel->info_error) {
        return ESP_FAIL;
    }
    *info = (present_lcd_rgb_info_t) {
        .width = panel->wrong_dimensions ? 2 : 3, .height = panel->wrong_dimensions ? 3 : 2,
        .frame_buffer_count = panel->one_fb ? 1 : (panel->buffer_count ? panel->buffer_count : 2),
        .frame_buffer_bytes = panel->wrong_size ? 10 : 12,
        .bits_per_pixel = panel->wrong_format ? 24 : 16,
        .bounce_buffer_bytes = panel->no_bounce ? 0 : 6,
        .continuous_refresh = !panel->one_shot,
        .custom_bounce_callback = panel->custom_bounce,
        .transformed = panel->transformed,
    };
    return ESP_OK;
}
#ifdef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
esp_err_t esp_lcd_rgb_panel_get_frame_buffer_info(esp_lcd_panel_handle_t panel, esp_lcd_rgb_panel_frame_buffer_info_t *info)
{
    if (panel->info_error) {
        return ESP_FAIL;
    }
    *info = (esp_lcd_rgb_panel_frame_buffer_info_t) {
        .width = panel->wrong_dimensions ? 2 : 3, .height = panel->wrong_dimensions ? 3 : 2,
        .frame_buffer_count = panel->one_fb ? 1 : (panel->buffer_count ? panel->buffer_count : 2),
        .frame_buffer_bytes = panel->wrong_size ? 10 : 12,
        .bits_per_pixel = panel->wrong_format ? 24 : 16,
        .bounce_buffer_bytes = panel->no_bounce ? 0 : 6, .continuous_refresh = !panel->one_shot,
        .frame_buffer_retirement = !panel->no_retirement && (!panel->no_bounce || panel->link_retirement),
        .transformed = panel->transformed,
        .custom_bounce_callback = panel->custom_bounce
    };
    return ESP_OK;
}
#endif
static void boundary(struct fake_lcd *p)
{
    if (p->dsi) {
        (void)p->dsi(p, NULL, p->user);
    }
    if (p->rgb) {
        (void)p->rgb(p, NULL, p->user);
    }
}

esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t p, const esp_lcd_dpi_panel_event_callbacks_t *cb, void *user)
{
    if (!user && p->detach_fail) {
        return ESP_FAIL;
    }
    if (!user && p->dsi) {
        (void)p->dsi(p, NULL, NULL); /* Old callback, newly cleared user_ctx. */
        if (p->dsi_vsync) {
            (void)p->dsi_vsync(p, NULL, NULL);
        }
    }
    p->dsi = cb->on_frame_buf_complete;
    p->dsi_vsync = cb->on_vsync;
    p->user = user;
    assert(!cb->on_color_trans_done);
    return ESP_OK;
}

esp_err_t esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t p, const esp_lcd_rgb_panel_event_callbacks_t *cb, void *user)
{
    if (!user && p->detach_fail) {
        return ESP_FAIL;
    }
    if (!user && p->rgb) {
        (void)p->rgb(p, NULL, NULL);
        if (p->rgb_vsync) {
            (void)p->rgb_vsync(p, NULL, NULL);
        }
    }
    p->rgb = cb->on_frame_buf_complete;
    p->rgb_vsync = cb->on_vsync;
    p->user = user;
    assert(!cb->on_color_trans_done);
    return ESP_OK;
}

static esp_err_t get_buffers(esp_lcd_panel_handle_t p, uint32_t n, void **first, va_list args)
{
    *first = p->buffers[0];
    for (uint32_t i = 1; i < n; ++i) {
        *va_arg(args, void **) = p->buffers[i];
    }
    return ESP_OK;
}

esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t p, uint32_t n, void **first, ...)
{
    va_list args;
    va_start(args, first);
    esp_err_t error = get_buffers(p, n, first, args);
    va_end(args);
    return error;
}

esp_err_t esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t p, uint32_t n, void **first, ...)
{
    va_list args;
    va_start(args, first);
    esp_err_t error = get_buffers(p, n, first, args);
    va_end(args);
    return error;
}

esp_err_t esp_cache_msync(void *p, size_t bytes, int flags)
{
    assert(p && bytes == 12 && (flags & ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    return ESP_OK;
}

esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p, int x, int y, int ex, int ey, const void *source)
{
    assert(x == 0 && y == 0 && ex == 3 && ey == 2);
    p->calls++;
    p->source = source;
    if (p->early) {
        boundary(p);
        boundary(p);
        submit_barrier_panel = p;
    }
    if (p->link_early) {
        assert(!*p->link_published);
        boundary(p);
        boundary(p); /* Coalesce an early duplicate; no publication on failure. */
        assert(!*p->link_published);
    }
    return p->fail ? ESP_FAIL : ESP_OK;
}

esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t p)
{
    if (p->stop_timeout) {
        return ESP_ERR_TIMEOUT;
    }
    assert(!p->deleted); /* Must never double-delete panel on stop retry. */
    p->dsi = NULL;
    p->rgb = NULL;
    p->deleted = true;
    return ESP_OK;
}

static void exercise(esp_display_present_lcd_fb_scanout_kind_t kind, bool fail, esp_err_t join_result, size_t buffer_count)
{
    barrier_result = join_result;
    struct fake_lcd panel = {.early = true, .fail = fail, .buffer_count = buffer_count};
    for (size_t i = 0; i < buffer_count; ++i) {
        panel.buffers[i] = aligned_alloc(64, 64);
        assert(panel.buffers[i]);
    }
    esp_display_present_lcd_fb_scanout_config_t cfg = {.panel = &panel, .kind = kind, .width = 3, .height = 2,
                                                       .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = buffer_count, .bounce = true, .delete_panel = true
                                                      };
    esp_display_present_lcd_fb_scanout_t *a;
    OK(esp_display_present_lcd_fb_scanout_create(&cfg, &a));
    esp_display_present_output_caps_t output_caps;
    OK(esp_display_present_lcd_fb_scanout_get_output_caps(a, &output_caps));
    assert(output_caps.full_only && output_caps.ext_bufs_required && output_caps.max_inflight == 1);
    void *buffers[3];
    size_t count;
    OK(esp_display_present_lcd_fb_scanout_get_buffers(a, buffers, 3, &count));
    assert(count == buffer_count);
    esp_display_present_panel_adapter_t interface;
    OK(esp_display_present_lcd_fb_scanout_get_interface(a, &interface));
    esp_display_present_backend_config_t bc = {.width = 3, .height = 2, .tile_rows = 2, .buffer_count = buffer_count, .max_frames = 2,
                                               .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .render_contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL, .ext_bufs = buffers,
                                               .ext_bytes = 64, .scan_buf = buffers[0], .panel = interface
                                              };
    const present_backend_ops_t *ops = &present_fb_backend;
    present_backend_t *b;
    OK(ops->create(&bc, &b));
    esp_display_present_frame_t f;
    esp_display_present_frame_request_t req = {.full_redraw = true};
    OK(ops->begin(b, &req, &f));
    esp_display_present_lease_t lease;
    OK(ops->acquire(b, f, NULL, 0, &lease));
    assert(lease.surface.pixels == buffers[1]); /* FB0 is still scanning. */
    memset(lease.surface.pixels, 0x55, 12);
    OK(ops->submit_region(b, f, lease.id));
    esp_display_present_receipt_t receipt;
    OK(ops->end(b, f, &receipt));
    OK(present_fb_backend.service(b));
    esp_display_present_frame_status_t status;
    OK(ops->query(b, receipt, &status));
    assert(!status.terminal);
    if (!fail && buffer_count == 2) {
        esp_display_present_frame_t blocked;
        assert(ops->begin(b, &req, &blocked) == ESP_ERR_NO_MEM);
    }
    boundary(&panel);
    OK(ops->query(b, receipt, &status));
    assert(status.terminal == (!fail && join_result == ESP_OK));
    /* Submit-time and join-time callbacks did not confirm this transaction.
     * Without a successful join, one more completion is still required. */
    boundary(&panel);
    OK(ops->query(b, receipt, &status));
    assert(status.terminal == !fail);
    if (!fail) {
        assert(status.present_confirmed);
        OK(ops->release_receipt(b, receipt));
        /* Confirmation retires FB0; it must now be the next writable lease,
         * while the newly scanning FB1 stays borrowed by the driver. */
        OK(ops->begin(b, &req, &f));
        OK(ops->acquire(b, f, NULL, 0, &lease));
        assert(lease.surface.pixels == buffers[0]);
        OK(ops->cancel_frame(b, f, 0));
    }
    panel.stop_timeout = true;
    assert(ops->stop(b, 0) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_lcd_fb_scanout_destroy(a) == ESP_ERR_INVALID_STATE);
    panel.stop_timeout = false;
    OK(ops->stop(b, 0));
    if (fail) {
        OK(ops->query(b, receipt, &status));
        assert(status.terminal && status.result == ESP_FAIL);
        OK(ops->release_receipt(b, receipt));
    }
    OK(ops->destroy(b));
    OK(esp_display_present_lcd_fb_scanout_destroy(a));
    assert(panel.deleted);
    for (size_t i = 0; i < buffer_count; ++i) {
        free(panel.buffers[i]);
    }
    submit_barrier_panel = NULL;
    barrier_result = ESP_OK;
}
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    bool block, entered;
    esp_display_present_panel_event_fn real_cb;
    void *real_user;
} cb_gate_t;

static bool blocking_panel_cb(void *user, const esp_display_present_backend_event_t *event, bool isr)
{
    cb_gate_t *g = user;
    pthread_mutex_lock(&g->lock);
    g->entered = true;
    pthread_cond_broadcast(&g->changed);
    while (g->block) {
        pthread_cond_wait(&g->changed, &g->lock);
    }
    pthread_mutex_unlock(&g->lock);
    return g->real_cb ? g->real_cb(g->real_user, event, isr) : false;
}

static void *boundary_thread(void *arg)
{
    struct fake_lcd *p = arg;
    boundary(p);
    return NULL;
}

static void test_callback_drain_retry(void)
{
    struct fake_lcd panel = {0};
    for (size_t i = 0; i < 2; ++i) {
        panel.buffers[i] = aligned_alloc(64, 64);
        assert(panel.buffers[i]);
    }
    cb_gate_t gate = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER, .block = true};
    esp_display_present_lcd_fb_scanout_config_t cfg = {.panel = &panel, .kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI, .width = 3, .height = 2,
                                                       .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = 2, .delete_panel = true
                                                      };
    esp_display_present_lcd_fb_scanout_t *a;
    OK(esp_display_present_lcd_fb_scanout_create(&cfg, &a));
    void *buffers[2];
    size_t count;
    OK(esp_display_present_lcd_fb_scanout_get_buffers(a, buffers, 2, &count));
    esp_display_present_panel_adapter_t interface;
    OK(esp_display_present_lcd_fb_scanout_get_interface(a, &interface));
    OK(interface.attach(interface.context, blocking_panel_cb, &gate));
    esp_display_present_surface_t surface = {buffers[1], 64, {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}};
    esp_display_present_rect_t area = {0, 0, 3, 2};
    esp_display_present_tx_result_t tx = interface.submit(interface.context, &surface, &area, 42);
    assert(tx.acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED && tx.result == ESP_OK);
    pthread_t th;
    assert(pthread_create(&th, NULL, boundary_thread, &panel) == 0);
    pthread_mutex_lock(&gate.lock);
    while (!gate.entered) {
        pthread_cond_wait(&gate.changed, &gate.lock);
    }
    pthread_mutex_unlock(&gate.lock);
    /* Panel deletes once, then callback drain times out; retry must not call esp_lcd_panel_del again. */
    assert(interface.quiesce(interface.context, 0) == ESP_ERR_TIMEOUT);
    assert(panel.deleted);
    assert(interface.quiesce(interface.context, 0) == ESP_ERR_TIMEOUT);
    pthread_mutex_lock(&gate.lock);
    gate.block = false;
    pthread_cond_broadcast(&gate.changed);
    pthread_mutex_unlock(&gate.lock);
    assert(pthread_join(th, NULL) == 0);
    OK(interface.quiesce(interface.context, UINT32_MAX));
    OK(esp_display_present_lcd_fb_scanout_destroy(a));
    pthread_cond_destroy(&gate.changed);
    pthread_mutex_destroy(&gate.lock);
    for (size_t i = 0; i < 2; ++i) {
        free(panel.buffers[i]);
    }
}

static bool raw_vsync(void *user, uint64_t timestamp, bool isr)
{
    assert(isr && timestamp);
    ++*(unsigned *)user;
    return false;
}
static void test_retained_scanout(void)
{
    for (unsigned rgb = 0; rgb < 2; ++rgb) {
        struct fake_lcd panel = {0};
        for (unsigned i = 0; i < 2; ++i) {
            panel.buffers[i] = aligned_alloc(64, 64);
            assert(panel.buffers[i]);
        }
        unsigned vsyncs = 0;
        esp_display_present_lcd_fb_scanout_config_t config = {
            .panel = &panel, .kind = rgb ? ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB : ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI,
            .width = 3, .height = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = 2,
            .bounce = true, .retain_scanout = true,
            .on_vsync = raw_vsync, .vsync_user = &vsyncs,
        };
        esp_display_present_lcd_fb_scanout_t *adapter;
        config.delete_panel = true;
        assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_INVALID_ARG);
        config.delete_panel = false;
        OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
        esp_display_present_panel_adapter_t interface;
        OK(esp_display_present_lcd_fb_scanout_get_interface(adapter, &interface));
        cb_gate_t gate = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
        OK(interface.attach(interface.context, blocking_panel_cb, &gate));
        if (rgb) {
            (void)panel.rgb_vsync(&panel, NULL, panel.user);
        } else {
            (void)panel.dsi_vsync(&panel, NULL, panel.user);
        }
        assert(vsyncs == 1 && !gate.entered); /* Raw VSYNC is never a retire fence. */
        esp_display_present_surface_t surface = {panel.buffers[1], 64, {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}};
        esp_display_present_rect_t area = {0, 0, 3, 2};
        assert(interface.submit(interface.context, &surface, &area, 42).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
        assert(interface.quiesce(interface.context, 0) == ESP_ERR_TIMEOUT);
        assert(esp_display_present_lcd_fb_scanout_destroy(adapter) == ESP_ERR_INVALID_STATE);
        boundary(&panel);
        /* Completion still drains while retained adapter closes. */
        assert(gate.entered);
        panel.detach_fail = true;
        assert(interface.quiesce(interface.context, 0) == ESP_FAIL);
        panel.detach_fail = false;
        late_dsi = panel.dsi;
        late_rgb = panel.rgb;
        late_user = panel.user;
        late_panel = &panel;
        late_joined = false;
        OK(interface.quiesce(interface.context, 0));
        assert(late_joined); /* Adapter was still alive when the cached ISR ran. */
        assert(!panel.deleted && !panel.rgb && !panel.dsi && panel.source == panel.buffers[1]);
        OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
        pthread_cond_destroy(&gate.changed);
        pthread_mutex_destroy(&gate.lock);
        for (unsigned i = 0; i < 2; ++i) {
            free(panel.buffers[i]);
        }
    }
}

static bool count_single(void *user, const esp_display_present_backend_event_t *event, bool isr)
{
    assert(isr && event->kind == ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED && event->transaction_cookie == 91);
    ++*(unsigned *)user;
    return false;
}
static void single_buffer_join(void)
{
    const esp_err_t outcomes[] = {ESP_OK, ESP_ERR_TIMEOUT, ESP_ERR_NOT_SUPPORTED};
    for (int kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI; kind <= ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB; ++kind) {
        for (unsigned result = 0; result < 3; ++result) {
            struct fake_lcd panel = {.early = true, .one_fb = true};
            panel.buffers[0] = aligned_alloc(64, 64);
            esp_display_present_lcd_fb_scanout_config_t config = {
                .panel = &panel, .kind = kind, .width = 3, .height = 2,
                .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = 1,
                .bounce = true, .delete_panel = true,
            };
            esp_display_present_lcd_fb_scanout_t *adapter;
            OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
            esp_display_present_panel_adapter_t api;
            OK(esp_display_present_lcd_fb_scanout_get_interface(adapter, &api));
            unsigned confirmed = 0;
            OK(api.attach(api.context, count_single, &confirmed));
            esp_display_present_surface_t surface = {panel.buffers[0], 12, {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}};
            esp_display_present_rect_t area = {0, 0, 3, 2};
            barrier_result = outcomes[result];
            assert(api.submit(api.context, &surface, &area, 91).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
            assert(!confirmed && !submit_barrier_panel); /* Both early callbacks and the old ISR were drained, not acknowledged. */
            boundary(&panel);
            assert(confirmed == (outcomes[result] == ESP_OK ? 1u : 0u));
            boundary(&panel);
            assert(confirmed == 1);
            barrier_result = ESP_OK;
            OK(api.quiesce(api.context, 100));
            OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
            free(panel.buffers[0]);
        }
    }
}
#ifdef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
static void queried_rgb(void)
{
    struct fake_lcd panel = {0};
    panel.buffers[0] = aligned_alloc(64, 64);
    panel.buffers[1] = aligned_alloc(64, 64);
    esp_display_present_lcd_fb_scanout_config_t config = {.panel = &panel, .kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
                                                          .width = 3, .height = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = 2, .retain_scanout = true
                                                         };
    esp_display_present_lcd_fb_scanout_t *adapter;
    /* No confirmation switch: the driver is authoritative. */
    OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
    OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
    panel.one_shot = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.one_shot = false;
    panel.custom_bounce = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.custom_bounce = false;
    panel.transformed = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.transformed = false;
    panel.no_bounce = true;
    config.bounce = true; /* A stale caller hint cannot override facts. */
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.link_retirement = true;
    config.bounce = false; /* Reliable direct-link retirement is sufficient. */
    OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
    OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
    panel.no_bounce = false;
    panel.link_retirement = false;
    panel.no_retirement = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.no_retirement = false;
    panel.wrong_dimensions = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.wrong_dimensions = false;
    panel.wrong_format = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.wrong_format = false;
    panel.wrong_size = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.wrong_size = false;
    panel.info_error = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_FAIL);
    panel.info_error = false;
    panel.one_fb = true;
    panel.no_bounce = true;
    config.buffer_count = 1;
    OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
    esp_display_present_panel_adapter_t api;
    OK(esp_display_present_lcd_fb_scanout_get_interface(adapter, &api));
    unsigned confirmed = 0;
    OK(api.attach(api.context, count_single, &confirmed));
    esp_display_present_surface_t surface = {panel.buffers[0], 12, {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}};
    esp_display_present_rect_t area = {0, 0, 3, 2};
    assert(api.submit(api.context, &surface, &area, 91).acceptance == ESP_DISPLAY_PRESENT_TX_ACCEPTED);
    assert(!panel.rgb && panel.rgb_vsync);
    (void)panel.rgb_vsync(&panel, NULL, panel.user);
    assert(confirmed == 1);
    (void)panel.rgb_vsync(&panel, NULL, panel.user);
    assert(confirmed == 1);
    OK(api.quiesce(api.context, 100));
    OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
    free(panel.buffers[0]);
    free(panel.buffers[1]);
}
#endif
typedef struct {
    unsigned count;
    uint64_t cookie;
    bool from_isr;
} link_receipt_t;
static bool link_complete(void *user, const esp_display_present_backend_event_t *event, bool from_isr)
{
    link_receipt_t *receipt = user;
    assert(event->kind == ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED);
    ++receipt->count;
    receipt->cookie = event->transaction_cookie;
    receipt->from_isr = from_isr;
    return false;
}
static void test_rgb_link_switch(void)
{
    for (size_t count = 2; count <= 3; ++count) {
        for (unsigned early = 0; early < 2; ++early) {
            for (unsigned fail = 0; fail < 2; ++fail) {
                link_receipt_t receipt = {0};
                struct fake_lcd panel = {.no_bounce = true, .link_retirement = true, .buffer_count = count,
                           .link_early = early, .board_info = true, .fail = fail, .link_published = &receipt.count
                };
                for (size_t i = 0; i < count; ++i) {
                    panel.buffers[i] = aligned_alloc(64, 64);
                }
                esp_display_present_lcd_fb_scanout_config_t config = {
                    .panel = &panel, .kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
                    .width = 3, .height = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
                    .buffer_count = count, .retain_scanout = true,
                };
                esp_display_present_lcd_fb_scanout_t *adapter;
                OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
                esp_display_present_panel_adapter_t api;
                OK(esp_display_present_lcd_fb_scanout_get_interface(adapter, &api));
                OK(api.attach(api.context, link_complete, &receipt));
                boundary(&panel); /* Idle unsolicited completion has no transaction. */
                assert(!receipt.count);
                esp_display_present_surface_t surface = {panel.buffers[1], 12,
                    {3, 2, 6, ESP_DISPLAY_PRESENT_FORMAT_RGB565, ESP_DISPLAY_PRESENT_ORDER_NATIVE}
                };
                esp_display_present_rect_t area = {0, 0, 3, 2};
                unsigned joins = barrier_calls;
                esp_display_present_tx_result_t tx = api.submit(api.context, &surface, &area, 71);
                assert(barrier_calls == joins); /* Never join away the sole link-switch event. */
                assert(tx.acceptance == (fail ? ESP_DISPLAY_PRESENT_TX_UNCERTAIN : ESP_DISPLAY_PRESENT_TX_ACCEPTED));
                if (fail) {
                    assert(!receipt.count);
                    boundary(&panel);
                    assert(!receipt.count); /* Uncertain submit cannot retire either allocation. */
                    assert(api.submit(api.context, &surface, &area, 72).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
                } else {
                    if (!early) {
                        assert(!receipt.count);
                        assert(api.submit(api.context, &surface, &area, 72).acceptance == ESP_DISPLAY_PRESENT_TX_REJECTED);
                        assert(api.quiesce(api.context, 0) == ESP_ERR_TIMEOUT);
                        boundary(&panel); /* Pending switch still drains while retained close retries. */
                    }
                    assert(receipt.count == 1 && receipt.cookie == 71 && receipt.from_isr == !early);
                    boundary(&panel);
                    assert(receipt.count == 1); /* Exactly one publication; no second periodic event required. */
                }
                OK(api.quiesce(api.context, 100));
                OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
                for (size_t i = 0; i < count; ++i) {
                    free(panel.buffers[i]);
                }
            }
        }
    }
}
static void test_board_facts(void)
{
    struct fake_lcd panel = {.board_info = true};
    panel.buffers[0] = aligned_alloc(64, 64);
    panel.buffers[1] = aligned_alloc(64, 64);
    esp_display_present_lcd_fb_scanout_config_t config = {
        .panel = &panel, .kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
        .width = 3, .height = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
        .buffer_count = 2, .retain_scanout = true, .bounce = true,
    };
    esp_display_present_lcd_fb_scanout_t *adapter;
    OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
    OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
#ifdef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
    assert(!panel.board_queries); /* Standard driver facts take precedence. */
#else
    assert(panel.board_queries);
#endif
    /* A stale confirmed-bounce fallback cannot hide known invalid facts. */
    panel.wrong_dimensions = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.wrong_dimensions = false;
    panel.transformed = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    panel.transformed = false;
    panel.info_error = true;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_FAIL);
    panel.info_error = false;
#ifndef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
    panel.board_info = false;
    config.bounce = false;
    assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
    config.bounce = true;
    OK(esp_display_present_lcd_fb_scanout_create(&config, &adapter));
    OK(esp_display_present_lcd_fb_scanout_destroy(adapter));
#endif
    free(panel.buffers[0]);
    free(panel.buffers[1]);
}
int main(void)
{
    test_board_facts();
    single_buffer_join();
    /* Callers cannot force single-event retirement without the ISR join. */
    struct fake_lcd one_boundary_panel = {0};
    for (int kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI; kind <= ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB; ++kind) {
        esp_display_present_lcd_fb_scanout_config_t config = {
            .panel = &one_boundary_panel, .kind = kind, .width = 3, .height = 2,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .buffer_count = 2,
            .delete_panel = true, .single_retire = true,
        };
        esp_display_present_lcd_fb_scanout_t *adapter = (void *)1;
        assert(esp_display_present_lcd_fb_scanout_create(&config, &adapter) == ESP_ERR_NOT_SUPPORTED);
        assert(!adapter && !one_boundary_panel.deleted);
    }
    /* Direct RGB is deliberately outside the audited bounce adapter contract. */
    struct fake_lcd unsupported_panel = {.no_bounce = true};
    esp_display_present_lcd_fb_scanout_config_t unsupported = {
        .panel = &unsupported_panel, .kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB,
        .width = 3, .height = 2, .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565,
        .buffer_count = 2, .delete_panel = true,
    };
    esp_display_present_lcd_fb_scanout_t *unsupported_adapter = NULL;
    assert(esp_display_present_lcd_fb_scanout_create(&unsupported, &unsupported_adapter) == ESP_ERR_NOT_SUPPORTED);
    assert(!unsupported_adapter && !unsupported_panel.deleted);
#ifdef ESP_LCD_RGB_PANEL_HAS_FRAME_BUFFER_INFO
    queried_rgb();
#endif
    test_rgb_link_switch();
    const esp_err_t join_results[] = {ESP_OK, ESP_ERR_NOT_SUPPORTED, ESP_ERR_TIMEOUT, ESP_FAIL};
    for (int kind = ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_DSI; kind <= ESP_DISPLAY_PRESENT_LCD_FB_SCANOUT_RGB; ++kind) {
        for (size_t count = 2; count <= 3; ++count) {
            for (size_t i = 0; i < sizeof(join_results) / sizeof(join_results[0]); ++i) {
                exercise(kind, false, join_results[i], count);
                exercise(kind, true, join_results[i], count);
            }
        }
    }
    test_callback_drain_retry();
    test_retained_scanout();
    puts("scanout: real adapter against driver stubs, boundary fence, initial scanout and stop retry passed");
}
