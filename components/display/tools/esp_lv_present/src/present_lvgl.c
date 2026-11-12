/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "present_lvgl.h"
#include <limits.h>
#include <string.h>

#if LVGL_VERSION_MAJOR != 9
#error "Binding requires LVGL major version 9"
#endif

struct esp_lv_present_native {
    esp_display_present_engine_t *presenter;
    lv_display_t *display;
    lv_draw_buf_t *draw;
    void *pool_base;
    struct {
        void *buffers[2];
        size_t count;
        bool requested;
        bool preferred;
        lv_draw_buf_t peer;
    } direct;
    esp_display_present_backend_caps_t caps;
    size_t bpp;
    bool refreshing;
    struct {
        bool in_refresh, closing;
    } lifecycle;
    bool zero_copy;
    bool pooled_partial;
    bool need_tile;
    lv_draw_buf_t borrowed;
    esp_display_present_lease_t lease;
    esp_err_t error;
};

static esp_err_t copy_flush(esp_lv_present_native_t *binding, const lv_area_t *area, uint8_t *pixels)
{
    if (!area || !pixels || area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 || area->y2 < area->y1 ||
            (uint32_t)area->x2 >= binding->caps.draw_layout.width || (uint32_t)area->y2 >= binding->caps.draw_layout.height) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t width = (uint32_t)(area->x2 - area->x1) + 1;
    uint32_t height = (uint32_t)(area->y2 - area->y1) + 1;
    if (width > SIZE_MAX / binding->bpp) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t row = width * binding->bpp;
#if LVGL_VERSION_MINOR >= 1
    lv_draw_buf_t *active = lv_display_get_buf_active(binding->display);
#else
    /* The copy path registers only binding->draw and no second LVGL buffer. */
    lv_draw_buf_t *active = binding->draw;
#endif
    if (!active || !active->data) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t stride = active->header.stride;
    bool full_surface = binding->caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION;
    uintptr_t base = (uintptr_t)active->data, ptr = (uintptr_t)pixels;
    if (full_surface) {
        /* LVGL FULL/DIRECT passes the full draw-buffer base, even for dirty areas. */
        if (!stride || ptr != base || (size_t)area->y1 > SIZE_MAX / stride) {
            return ESP_ERR_INVALID_SIZE;
        }
        size_t offset = (size_t)area->y1 * stride;
        size_t column = (size_t)area->x1 * binding->bpp;
        if (offset > active->data_size || column > active->data_size - offset) {
            return ESP_ERR_INVALID_SIZE;
        }
        pixels += offset + column;
        ptr = (uintptr_t)pixels;
    }
    if (stride < row || height - 1 > (SIZE_MAX - row) / stride ||
            ptr < base || ptr - base > active->data_size ||
            (height - 1) * stride + row > active->data_size - (ptr - base)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!binding->refreshing) {
        /* copy_flush reads LVGL's persistent canvas, never the destination's
         * previous contents. Coverage comes from each actual flushed region. */
        const esp_display_present_frame_request_t request = {
            .resolved_pixels = binding->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT &&
            !binding->caps.writes_active_scanout
        };
        esp_err_t ret = esp_display_present_engine_begin_next_frame(binding->presenter, &request);
        if (ret != ESP_OK) {
            return ret;
        }
        binding->refreshing = true;
    }
    for (uint32_t offset = 0; offset < height;) {
        esp_display_present_lease_t lease;
        esp_err_t ret = esp_display_present_engine_acquire_buffer(binding->presenter, &lease);
        if (ret != ESP_OK) {
            return ret;
        }
        uint32_t rows = height - offset;
        if (!full_surface) {
            ret = esp_display_present_engine_resolve_rows(binding->presenter, &lease, row, height - offset, &rows);
            if (ret != ESP_OK) {
                return ret;
            }
        }
        size_t destination_stride = full_surface ? lease.surface.layout.stride_bytes : row;
        if (!rows) {
            return ESP_ERR_INVALID_SIZE;
        }
        for (uint32_t y = 0; y < rows; ++y) {
            size_t destination_offset = full_surface ?
                                        ((size_t)area->y1 + offset + y) * destination_stride + (size_t)area->x1 * binding->bpp : y * row;
            if (destination_offset > lease.surface.capacity_bytes || row > lease.surface.capacity_bytes - destination_offset) {
                return ESP_ERR_INVALID_SIZE;
            }
            uint8_t *dst = (uint8_t *)lease.surface.pixels + destination_offset;
            const uint8_t *src = pixels + (offset + y) * stride;
            if (binding->bpp == 2) {
                memcpy(dst, src, row);
            } else {
                /* LVGL RGB888 stores lv_color_t as B,G,R; ESP_DISPLAY_PRESENT_FORMAT_RGB888 is R,G,B.
                 * Normalize channels while copying, independently of stride. */
                _Static_assert(sizeof(lv_color_t) == 3, "Unexpected LVGL RGB888 representation");
                for (size_t x = 0; x < width; ++x) {
                    lv_color_t color;
                    memcpy(&color, src + x * 3, sizeof(color));
                    dst[x * 3] = color.red;
                    dst[x * 3 + 1] = color.green;
                    dst[x * 3 + 2] = color.blue;
                }
            }
        }
        esp_display_present_engine_area_t band = { area->x1, area->y1 + (int32_t)offset,
                                                   area->x2, area->y1 + (int32_t)(offset + rows - 1)
                                                 };
        ret = esp_display_present_engine_submit_buffer(binding->presenter, &lease, &band, destination_stride);
        if (ret != ESP_OK) {
            return ret;
        }
        offset += rows;
    }
    return ESP_OK;
}

static void detach(esp_lv_present_native_t *binding)
{
    if (binding->draw) {
        lv_display_set_draw_buffers(binding->display, binding->draw, NULL);
    } else {
        /* Keep descriptors/sync history, revoke every borrowed pixel address. */
        binding->borrowed.data = NULL;
        binding->borrowed.unaligned_data = NULL;
        binding->direct.peer.data = NULL;
        binding->direct.peer.unaligned_data = NULL;
    }
}

static void fail_refresh(esp_lv_present_native_t *binding, esp_err_t error)
{
    if (binding->error == ESP_OK) {
        binding->error = error;
    }
    lv_timer_pause(lv_display_get_refr_timer(binding->display));
    if (binding->zero_copy) {
        binding->need_tile = false;
        detach(binding);
    }
    if (esp_display_present_engine_cancel_frame(binding->presenter) == ESP_OK) {
        binding->refreshing = false;
    }
}

static void round_area(lv_event_t *event)
{
    esp_lv_present_native_t *binding = lv_event_get_user_data(event);
    lv_area_t *area = lv_event_get_param(event);
    if (!area || area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 || area->y2 < area->y1) {
        return;
    }
    esp_display_present_rect_t dirty = {area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1}, render;
    esp_err_t error = esp_display_present_engine_resolve_render_area(binding->presenter, &dirty, &render);
    if (error != ESP_OK) {
        fail_refresh(binding, error);
        return;
    }
    area->x1 = render.x;
    area->y1 = render.y;
    area->x2 = render.x + render.width - 1;
    area->y2 = render.y + render.height - 1;
}

/* A single LVGL descriptor is rebound only after all drawing for the previous
 * band has drained. Presenter remains the authority for tile availability. */
/* LVGL calls this after shaping the next single-buffer band and preparing
 * its layer, but before adding draw tasks. Preserve that shape: only exchange
 * storage, never reset the header or layer clip coordinates here. */
#if LVGL_VERSION_MINOR >= 5
static void acquire_before_draw(lv_event_t *event)
{
    esp_lv_present_native_t *binding = lv_event_get_user_data(event);
    if (!binding->need_tile || binding->error != ESP_OK) {
        return;
    }
    binding->need_tile = false;
    if (!binding->refreshing) {
        const esp_display_present_frame_request_t request = {0};
        esp_err_t ret = esp_display_present_engine_begin_next_frame(binding->presenter, &request);
        if (ret != ESP_OK) {
            fail_refresh(binding, ret);
            return;
        }
        binding->refreshing = true;
    }
    esp_err_t ret = esp_display_present_engine_acquire_buffer(binding->presenter, &binding->lease);
    if (ret != ESP_OK) {
        binding->borrowed.data = binding->draw->data;
        binding->borrowed.unaligned_data = binding->draw->unaligned_data;
        fail_refresh(binding, ret);
        return;
    }
    binding->borrowed.data = binding->lease.surface.pixels;
    binding->borrowed.unaligned_data = binding->lease.surface.pixels;
}

#endif

/* Borrow before LVGL's entry buffer check, not only before RENDER_START.
 * No owned pixel canvas is needed for FULL or physical two-buffer DIRECT. */
static esp_err_t borrow_refresh(esp_lv_present_native_t *binding)
{
    if (binding->refreshing) {
        return ESP_OK;
    }
    const esp_display_present_frame_request_t request = {
        .resolved_pixels = binding->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT
    };
    esp_err_t error = esp_display_present_engine_begin_next_frame(binding->presenter, &request);
    if (error != ESP_OK) {
        return error;
    }
    binding->refreshing = true;
    error = esp_display_present_engine_acquire_buffer(binding->presenter, &binding->lease);
    if (error != ESP_OK) {
        return error;
    }
    if (binding->direct.count == 2 && binding->lease.surface.pixels != binding->direct.buffers[0] &&
            binding->lease.surface.pixels != binding->direct.buffers[1]) {
        return ESP_ERR_INVALID_STATE;
    }
    if (binding->lease.surface.capacity_bytes > UINT32_MAX ||
            lv_draw_buf_init(&binding->borrowed, binding->caps.draw_layout.width, binding->caps.draw_layout.height,
                             LV_COLOR_FORMAT_RGB565, binding->caps.draw_layout.stride_bytes, binding->lease.surface.pixels,
                             binding->lease.surface.capacity_bytes) != LV_RESULT_OK) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (binding->direct.count == 2) {
        void *peer = binding->lease.surface.pixels == binding->direct.buffers[0] ?
                     binding->direct.buffers[1] : binding->direct.buffers[0];
        if (lv_draw_buf_init(&binding->direct.peer, binding->caps.draw_layout.width, binding->caps.draw_layout.height,
                             LV_COLOR_FORMAT_RGB565, binding->caps.draw_layout.stride_bytes, peer,
                             binding->lease.surface.capacity_bytes) != LV_RESULT_OK) {
            return ESP_ERR_INVALID_SIZE;
        }
    }
    if (binding->display) {
        lv_display_set_draw_buffers(binding->display, &binding->borrowed,
                                    binding->direct.count == 2 ? &binding->direct.peer : NULL);
    }
    return ESP_OK;
}

static void refresh_event(lv_event_t *event)
{
    esp_lv_present_native_t *binding = lv_event_get_user_data(event);
    if (!binding->zero_copy || binding->lifecycle.closing) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_REFR_REQUEST) {
        binding->direct.requested = true;
        if (!binding->pooled_partial && !binding->lifecycle.in_refresh && binding->error == ESP_OK) {
            esp_err_t error = borrow_refresh(binding);
            if (error != ESP_OK) {
                fail_refresh(binding, error);
            }
        }
        return;
    }
    if (code == LV_EVENT_REFR_START) {
        binding->lifecycle.in_refresh = true;
        return;
    }
    if (code == LV_EVENT_REFR_READY) {
        binding->lifecycle.in_refresh = false;
        binding->direct.requested = false;
        binding->need_tile = false;
        detach(binding);
        if (binding->refreshing) {
            esp_err_t error = esp_display_present_engine_cancel_frame(binding->presenter);
            if (error == ESP_OK) {
                binding->refreshing = false;
            } else {
                fail_refresh(binding, error);
            }
        }
        return;
    }
    if (code == LV_EVENT_RENDER_START && binding->pooled_partial && binding->error == ESP_OK) {
        (void)lv_draw_buf_init(&binding->borrowed, binding->caps.draw_layout.width, binding->draw->header.h,
                               LV_COLOR_FORMAT_RGB565, binding->caps.draw_layout.width * 2,
                               binding->pool_base, binding->draw->data_size);
        lv_display_set_draw_buffers(binding->display, &binding->borrowed, NULL);
        binding->need_tile = true;
    }
}

static esp_err_t submit_borrowed(esp_lv_present_native_t *binding, const lv_area_t *area, uint8_t *pixels)
{
    if (binding->pooled_partial) {
        if (!binding->refreshing || !area || pixels != binding->lease.surface.pixels || !binding->borrowed.header.stride) {
            return ESP_ERR_INVALID_STATE;
        }
        esp_display_present_engine_area_t band = {area->x1, area->y1, area->x2, area->y2};
        return esp_display_present_engine_submit_buffer(binding->presenter, &binding->lease, &band, binding->borrowed.header.stride);
    }
    if (binding->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT) {
        if (!binding->refreshing || !area || pixels != binding->borrowed.data || pixels != binding->lease.surface.pixels ||
                area->x1 < 0 || area->y1 < 0 || area->x2 < area->x1 || area->y2 < area->y1 ||
                (uint32_t)area->x2 >= binding->caps.draw_layout.width || (uint32_t)area->y2 >= binding->caps.draw_layout.height) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!lv_display_flush_is_last(binding->display)) {
            return ESP_OK;
        }
        /* LVGL's two-buffer sync plus this refresh's drawing define the whole
         * leased image. Submit it once: backend history repair would repeat the
         * sync. LVGL still retains its exact dirty list for the next refresh.
         * The first refresh / orientation change is explicitly invalidated full. */
        const esp_display_present_engine_area_t full = {0, 0, binding->caps.draw_layout.width - 1,
                                                        binding->caps.draw_layout.height - 1
                                                       };
        detach(binding);
        return esp_display_present_engine_submit_buffer(binding->presenter, &binding->lease, &full,
                                                        binding->lease.surface.layout.stride_bytes);
    }
    if (!binding->refreshing || pixels != binding->lease.surface.pixels || !area || area->x1 || area->y1 ||
            area->x2 != (int32_t)binding->caps.draw_layout.width - 1 || area->y2 != (int32_t)binding->caps.draw_layout.height - 1 ||
            !lv_display_flush_is_last(binding->display)) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_display_present_engine_area_t full = {area->x1, area->y1, area->x2, area->y2};
    detach(binding);
    return esp_display_present_engine_submit_buffer(binding->presenter, &binding->lease, &full, binding->lease.surface.layout.stride_bytes);
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    esp_lv_present_native_t *binding = lv_display_get_user_data(display);
    if (binding->error == ESP_OK) {
        esp_err_t ret = binding->zero_copy ? submit_borrowed(binding, area, pixels) : copy_flush(binding, area, pixels);
        if (ret == ESP_OK && lv_display_flush_is_last(display)) {
            ret = esp_display_present_engine_commit_frame(binding->presenter, NULL);
            if (ret == ESP_OK) {
                binding->refreshing = false;
                (void)esp_display_present_engine_service(binding->presenter);
            }
        }
        if (ret == ESP_OK && binding->pooled_partial && binding->refreshing) {
            binding->need_tile = true;
        }
        if (ret != ESP_OK) {
            fail_refresh(binding, ret);
        }
    }
    if (binding->zero_copy && binding->direct.count != 2 && (!binding->refreshing ||
                                                             (!binding->pooled_partial && binding->caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT))) {
        detach(binding);
    }
    /* Pooled PARTIAL defers acquisition until FLUSH_WAIT_START just before
     * the next band draws; no draw task may use the old source in between. */
    lv_display_flush_ready(display);
}

static esp_err_t create_binding(esp_display_present_engine_t *presenter, uint32_t rows, bool zero_copy, esp_lv_present_native_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!presenter || !rows) {
        return ESP_ERR_INVALID_ARG;
    }
#if LVGL_VERSION_MINOR < 5
    /* Older LVGL releases use the independent copy path; no pool is lent. */
    if (zero_copy) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
    if (!lv_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_backend_caps_t caps;
    esp_err_t ret = esp_display_present_engine_get_caps(presenter, &caps);
    if (ret != ESP_OK) {
        return ret;
    }
    if (caps.contract > ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT || caps.draw_layout.byte_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE ||
            (caps.draw_layout.format != ESP_DISPLAY_PRESENT_FORMAT_RGB565 && caps.draw_layout.format != ESP_DISPLAY_PRESENT_FORMAT_RGB888)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_display_present_engine_status_t status;
    ret = esp_display_present_engine_get_status(presenter, &status);
    if (ret != ESP_OK) {
        return ret;
    }
    if (status.state != ESP_DISPLAY_PRESENT_ENGINE_RUNNING || status.frame_active || status.pending_frames || status.first_error != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        rows = caps.draw_layout.height;
    }
    if (rows > caps.draw_layout.height) {
        rows = caps.draw_layout.height;
    }
    size_t bpp = caps.draw_layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (caps.draw_layout.width > INT_MAX || caps.draw_layout.height > INT_MAX ||
            caps.draw_layout.width > UINT32_MAX / bpp) {
        return ESP_ERR_INVALID_SIZE;
    }
    lv_color_format_t cf = bpp == 2 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888;
    uint32_t stride = lv_draw_buf_width_to_stride(caps.draw_layout.width, cf);
    if (zero_copy && (bpp != 2 || caps.draw_layout.stride_bytes != stride ||
                      (caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL && caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION &&
                       caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT) ||
                      (caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && caps.writes_active_scanout))) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    void *pool[16] = {0};
    size_t pool_count = 0;
    if (zero_copy && caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL) {
        size_t count = 0, bytes = 0;
        ret = esp_display_present_engine_get_draw_buffers(presenter, pool, 16, &count, &bytes);
        if (ret != ESP_OK) {
            return ret;
        }
        /* DIRECT may borrow exactly two physical buffers. A shared logical
         * conversion canvas uses the copy path to overlap drawing/admission. */
        if (!count || bytes < (size_t)stride * rows ||
                (caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT && count != 2)) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        pool_count = count;
    }
    if (!stride || rows > UINT32_MAX / stride) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_lv_present_native_t *binding = lv_malloc_zeroed(sizeof(*binding));
    if (!binding) {
        return ESP_ERR_NO_MEM;
    }
    binding->presenter = presenter;
    binding->pool_base = pool[0];
    if (zero_copy && caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT) {
        binding->direct.count = pool_count;
        binding->direct.requested = true;
        binding->direct.preferred = true;
        for (size_t i = 0; i < pool_count; ++i) {
            binding->direct.buffers[i] = pool[i];
        }
    }
    binding->caps = caps;
    binding->bpp = bpp;
    binding->zero_copy = zero_copy;
    binding->pooled_partial = zero_copy && caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION;
    /* Copy paths own their canvas. Pooled PARTIAL keeps a tile-sized error
     * sink because LVGL cannot abort a band after FLUSH_WAIT_START fails. */
    if (!zero_copy || binding->pooled_partial) {
        binding->draw = lv_draw_buf_create(caps.draw_layout.width, rows, cf, stride);
        if (!binding->draw) {
            lv_free(binding);
            return ESP_ERR_NO_MEM;
        }
    } else {
        ret = borrow_refresh(binding);
        if (ret != ESP_OK) {
            (void)esp_display_present_engine_cancel_frame(presenter);
            lv_free(binding);
            return ret;
        }
    }
    binding->display = lv_display_create((int32_t)caps.draw_layout.width, (int32_t)caps.draw_layout.height);
    if (!binding->display) {
        if (binding->refreshing) {
            (void)esp_display_present_engine_cancel_frame(presenter);
        }
        if (binding->draw) {
            lv_draw_buf_destroy(binding->draw);
        }
        lv_free(binding);
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_color_format(binding->display, cf);
    lv_display_set_user_data(binding->display, binding);
    lv_display_set_draw_buffers(binding->display, binding->draw ? binding->draw : &binding->borrowed,
                                binding->direct.count == 2 ? &binding->direct.peer : NULL);
    lv_display_render_mode_t mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
    switch (caps.contract) {
    case ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL:
        mode = LV_DISPLAY_RENDER_MODE_FULL;
        break;
    case ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT:
        mode = LV_DISPLAY_RENDER_MODE_DIRECT;
        break;
    default:
        break;
    }
    lv_display_set_render_mode(binding->display, mode);
    lv_display_set_flush_cb(binding->display, flush);
    lv_display_add_event_cb(binding->display, round_area, LV_EVENT_INVALIDATE_AREA, binding);
#if LVGL_VERSION_MINOR >= 5
    if (binding->pooled_partial) {
        lv_display_add_event_cb(binding->display, acquire_before_draw, LV_EVENT_FLUSH_WAIT_START, binding);
    }
#endif
    if (zero_copy) {
        if (!binding->pooled_partial) {
            lv_display_add_event_cb(binding->display, refresh_event, LV_EVENT_REFR_REQUEST, binding);
            lv_display_add_event_cb(binding->display, refresh_event, LV_EVENT_REFR_START, binding);
        } else {
            lv_display_add_event_cb(binding->display, refresh_event, LV_EVENT_RENDER_START, binding);
        }
        lv_display_add_event_cb(binding->display, refresh_event, LV_EVENT_REFR_READY, binding);
    }
    /* TE has no baseline until actual full coverage has been composed. */
    lv_obj_invalidate(lv_display_get_screen_active(binding->display));
    *out = binding;
    return ESP_OK;
}

esp_err_t esp_lv_present_native_create(esp_display_present_engine_t *presenter, uint32_t rows, esp_lv_present_native_t **out)
{
    return create_binding(presenter, rows, false, out);
}

esp_err_t esp_lv_present_native_create_zero_copy(esp_display_present_engine_t *presenter, esp_lv_present_native_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    esp_display_present_backend_caps_t caps;
    esp_err_t error = esp_display_present_engine_get_caps(presenter, &caps);
    if (error != ESP_OK) {
        return error;
    }
    if (caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL && caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return create_binding(presenter, 1, true, out);
}
esp_err_t esp_lv_present_native_create_pooled(esp_display_present_engine_t *presenter, uint32_t rows, esp_lv_present_native_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    esp_display_present_backend_caps_t caps;
    esp_err_t error = esp_display_present_engine_get_caps(presenter, &caps);
    if (error != ESP_OK) {
        return error;
    }
    if (caps.contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return create_binding(presenter, rows, true, out);
}

lv_display_t *esp_lv_present_native_get_display(esp_lv_present_native_t *binding)
{
    return binding ? binding->display : NULL;
}

esp_err_t esp_lv_present_native_get_error(esp_lv_present_native_t *binding)
{
    if (!binding) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_engine_status_t status;
    esp_err_t ret = esp_display_present_engine_get_status(binding->presenter, &status);
    if (ret != ESP_OK) {
        return ret;
    }
    if (binding->error == ESP_OK && status.first_error != ESP_OK) {
        fail_refresh(binding, status.first_error);
    }
    return binding->error;
}

esp_err_t esp_lv_present_native_destroy(esp_lv_present_native_t *binding)
{
    if (!binding) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_engine_status_t status;
    esp_err_t ret = esp_display_present_engine_get_status(binding->presenter, &status);
    if (ret != ESP_OK) {
        return ret;
    }
    binding->lifecycle.closing = true;
    lv_timer_pause(lv_display_get_refr_timer(binding->display));
    detach(binding);
    ret = esp_display_present_engine_cancel_frame(binding->presenter);
    if (ret != ESP_OK) {
        return ret;
    }
    lv_display_delete(binding->display);
    if (binding->draw) {
        lv_draw_buf_destroy(binding->draw);
    }
    lv_free(binding);
    return ESP_OK;
}

esp_err_t esp_lv_present_native_set_rotation(esp_lv_present_native_t *binding, esp_display_present_engine_rotation_t rotation, uint32_t timeout)
{
    if (!binding || rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) {
        return ESP_ERR_INVALID_ARG;
    }
    if (binding->lifecycle.in_refresh || binding->lifecycle.closing || binding->error != ESP_OK ||
            (binding->refreshing && binding->draw)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_engine_rotation_t old_rotation;
    esp_err_t error = esp_display_present_engine_get_rotation(binding->presenter, &old_rotation);
    if (error != ESP_OK) {
        return error;
    }
    if (old_rotation == rotation) {
        return ESP_OK;
    }
    bool axes = ((unsigned)old_rotation ^ (unsigned)rotation) & 1;
    uint32_t width = axes ? binding->caps.draw_layout.height : binding->caps.draw_layout.width;
    uint32_t height = axes ? binding->caps.draw_layout.width : binding->caps.draw_layout.height;
    uint32_t rows = binding->caps.contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? binding->draw->header.h : height;
    if (rows > height) {
        rows = height;
    }
    lv_color_format_t format = binding->bpp == 2 ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_RGB888;
    uint32_t stride = lv_draw_buf_width_to_stride(width, format);
    if (binding->zero_copy && stride != (size_t)width * binding->bpp) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (binding->pooled_partial) {
        /* A quarter-turn changes the full-width tile stride; the backing pool
         * retains its original byte capacity. Reduce the rows before rebinding. */
        size_t available = binding->caps.drawbuf_capacity_bytes / stride;
        if (!available) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (rows > available) {
            rows = available;
        }
    }
    bool owned = binding->pooled_partial || (!binding->zero_copy && !binding->direct.preferred) ||
                 (binding->direct.preferred && rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0);
    lv_draw_buf_t *draw = owned ? lv_draw_buf_create(width, rows, format, stride) : NULL;
    if (owned && !draw) {
        return ESP_ERR_NO_MEM;
    }
    if (binding->refreshing) {
        detach(binding);
        error = esp_display_present_engine_cancel_frame(binding->presenter);
        if (error != ESP_OK) {
            if (draw) {
                lv_draw_buf_destroy(draw);
            }
            return error;
        }
        binding->refreshing = false;
    }
    error = esp_display_present_engine_set_rotation(binding->presenter, rotation, timeout);
    if (error != ESP_OK) {
        if (draw) {
            lv_draw_buf_destroy(draw);
        }
        lv_obj_invalidate(lv_display_get_screen_active(binding->display));
        return error;
    }
    error = esp_display_present_engine_get_caps(binding->presenter, &binding->caps);
    if (error != ESP_OK) {
        if (draw) {
            lv_draw_buf_destroy(draw);
        }
        fail_refresh(binding, error);
        return error;
    }
    if (binding->direct.preferred) {
        void *pool[16] = {0};
        size_t count = 0, bytes = 0;
        error = esp_display_present_engine_get_draw_buffers(binding->presenter, pool, 16, &count, &bytes);
        if (error != ESP_OK) {
            if (draw) {
                lv_draw_buf_destroy(draw);
            }
            fail_refresh(binding, error);
            return error;
        }
        binding->zero_copy = count == 2 && !binding->caps.writes_active_scanout;
        binding->direct.count = binding->zero_copy ? count : 0;
        binding->direct.requested = true;
        for (size_t i = 0; i < binding->direct.count; ++i) {
            binding->direct.buffers[i] = pool[i];
        }
        binding->pool_base = binding->zero_copy ? pool[0] : NULL;
    }
    if (binding->pooled_partial) {
        void *pool[16] = {0};
        size_t count = 0, bytes = 0;
        if (esp_display_present_engine_get_draw_buffers(binding->presenter, pool, 16, &count, &bytes) == ESP_OK && count) {
            binding->pool_base = pool[0];
        }
    }
    lv_draw_buf_t *old = binding->draw;
    binding->draw = draw;
    if (draw) {
        lv_display_set_draw_buffers(binding->display, draw, NULL);
    } else {
        error = borrow_refresh(binding);
        if (error != ESP_OK) {
            fail_refresh(binding, error);
            if (old) {
                lv_draw_buf_destroy(old);
            }
            return error;
        }
    }
    lv_display_set_resolution(binding->display, width, height);
    if (old) {
        lv_draw_buf_destroy(old);
    }
    lv_obj_invalidate(lv_display_get_screen_active(binding->display));
    return ESP_OK;
}
