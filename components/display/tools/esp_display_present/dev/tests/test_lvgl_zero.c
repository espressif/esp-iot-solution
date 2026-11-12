/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "core/present_engine.h"
#include "present_lvgl.h"
#include "pixel/present_pixel_ops.h"
#if LVGL_VERSION_MINOR == 0
#include "src/display/lv_display_private.h"
#define lv_display_get_buf_active(display) ((display)->buf_act)
#endif
#include "scan_panel.h"
#include <time.h>
#include <stdio.h>
#define OK(x) assert((x)==ESP_OK)
static unsigned canvas_allocations;
lv_draw_buf_t *__real_lv_draw_buf_create(uint32_t width, uint32_t height, lv_color_format_t format, uint32_t stride);
lv_draw_buf_t *__wrap_lv_draw_buf_create(uint32_t width, uint32_t height, lv_color_format_t format, uint32_t stride)
{
    ++canvas_allocations;
    return __real_lv_draw_buf_create(width, height, format, stride);
}
#define W 8
#define H 6
static void exercise(bool timeout, esp_display_present_present_mode_t mode)
{
    scan_panel_t panel = {0};
    esp_display_present_engine_config_t cfg = {.backend = {.width = W, .height = H, .tile_rows = 2, .buffer_count = 3, .max_frames = 4,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = scan_interface(&panel)
                                                          }, .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = mode, .timeout_ms = 0
                                              };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&cfg, &p));
    void *buffers[3];
    size_t count, bytes;
    OK(esp_display_present_engine_get_draw_buffers(p, buffers, 3, &count, &bytes));
    esp_lv_present_native_t *b;
#if LVGL_VERSION_MINOR < 5
    assert(esp_lv_present_native_create_zero_copy(p, &b) == ESP_ERR_NOT_SUPPORTED);
    OK(esp_lv_present_native_create(p, 2, &b));
#else
    unsigned before = canvas_allocations;
    OK(esp_lv_present_native_create_zero_copy(p, &b));
    assert(canvas_allocations == before); /* Borrowed FULL needs no pixel allocation. */
#endif
    lv_display_t *d = esp_lv_present_native_get_display(b);
    lv_obj_t *s = lv_display_get_screen_active(d);
    lv_obj_remove_style_all(s);
    lv_obj_set_style_bg_color(s, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_refr_now(d);
    OK(esp_lv_present_native_get_error(b));
    OK(esp_display_present_engine_service(p));
    bool known = false;
    for (size_t i = 0; i < count; i++) {
        known |= buffers[i] == panel.pending.pixels;
    }
    assert(known);
    assert(lv_display_get_buf_active(d)->data != panel.pending.pixels);
    uint8_t saved[W * H * 2];
    memcpy(saved, panel.pending.pixels, sizeof(saved));
    for (unsigned i = 0; i < W * H; i++) {
        uint16_t v;
        memcpy(&v, saved + 2 * i, 2);
        assert(v == 0xf800);
    }
    if (timeout) {
        /* One free slot can be queued; third admission must fail safely. */
        lv_obj_set_style_bg_color(s, lv_color_hex(0x00ff00), 0);
        lv_refr_now(d);
        const void *queued = buffers[0] == panel.pending.pixels ? buffers[1] : buffers[0];
        uint8_t queued_pixels[W * H * 2];
        memcpy(queued_pixels, queued, sizeof(queued_pixels));
        lv_refr_now(d);
        OK(esp_lv_present_native_get_error(b)); /* No damage: do not request a third buffer. */
        lv_obj_set_style_bg_color(s, lv_color_hex(0x0000ff), 0);
        lv_refr_now(d);
        assert(esp_lv_present_native_get_error(b) == ESP_ERR_TIMEOUT);
        assert(!memcmp(saved, panel.pending.pixels, sizeof(saved)));
        assert(!memcmp(queued_pixels, queued, sizeof(queued_pixels)));
        OK(esp_lv_present_native_destroy(b));
        scan_confirm(&panel);
        OK(esp_display_present_engine_service(p));
        if (panel.cookie) {
            scan_confirm(&panel);
        }
    } else {
        scan_confirm(&panel);
        OK(esp_display_present_engine_drain(p, 0));
        lv_refr_now(d); /* No invalidations must return the unused lease. */
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(p, &status));
        assert(!status.frame_active);
        lv_obj_set_style_bg_color(s, lv_color_hex(0x00ff00), 0);
        lv_refr_now(d);
        OK(esp_lv_present_native_get_error(b));
        OK(esp_display_present_engine_service(p));
        assert(!memcmp(saved, panel.current.pixels, sizeof(saved)));
        OK(esp_lv_present_native_destroy(b)); /* Submitted storage belongs to backend, not binding. */
        scan_confirm(&panel);
        for (unsigned i = 0; i < W * H; i++) {
            uint16_t v;
            memcpy(&v, (uint8_t*)panel.current.pixels + 2 * i, 2);
            assert(v == 0x07e0);
        }
    }
    OK(esp_display_present_engine_drain(p, 0));
    OK(esp_display_present_engine_stop(p, 1000));
    OK(esp_display_present_engine_destroy(p));
}
#if LVGL_VERSION_MINOR >= 5
/* Binding creation reserves a writable surface before mounting LVGL. An
 * immediate close must revoke it without submitting or leaking a frame. */
static void destroy_before_refresh(esp_display_present_present_mode_t mode)
{
    scan_panel_t panel = {.immediate = true};
    esp_display_present_engine_config_t cfg = {
        .backend = {
            .width = W, .height = H, .tile_rows = 2, .buffer_count = 2, .max_frames = 4,
            .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = scan_interface(&panel)
        },
        .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = mode, .timeout_ms = 1000
    };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&cfg, &p));
    unsigned before = canvas_allocations;
    esp_lv_present_native_t *binding;
    OK(esp_lv_present_native_create_zero_copy(p, &binding));
    assert(canvas_allocations == before);
    esp_display_present_engine_status_t status;
    OK(esp_display_present_engine_get_status(p, &status));
    assert(status.frame_active);
    OK(esp_lv_present_native_destroy(binding));
    OK(esp_display_present_engine_get_status(p, &status));
    assert(!status.frame_active && !panel.cookie);
    OK(esp_display_present_engine_stop(p, 1000));
    OK(esp_display_present_engine_destroy(p));
}
static void pause_copy(void)
{
    const struct timespec delay = {.tv_nsec = 1000000};
    nanosleep(&delay, NULL);
}
/* A persistent logical canvas must retain disjoint damage across frames while
 * the physical candidate alternates. It must never alias current scanout. */
static void check_direct(scan_panel_t *panel, unsigned rotation, unsigned changed, bool swap)
{
    unsigned pw = rotation & 1 ? H : W;
    for (unsigned y = 0; y < H; ++y) {
        for (unsigned x = 0; x < W; ++x) {
            unsigned px = x, py = y;
            switch (rotation) {
            case 1: px = H - 1 - y; py = x; break;
            case 2: px = W - 1 - x; py = H - 1 - y; break;
            case 3: px = y; py = W - 1 - x; break;
            }
            uint16_t pixel;
            memcpy(&pixel, (uint8_t *)panel->current.pixels + (py * pw + px) * 2, 2);
            if (swap) {
                pixel = (uint16_t)((pixel << 8) | (pixel >> 8));
            }
            uint16_t expected = (changed & 1) && x >= 1 && x < 3 && y >= 1 && y < 3 ? 0xf800 :
                                (changed & 2) && x >= 5 && x < 7 && y >= 3 && y < 5 ? 0x001f : 0xffff;
            assert(pixel == expected);
        }
    }
}
static esp_err_t checked_direct_copy(void *user, const esp_display_present_copy_request_t *request)
{
    (void)user;
    uint8_t saved[W * H * 2];
    assert(request->src.capacity_bytes <= sizeof(saved));
    memcpy(saved, request->src.pixels, request->src.capacity_bytes);
    pause_copy(); /* Keep conversion alive while a following LVGL area is ready. */
    assert(!memcmp(saved, request->src.pixels, request->src.capacity_bytes));
    esp_err_t result = esp_display_present_pixel_execute_cpu(request);
    assert(!memcmp(saved, request->src.pixels, request->src.capacity_bytes));
    return result;
}
static void exercise_direct(unsigned rotation, bool async, bool swap)
{
    scan_panel_t panel = {.immediate = rotation == 0};
    esp_display_present_engine_config_t cfg = {.backend = {.width = W, .height = H, .tile_rows = 2,
                                                           .buffer_count = 2, .max_frames = 4, .rotation = rotation, .async_copy = async, .copy_execute = checked_direct_copy,
                                                           .panel_order = swap ? ESP_DISPLAY_PRESENT_ORDER_SWAPPED : ESP_DISPLAY_PRESENT_ORDER_NATIVE,
                                                           .format = ESP_DISPLAY_PRESENT_FORMAT_RGB565, .panel = scan_interface(&panel)
                                                          },
                                               .backend_kind = ESP_DISPLAY_PRESENT_ENGINE_FB, .mode = ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT,
                                               .timeout_ms = 1000
                                              };
    esp_display_present_engine_t *p;
    OK(esp_display_present_engine_create(&cfg, &p));
    esp_lv_present_native_t *b;
    unsigned before = canvas_allocations;
    esp_err_t result = esp_lv_present_native_create_zero_copy(p, &b);
    assert(canvas_allocations == before); /* No canvas, including layout rejection. */
    if (rotation || swap) {
        assert(result == ESP_ERR_NOT_SUPPORTED);
        OK(esp_lv_present_native_create(p, 2, &b));
    } else {
        OK(result);
    }
    {
        lv_display_t *d = esp_lv_present_native_get_display(b);
        lv_obj_t *screen = lv_display_get_screen_active(d);
        lv_obj_remove_style_all(screen);
        lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_refr_now(d);
        OK(esp_lv_present_native_get_error(b));
        /* Async conversion and backend worker require pumping before confirmation. */
        for (unsigned i = 0; !panel.cookie && !panel.immediate && i < 10000; ++i) {
            OK(esp_display_present_engine_service(p));
            pause_copy();
        }
        if (!panel.immediate) {
            assert(panel.cookie);
            scan_confirm(&panel);
        }
        OK(esp_display_present_engine_drain(p, 1000));
        check_direct(&panel, rotation, false, swap);
        lv_obj_t *boxes[2];
        for (unsigned i = 0; i < 2; ++i) {
            boxes[i] = lv_obj_create(screen);
            lv_obj_remove_style_all(boxes[i]);
            lv_obj_set_pos(boxes[i], i ? 5 : 1, i ? 3 : 1);
            lv_obj_set_size(boxes[i], 2, 2);
            lv_obj_set_style_bg_color(boxes[i], lv_color_white(), 0);
            lv_obj_set_style_bg_opa(boxes[i], LV_OPA_COVER, 0);
        }
        unsigned changed = 0;
        for (unsigned frame = 0; frame < 16; ++frame) {
            uint8_t saved[W * H * 2];
            void *current = panel.current.pixels;
            memcpy(saved, current, sizeof(saved));
            unsigned box = frame & 1;
            changed ^= 1u << box;
            lv_obj_set_style_bg_color(boxes[box], (changed & (1u << box)) ?
                                      lv_color_hex(box ? 0x0000ff : 0xff0000) : lv_color_white(), 0);
            lv_refr_now(d);
            OK(esp_lv_present_native_get_error(b));
            for (unsigned i = 0; !panel.cookie && !panel.immediate && i < 10000; ++i) {
                OK(esp_display_present_engine_service(p));
                pause_copy();
            }
            if (panel.immediate) {
                OK(esp_display_present_engine_drain(p, 1000));
            }
            assert((panel.immediate ? panel.current.pixels : panel.pending.pixels) != current);
            assert(!memcmp(saved, current, sizeof(saved)));
            if (!panel.immediate) {
                assert(panel.cookie);
                scan_confirm(&panel);
            }
            OK(esp_display_present_engine_drain(p, 1000));
            check_direct(&panel, rotation, changed, swap);
        }
        /* No damage must not publish another frame or retain a write lease. */
        size_t submissions = panel.submissions;
        lv_refr_now(d);
        OK(esp_lv_present_native_get_error(b));
        assert(panel.submissions == submissions);
        esp_display_present_engine_status_t status;
        OK(esp_display_present_engine_get_status(p, &status));
        assert(!status.frame_active);
        /* Transition from a logical canvas to physical DIRECT must remain safe. */
        panel.immediate = true;
        OK(esp_lv_present_native_set_rotation(b, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0, 1000));
        lv_obj_clean(screen);
        lv_refr_now(d);
        OK(esp_lv_present_native_get_error(b));
        for (unsigned i = 0; !panel.cookie && !panel.immediate && i < 10000; ++i) {
            OK(esp_display_present_engine_service(p));
            pause_copy();
        }
        if (!panel.immediate) {
            assert(panel.cookie);
            scan_confirm(&panel);
        }
        OK(esp_display_present_engine_drain(p, 1000));
        for (unsigned i = 0; i < W * H; ++i) {
            uint16_t pixel;
            memcpy(&pixel, (uint8_t *)panel.current.pixels + 2 * i, 2);
            assert(pixel == 0xffff);
        }
        if (!rotation && !swap) {
            unsigned before_rotation = canvas_allocations;
            OK(esp_lv_present_native_set_rotation(b, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, 1000));
            assert(canvas_allocations == before_rotation + 1); /* Actual DIRECT copy source. */
            lv_refr_now(d);
            OK(esp_lv_present_native_get_error(b));
            OK(esp_display_present_engine_drain(p, 1000));
            before_rotation = canvas_allocations;
            OK(esp_lv_present_native_set_rotation(b, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0, 1000));
            assert(canvas_allocations == before_rotation); /* Borrow again, no placeholder. */
            lv_refr_now(d);
            OK(esp_lv_present_native_get_error(b));
            OK(esp_display_present_engine_drain(p, 1000));
        }
        OK(esp_lv_present_native_destroy(b));
    }
    OK(esp_display_present_engine_stop(p, 1000));
    OK(esp_display_present_engine_destroy(p));
}
#endif
int main(void)
{
    lv_init();
    exercise(false, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL);
    exercise(true, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL);
    exercise(false, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT);
    exercise(true, ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT);
#if LVGL_VERSION_MINOR >= 5
    destroy_before_refresh(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_FULL);
    destroy_before_refresh(ESP_DISPLAY_PRESENT_ENGINE_MODE_DOUBLE_DIRECT);
    for (unsigned r = 0; r < 4; ++r) {
        exercise_direct(r, false, false);
        exercise_direct(r, true, false);
        exercise_direct(r, false, true);
        exercise_direct(r, true, true);
    }
#endif
    lv_deinit();
    puts("LVGL zero-copy: lease pixels, detach, backpressure and destroy lifetime passed");
}
