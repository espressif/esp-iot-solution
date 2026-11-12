#pragma once
/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../../src/resources/present_resources.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_spi.h"
#include "esp_cache.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#define OK(x) assert((x) == ESP_OK)
struct fake_lcd {
    unsigned available_fbs; /* Zero retains the three-FB fixture default. */
    pthread_mutex_t lock;
    pthread_cond_t changed;
    void *buffers[3];
    dpi_cb_t dsi_color, dsi, dsi_vsync;
    rgb_cb_t rgb_color, rgb, rgb_vsync;
    esp_lcd_panel_io_color_trans_done_cb_t io;
    void *user;
    const void *source;
    unsigned calls, gets, detached, deleted;
    unsigned physical_width, physical_height; /* Zero: default 8x4. */
    bool attach_fail, detach_fail, psram_direct;
    bool no_bounce, one_shot, transformed, custom_bounce, one_fb;
};
esp_err_t esp_lcd_panel_io_spi_get_memory_info(esp_lcd_panel_io_handle_t io,
                                               esp_lcd_panel_io_spi_memory_info_t *info)
{
    if (!io || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    *info = (esp_lcd_panel_io_spi_memory_info_t) {
        .psram_dma_direct = io->psram_direct
    };
    return ESP_OK;
}
bool esp_ptr_internal(const void *pixels)
{
    return pixels != NULL;
}
bool esp_ptr_dma_capable(const void *pixels)
{
    return pixels != NULL;
}
bool esp_ptr_dma_ext_capable(const void *pixels)
{
    (void)pixels;
    return false;
}
static unsigned executor_failure, destroy_failure, executor_calls;
static bool unavailable = true;
static pthread_mutex_t executor_gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t executor_changed = PTHREAD_COND_INITIALIZER;
static bool executor_hold, executor_entered;
typedef struct {
    esp_display_present_executor_t base;
    esp_display_present_executor_t *cpu;
} mock_executor_t;
static esp_err_t mock_caps(void *context, esp_display_present_executor_caps_t *caps)
{
    mock_executor_t *executor = context;
    esp_err_t error = esp_display_present_executor_get_caps(executor->cpu, caps);
    caps->name = "mock-hardware";
    return error;
}
static esp_err_t mock_probe(void *context, const esp_display_present_copy_request_t *request, esp_display_present_executor_reason_t *reason)
{
    return esp_display_present_executor_probe(((mock_executor_t *)context)->cpu, request, reason);
}
static esp_err_t mock_execute(void *context, const esp_display_present_copy_request_t *request)
{
    pthread_mutex_lock(&executor_gate);
    executor_entered = true;
    pthread_cond_broadcast(&executor_changed);
    while (executor_hold) {
        pthread_cond_wait(&executor_changed, &executor_gate);
    }
    pthread_mutex_unlock(&executor_gate);
    ++executor_calls;
    return executor_failure ? ESP_FAIL : esp_display_present_executor_execute(((mock_executor_t *)context)->cpu, request);
}
static esp_err_t mock_destroy(void *context)
{
    if (destroy_failure) {
        --destroy_failure;
        return ESP_FAIL;
    }
    mock_executor_t *executor = context;
    OK(esp_display_present_executor_destroy(executor->cpu));
    free(executor);
    return ESP_OK;
}
static const esp_display_present_executor_ops_t mock_ops = {mock_caps, mock_probe, mock_execute, mock_destroy};
/* Substitute constructors only in this executable: aggregation/rollback tests,
 * not hardware accuracy (the four existing executor suites test those plugins). */
esp_err_t esp_display_present_executor_dma2d_create(esp_display_present_executor_t **out)
{
    *out = NULL;
    if (unavailable) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    mock_executor_t *executor = calloc(1, sizeof(*executor));
    assert(executor);
    OK(esp_display_present_executor_cpu_create(&executor->cpu));
    executor->base = (esp_display_present_executor_t) {
        &mock_ops, executor
    };
    *out = &executor->base;
    return ESP_OK;
}
esp_err_t esp_display_present_executor_ppa_create(esp_display_present_executor_t **out)
{
    *out = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}
static esp_err_t register_callbacks(struct fake_lcd *panel, dpi_cb_t dsi, dpi_cb_t dsi_vsync,
                                    rgb_cb_t rgb, rgb_cb_t rgb_vsync, void *user)
{
    if ((user && panel->attach_fail) || (!user && panel->detach_fail)) {
        return ESP_FAIL;
    }
    pthread_mutex_lock(&panel->lock);
    panel->dsi = dsi;
    panel->dsi_vsync = dsi_vsync;
    panel->rgb = rgb;
    panel->rgb_vsync = rgb_vsync;
    panel->user = user;
    if (!user) {
        ++panel->detached;
    }
    pthread_mutex_unlock(&panel->lock);
    return ESP_OK;
}
esp_err_t esp_lcd_dpi_panel_register_event_callbacks(esp_lcd_panel_handle_t panel, const esp_lcd_dpi_panel_event_callbacks_t *callbacks, void *user)
{
    esp_err_t error = register_callbacks(panel, callbacks->on_frame_buf_complete, callbacks->on_vsync, NULL, NULL, user);
    if (error == ESP_OK) {
        panel->dsi_color = callbacks->on_color_trans_done;
    }
    return error;
}
esp_err_t esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_callbacks_t *callbacks, void *user)
{
    esp_err_t error = register_callbacks(panel, NULL, NULL, callbacks->on_frame_buf_complete, callbacks->on_vsync, user);
    if (error == ESP_OK) {
        panel->rgb_color = callbacks->on_color_trans_done;
    }
    return error;
}
esp_err_t esp_lcd_panel_io_register_event_callbacks(esp_lcd_panel_io_handle_t panel, const esp_lcd_panel_io_callbacks_t *callbacks, void *user)
{
    if ((user && panel->attach_fail) || (!user && panel->detach_fail)) {
        return ESP_FAIL;
    }
    panel->io = callbacks->on_color_trans_done;
    panel->user = user;
    panel->detached += !user;
    return ESP_OK;
}
static esp_err_t get_buffers(struct fake_lcd *panel, uint32_t count, void **first, va_list args)
{
    ++panel->gets;
    unsigned available = panel->one_fb ? 1 : panel->available_fbs ? panel->available_fbs : 3;
    if (!count || count > available) {
        return ESP_ERR_INVALID_ARG;
    }
    *first = panel->buffers[0];
    for (unsigned i = 1; i < count; ++i) {
        *va_arg(args, void **) = panel->buffers[i];
    }
    return ESP_OK;
}
esp_err_t esp_lcd_dpi_panel_get_frame_buffer(esp_lcd_panel_handle_t panel, uint32_t count, void **first, ...)
{
    va_list args;
    va_start(args, first);
    esp_err_t result = get_buffers(panel, count, first, args);
    va_end(args);
    return result;
}
esp_err_t esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t panel, uint32_t count, void **first, ...)
{
    va_list args;
    va_start(args, first);
    esp_err_t result = get_buffers(panel, count, first, args);
    va_end(args);
    return result;
}
esp_err_t esp_cache_msync(void *pixels, size_t bytes, int flags)
{
    assert(pixels && bytes && (flags & ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    return ESP_OK;
}
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x, int y, int ex, int ey, const void *source)
{
    assert(x >= 0 && y >= 0 && ex <= (int)(panel->physical_width ? panel->physical_width : 8) &&
           ey <= (int)(panel->physical_height ? panel->physical_height : 4) && x < ex && y < ey);
    pthread_mutex_lock(&panel->lock);
    panel->source = source;
    ++panel->calls;
    rgb_cb_t rgb_done = panel->rgb_color;
    dpi_cb_t dsi_done = panel->dsi_color;
    void *user = panel->user;
    pthread_cond_broadcast(&panel->changed);
    pthread_mutex_unlock(&panel->lock);
    if (rgb_done) {
        (void)rgb_done(panel, NULL, user);
    }
    if (dsi_done) {
        (void)dsi_done(panel, NULL, user);
    }
    return ESP_OK;
}
esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t panel)
{
    ++panel->deleted;
    return ESP_OK;
}
static void boundary(struct fake_lcd *panel)
{
    pthread_mutex_lock(&panel->lock);
    dpi_cb_t dsi = panel->dsi;
    rgb_cb_t rgb = panel->rgb;
    void *user = panel->user;
    pthread_mutex_unlock(&panel->lock);
    if (dsi) {
        (void)dsi(panel, NULL, user);
    }
    if (rgb) {
        (void)rgb(panel, NULL, user);
    }
}
static void init_panel(struct fake_lcd *panel)
{
    memset(panel, 0, sizeof(*panel));
    assert(pthread_mutex_init(&panel->lock, NULL) == 0);
    assert(pthread_cond_init(&panel->changed, NULL) == 0);
    for (unsigned i = 0; i < 3; ++i) {
        panel->buffers[i] = aligned_alloc(64, 64);
        assert(panel->buffers[i]);
        memset(panel->buffers[i], 0, 64);
    }
}
static void free_panel(struct fake_lcd *panel)
{
    assert(!panel->deleted && !panel->dsi_color && !panel->dsi && !panel->rgb && !panel->io);
    for (unsigned i = 0; i < 3; ++i) {
        free(panel->buffers[i]);
    }
    pthread_cond_destroy(&panel->changed);
    pthread_mutex_destroy(&panel->lock);
}
static esp_display_presenter_config_t config(struct fake_lcd *panel, bool rgb)
{
    return (esp_display_presenter_config_t) {
        .width = 8, .height = 4, .target = {
            .hw = {.panel = panel, .panel_type = rgb ? ESP_DISPLAY_PRESENT_PANEL_RGB : ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI},
            .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_DOUBLE_PARTIAL}, .drawbuf = {.lines = 4},
        },
    };
}

#include "../../../src/platform/te/present_te_sync.h"
struct esp_display_present_te_sync_context {
    esp_display_present_te_wakeup_fn wakeup;
    void *user;
    uint64_t edge;
    bool allow;
    pthread_mutex_t lock;
};
static size_t te_memory_test_extra;
size_t esp_display_present_te_sync_memory(void)
{
    return sizeof(esp_display_present_te_sync_context_t) + te_memory_test_extra;
}
static esp_display_present_te_sync_context_t *test_te;
static unsigned te_destroy_failures;
static unsigned te_create_failures;
esp_err_t esp_display_present_te_sync_create(const esp_display_present_te_sync_config_t *cfg, bool end,
                                             esp_display_present_te_sync_context_t **out)
{
    (void)cfg;
    (void)end;
    if (te_create_failures) {
        --te_create_failures;
        *out = NULL;
        return ESP_ERR_NO_MEM;
    }
    *out = calloc(1, sizeof(**out));
    test_te = *out;
    if (*out) {
        assert(!pthread_mutex_init(&(*out)->lock, NULL));
    }
    return *out ? ESP_OK : ESP_ERR_NO_MEM;
}
void esp_display_present_te_sync_set_wakeup(esp_display_present_te_sync_context_t *ctx,
                                            esp_display_present_te_wakeup_fn callback, void *user)
{
    ctx->wakeup = callback;
    ctx->user = user;
}
esp_err_t esp_display_present_te_sync_destroy_checked(esp_display_present_te_sync_context_t *ctx)
{
    if (te_destroy_failures) {
        --te_destroy_failures;
        return ESP_FAIL;
    }
    if (test_te == ctx) {
        test_te = NULL;
    }
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
    return ESP_OK;
}
esp_err_t esp_display_present_te_sync_destroy_budget(esp_display_present_te_sync_context_t *ctx, uint32_t timeout)
{
    (void)timeout;
    return esp_display_present_te_sync_destroy_checked(ctx);
}
esp_err_t esp_display_present_te_sync_admit(esp_display_present_te_sync_context_t *ctx,
                                            size_t bytes, uint64_t after, uint64_t *edge)
{
    (void)bytes;
    pthread_mutex_lock(&ctx->lock);
    bool ready = ctx->allow && ctx->edge > after;
    if (ready) {
        *edge = ctx->edge;
        ctx->allow = false;
    }
    pthread_mutex_unlock(&ctx->lock);
    return ready ? ESP_OK : ESP_ERR_TIMEOUT;
}

void esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t *ctx)
{
    (void)ctx;
}
void esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t *ctx)
{
    (void)ctx;
}
void esp_display_present_te_sync_abort_tx(esp_display_present_te_sync_context_t *ctx)
{
    (void)ctx;
}

esp_err_t esp_lcd_rgb_panel_get_frame_buffer_info(esp_lcd_panel_handle_t panel, esp_lcd_rgb_panel_frame_buffer_info_t *info)
{
    *info = (esp_lcd_rgb_panel_frame_buffer_info_t) {
        .width = panel->physical_width ? panel->physical_width : 8,
        .height = panel->physical_height ? panel->physical_height : 4,
        .frame_buffer_count = panel->one_fb ? 1 : panel->available_fbs ? panel->available_fbs : 3,
        .bits_per_pixel = 16,
        .continuous_refresh = !panel->one_shot,
        .transformed = panel->transformed,
        .custom_bounce_callback = panel->custom_bounce,
        .bounce_buffer_bytes = panel->no_bounce ? 0 : 16,
        .frame_buffer_retirement = !panel->no_bounce,
    };
    info->frame_buffer_bytes = (size_t)info->width * info->height * 2;
    return ESP_OK;
}
