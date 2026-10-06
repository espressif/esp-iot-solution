/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * LVGL -> esp_display_present binding:
 *   - PARTITION borrows the presenter's draw-buffer pool (no adapter malloc)
 *   - DIRECT/FULL render into the presenter's rotating framebuffer lease
 *   - Each flush submits a rendered surface; the last flush commits the frame
 *
 * LVGL's finalized inv_areas are REPAIR coverage (same role as esp_lv_adapter).
 * Flush bands are transport chunks only — using them as coverage fragments the
 * diff tracker and forces large baseline copies.
 */

#include "esp_lv_present.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl_private.h"

static const char *TAG = "esp_lv_present";

/** Match LVGL's LV_INV_BUF_SIZE (v9 default 32). */
#define ESP_LV_PRESENT_MAX_AREAS 32

typedef struct {
    bool lvgl_inited;
    esp_display_presenter_t *presenter;
    esp_display_presenter_caps_t caps;
    lv_display_t *display;
    size_t bytes_per_pixel;
    bool is_surface_contract;
    bool is_full_contract;
    bool frame_full;
    bool needs_full_redraw;
    bool have_presented;
    bool frame_active;
    esp_display_presenter_buffer_t acquired;
    esp_display_present_area_t damage[ESP_LV_PRESENT_MAX_AREAS];
    size_t damage_count;
    uint32_t frame_count;
} esp_lv_present_ctx_t;

static esp_lv_present_ctx_t s_ctx;

static uint32_t esp_lv_present_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static bool esp_lv_present_area_is_full(const esp_lv_present_ctx_t *ctx,
                                        const esp_display_present_area_t *area)
{
    return ctx != NULL && area != NULL && area->x1 == 0 && area->y1 == 0 &&
           area->x2 == (int32_t)ctx->caps.width - 1 &&
           area->y2 == (int32_t)ctx->caps.height - 1;
}

static void esp_lv_present_capture_damage(esp_lv_present_ctx_t *ctx)
{
    ctx->damage_count = 0;
    lv_display_t *refreshing = lv_refr_get_disp_refreshing();
    if (refreshing == NULL) {
        return;
    }
    const uint16_t count = LV_MIN(refreshing->inv_p,
                                  (uint16_t)ESP_LV_PRESENT_MAX_AREAS);
    for (uint16_t index = 0; index < count; ++index) {
        if (refreshing->inv_area_joined[index]) {
            continue;
        }
        const lv_area_t *area = &refreshing->inv_areas[index];
        esp_display_present_area_t damage = {
            .x1 = area->x1, .y1 = area->y1,
            .x2 = area->x2, .y2 = area->y2,
        };
        if (esp_lv_present_area_is_full(ctx, &damage)) {
            ctx->damage[0] = damage;
            ctx->damage_count = 1;
            return;
        }
        ctx->damage[ctx->damage_count++] = damage;
    }
}

static void esp_lv_present_reset_frame(esp_lv_present_ctx_t *ctx)
{
    ctx->frame_active = false;
    ctx->frame_full = false;
    ctx->damage_count = 0;
    memset(&ctx->acquired, 0, sizeof(ctx->acquired));
}

static esp_err_t esp_lv_present_fail_frame(esp_lv_present_ctx_t *ctx,
                                           esp_err_t error)
{
    if (ctx->frame_active) {
        esp_display_presenter_cancel_frame(ctx->presenter);
    }
    esp_lv_present_reset_frame(ctx);
    return error == ESP_OK ? ESP_FAIL : error;
}

static esp_err_t esp_lv_present_begin_frame(esp_lv_present_ctx_t *ctx)
{
    /* Read LVGL's finalized, joined invalidation list after refresh has begun.
     * LV_EVENT_INVALIDATE_AREA fires while damage is still being accumulated;
     * using it as a separate frame lifecycle can clear/overflow the list and
     * falsely claim FULL coverage for a partial raster. */
    esp_lv_present_capture_damage(ctx);
    const esp_display_present_area_t full_area = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (int)ctx->caps.width - 1,
        .y2 = (int)ctx->caps.height - 1,
    };
    const bool requested_full = ctx->is_full_contract ||
                                ctx->damage_count == 0 ||
                                (ctx->damage_count == 1 &&
                                 esp_lv_present_area_is_full(ctx, &ctx->damage[0]));
    const esp_display_present_surface_request_t request = {
        .dirty_areas = requested_full ? &full_area : ctx->damage,
        .dirty_area_count = requested_full ? 1 : ctx->damage_count,
    };
    /* FramePlan requires a capacity ≥ dirty count; LVGL still paints by its
     * own flush areas, so the returned render list is unused. */
    esp_display_present_area_t render_areas[ESP_LV_PRESENT_MAX_AREAS];
    size_t render_count = 0;
    bool full = false;
    esp_err_t ret = esp_display_presenter_begin_next_frame(
                        ctx->presenter, &request, render_areas,
                        ESP_LV_PRESENT_MAX_AREAS, &render_count, &full);
    if (ret != ESP_OK) {
        return ret;
    }
    if (full && !requested_full) {
        /* First present must be a true full raster (repair has no prior FB).
         * After that, accept the partial LVGL flush and let REPAIR copy the
         * undrawn complement from disp_fb. */
        if (!ctx->have_presented) {
            esp_display_presenter_cancel_frame(ctx->presenter);
            ctx->needs_full_redraw = true;
            return ESP_ERR_INVALID_STATE;
        }
        ctx->frame_active = true;
        ctx->frame_full = false;
        return ESP_OK;
    }
    ctx->frame_active = true;
    ctx->frame_full = full || requested_full;
    return ESP_OK;
}

static esp_err_t esp_lv_present_bind_surface_buffer(
    esp_lv_present_ctx_t *ctx)
{
    if (!ctx->is_surface_contract || ctx->frame_active) {
        return ESP_OK;
    }
    esp_err_t ret = esp_lv_present_begin_frame(ctx);
    if (ret == ESP_OK) {
        ret = esp_display_presenter_acquire_buffer(ctx->presenter,
                                                   &ctx->acquired);
    }
    const size_t required = (size_t)ctx->caps.width * ctx->caps.height *
                            ctx->bytes_per_pixel;
    if (ret == ESP_OK &&
            (ctx->acquired.surface.pixels == NULL ||
             ctx->acquired.capacity_bytes < required)) {
        ret = ESP_ERR_INVALID_SIZE;
    }
    if (ret != ESP_OK) {
        return esp_lv_present_fail_frame(ctx, ret);
    }
    lv_display_set_buffers(ctx->display, ctx->acquired.surface.pixels, NULL,
                           ctx->acquired.capacity_bytes,
                           ctx->is_full_contract
                           ? LV_DISPLAY_RENDER_MODE_FULL
                           : LV_DISPLAY_RENDER_MODE_DIRECT);
    return ESP_OK;
}

static void esp_lv_present_on_render_start(lv_event_t *event)
{
    esp_lv_present_ctx_t *ctx = lv_event_get_user_data(event);
    esp_err_t ret = esp_lv_present_bind_surface_buffer(ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "acquire surface draw buffer failed: %s",
                 esp_err_to_name(ret));
    }
}

static esp_err_t esp_lv_present_submit_surface(
    esp_lv_present_ctx_t *ctx)
{
    size_t rows = 0;
    const size_t stride = (size_t)ctx->caps.width * ctx->bytes_per_pixel;
    esp_err_t ret = ctx->acquired.resolve_rows != NULL
                    ? ctx->acquired.resolve_rows(
                        ctx->acquired.resolve_rows_ctx,
                        ctx->acquired.lease_id, stride, ctx->caps.height,
                        &rows)
                    : ESP_ERR_INVALID_STATE;
    if (ret != ESP_OK || rows != ctx->caps.height) {
        return ESP_ERR_INVALID_SIZE;
    }
    const esp_display_present_area_t full = {
        .x1 = 0,
        .y1 = 0,
        .x2 = (int32_t)ctx->caps.width - 1,
        .y2 = (int32_t)ctx->caps.height - 1,
    };
    return esp_display_presenter_submit_buffer(
               ctx->presenter, &ctx->acquired, &full, stride);
}

static esp_err_t esp_lv_present_flush_area(esp_lv_present_ctx_t *ctx,
                                           const lv_area_t *area,
                                           const uint8_t *color_map,
                                           bool last)
{
    if (ctx->is_surface_contract) {
        return last ? esp_lv_present_submit_surface(ctx) : ESP_OK;
    }
    const size_t width = (size_t)(area->x2 - area->x1 + 1);
    const size_t height = (size_t)(area->y2 - area->y1 + 1);
    const size_t stride = width * ctx->bytes_per_pixel;

    esp_display_presenter_buffer_t acquired = ctx->acquired;
    esp_err_t ret = ESP_OK;
    ret = esp_display_presenter_acquire_buffer(ctx->presenter, &acquired);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t rows = 0;
    ret = acquired.resolve_rows != NULL
          ? acquired.resolve_rows(acquired.resolve_rows_ctx, acquired.lease_id,
                                  stride, height, &rows)
          : ESP_ERR_INVALID_STATE;
    if (ret != ESP_OK || rows != height) {
        esp_display_presenter_cancel_frame(ctx->presenter);
        return ESP_ERR_INVALID_SIZE;
    }

    if (color_map != (const uint8_t *)acquired.surface.pixels) {
        memcpy(acquired.surface.pixels, color_map, rows * stride);
    }

    const esp_display_present_area_t band = {
        .x1 = area->x1,
        .y1 = area->y1,
        .x2 = area->x2,
        .y2 = area->y2,
    };
    return esp_display_presenter_submit_buffer(ctx->presenter, &acquired, &band,
                                               stride);
}

static esp_err_t esp_lv_present_commit_frame(esp_lv_present_ctx_t *ctx)
{
    /* Keep the triple-buffer revision history in terms of LVGL's invalidation
     * areas, exactly like esp_lv_adapter's inv_areas tracker. Flush bands are
     * transport chunks and may split/merge overlapping object damage, so they
     * are not a stable description of what changed between framebuffer
     * revisions (notably with several moving transparent images). */
    const bool use_damage = !ctx->frame_full && ctx->damage_count > 0;
    const esp_display_presenter_submit_t submit = {
        .coverage = use_damage
        ? ESP_DISPLAY_PRESENT_COVERAGE_AREAS
        : ESP_DISPLAY_PRESENT_COVERAGE_FULL,
        .areas = use_damage ? ctx->damage : NULL,
        .area_count = use_damage ? ctx->damage_count : 0,
    };
    esp_err_t ret = esp_display_presenter_commit_frame(ctx->presenter, &submit);
    if (ret == ESP_OK) {
        ctx->frame_count++;
        ctx->have_presented = true;
    }
    return ret;
}

static void esp_lv_present_flush_cb(lv_display_t *disp, const lv_area_t *area,
                                    uint8_t *color_map)
{
    esp_lv_present_ctx_t *ctx = &s_ctx;
    const bool last = lv_display_flush_is_last(disp);

    if (ctx->needs_full_redraw) {
        if (last) {
            ctx->needs_full_redraw = false;
            lv_obj_invalidate(lv_display_get_screen_active(disp));
        }
        lv_display_flush_ready(disp);
        return;
    }

    esp_err_t ret = ESP_OK;
    if (!ctx->frame_active) {
        ret = esp_lv_present_begin_frame(ctx);
    }
    if (ret == ESP_OK) {
        ret = esp_lv_present_flush_area(ctx, area, color_map, last);
    }
    if (ret == ESP_OK && last) {
        ret = esp_lv_present_commit_frame(ctx);
        if (ret == ESP_OK) {
            esp_lv_present_reset_frame(ctx);
        }
    }
    if (ret != ESP_OK) {
        const bool retry_full = ctx->needs_full_redraw;
        if (!retry_full) {
            ESP_LOGE(TAG, "flush failed: %s", esp_err_to_name(ret));
        }
        (void)esp_lv_present_fail_frame(ctx, ret);
        if (retry_full && last) {
            ctx->needs_full_redraw = false;
            lv_obj_invalidate(lv_display_get_screen_active(disp));
        }
    }

    lv_display_flush_ready(disp);
}

esp_err_t esp_lv_present_start(esp_display_presenter_t *presenter,
                               lv_display_t **ret_disp)
{
    ESP_RETURN_ON_FALSE(presenter != NULL && ret_disp != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "arguments missing");
    esp_lv_present_ctx_t *ctx = &s_ctx;
    ESP_RETURN_ON_FALSE(ctx->display == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "binding already started");

    if (!ctx->lvgl_inited) {
        lv_init();
        lv_tick_set_cb(esp_lv_present_tick_cb);
        ctx->lvgl_inited = true;
    }

    ESP_RETURN_ON_ERROR(esp_display_presenter_get_caps(presenter, &ctx->caps),
                        TAG, "query presenter caps");
    ESP_RETURN_ON_FALSE(
        ctx->caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_PARTITION ||
        ctx->caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_DIRECT ||
        ctx->caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_FULL,
        ESP_ERR_NOT_SUPPORTED, TAG, "unsupported presenter contract %d",
        ctx->caps.contract);
    ESP_RETURN_ON_FALSE(
        ctx->caps.pixel_format == ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565 ||
        ctx->caps.pixel_format == ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888,
        ESP_ERR_NOT_SUPPORTED, TAG, "unsupported pixel format %d",
        ctx->caps.pixel_format);

    ESP_RETURN_ON_ERROR(esp_display_presenter_rebind_producer(presenter),
                        TAG, "rebind presenter producer");

    ctx->presenter = presenter;
    ctx->is_surface_contract =
        ctx->caps.contract != ESP_DISPLAY_PRESENT_CONTRACT_PARTITION;
    ctx->is_full_contract =
        ctx->caps.contract == ESP_DISPLAY_PRESENT_CONTRACT_FULL;
    ctx->bytes_per_pixel =
        ctx->caps.pixel_format == ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565
        ? 2U : 3U;

    esp_display_presenter_drawbuf_t drawbuf = {0};
    if (!ctx->is_surface_contract) {
        ESP_RETURN_ON_ERROR(
            esp_display_presenter_get_drawbuf(presenter, &drawbuf), TAG,
            "borrow presenter draw buffer");
    }
    esp_lv_present_reset_frame(ctx);
    ctx->needs_full_redraw = false;
    ctx->have_presented = false;
    ctx->frame_count = 0;

    lv_display_t *display = lv_display_create((int32_t)ctx->caps.width,
                                              (int32_t)ctx->caps.height);
    if (display == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (ctx->caps.pixel_format == ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888) {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    }

    ctx->display = display;
    esp_err_t ret;
    if (ctx->is_surface_contract) {
        ret = esp_lv_present_bind_surface_buffer(ctx);
        if (ret != ESP_OK) {
            lv_display_delete(display);
            ctx->display = NULL;
            return ret;
        }
        lv_display_add_event_cb(display, esp_lv_present_on_render_start,
                                LV_EVENT_RENDER_START, ctx);
    } else {
        lv_display_set_buffers(display, drawbuf.buffers[0],
                               drawbuf.count > 1 ? drawbuf.buffers[1] : NULL,
                               drawbuf.bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }
    lv_display_set_flush_cb(display, esp_lv_present_flush_cb);
    *ret_disp = display;
    ESP_LOGI(TAG,
             "LVGL display %ux%u on presenter (%s, %s, rows=%u, presenter drawbuf)",
             ctx->caps.width, ctx->caps.height,
             ctx->is_full_contract ? "FULL" :
             (ctx->is_surface_contract ? "DIRECT" : "PARTITION"),
             ctx->bytes_per_pixel == 2 ? "RGB565" : "RGB888",
             ctx->is_surface_contract ? ctx->caps.height :
             (unsigned)drawbuf.lines);
    return ESP_OK;
}

esp_err_t esp_lv_present_stop(void)
{
    esp_lv_present_ctx_t *ctx = &s_ctx;
    ESP_RETURN_ON_FALSE(ctx->display != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "binding not started");
    if (ctx->frame_active) {
        esp_display_presenter_cancel_frame(ctx->presenter);
    }
    lv_display_delete(ctx->display);
    ctx->display = NULL;
    ctx->presenter = NULL;
    ctx->is_surface_contract = false;
    ctx->is_full_contract = false;
    ctx->have_presented = false;
    esp_lv_present_reset_frame(ctx);
    return ESP_OK;
}

uint32_t esp_lv_present_get_frame_count(void)
{
    return s_ctx.frame_count;
}
