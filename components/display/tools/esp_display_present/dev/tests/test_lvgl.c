/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_ops.h"
#include "core/present_engine.h"
#include "present_lvgl.h"
#if LVGL_VERSION_MINOR == 0
#include "src/display/lv_display_private.h"
#define lv_display_get_buf_active(display) ((display)->buf_act)
#endif
#include "test_panel.h"
#include <stdio.h>

static esp_display_present_engine_t *make(test_panel_t *panel)
{
    esp_display_present_engine_config_t config = {.backend = {.width = panel->width, .height = panel->height,
                                                              .tile_rows = 2, .buffer_count = 2, .max_frames = 2,
                                                              .format = panel->bpp == 2 ? ESP_DISPLAY_PRESENT_FORMAT_RGB565 : ESP_DISPLAY_PRESENT_FORMAT_RGB888,
                                                              .panel = test_panel_interface(panel)
                                                             }
                                                 };
    if (!(panel->width % 2) && !(panel->height % 2))
        config.output_caps = (esp_display_present_output_caps_t) {
        .x_alignment = 2, .y_alignment = 2, .w_align = 2, .h_align = 2
    };
    esp_display_present_engine_t *p;
    assert(esp_display_present_engine_create(&config, &p) == ESP_OK);
    return p;
}

static uint16_t pixel(test_panel_t *p, unsigned x, unsigned y)
{
    uint16_t color;
    memcpy(&color, p->pixels + (y * p->width + x) * 2, 2);
    return color;
}

static void complete_all(test_panel_t *p)
{
    while (p->count) {
        test_panel_complete(p);
    }
}

int main(void)
{
    lv_init();
    test_panel_t panel = {.width = 16, .height = 12, .bpp = 2, .service_previous = true};
    esp_display_present_engine_t *p = make(&panel);
    esp_lv_present_native_t *binding;
    /* LVGL renders four rows at a time, backend holds only two full rows. */
    assert(esp_lv_present_native_create(p, 4, &binding) == ESP_OK);
    lv_display_t *display = esp_lv_present_native_get_display(binding);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_OK && panel.count == 1);
    assert(panel.submitted >= 6);
    esp_display_present_engine_status_t status;
    assert(esp_display_present_engine_get_status(p, &status) == ESP_OK && status.committed_frames == 1);
    /* flush-ready has returned control to LVGL. Reusing its source cannot
     * change the still-inflight backend source read by simulated DMA. */
    lv_draw_buf_t *draw = lv_display_get_buf_active(display);
    memset(draw->data, 0, draw->data_size);
    complete_all(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    for (unsigned y = 0; y < 12; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            assert(pixel(&panel, x, y) == 0xFFFF);
        }
    }
    lv_obj_t *box = lv_obj_create(screen);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 3, 2);
    lv_obj_set_size(box, 4, 3);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_OK);
    complete_all(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    for (unsigned y = 0; y < 12; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            uint16_t expected = x >= 3 && x < 7 && y >= 2 && y < 5 ? 0xF800 : 0xFFFF;
            assert(pixel(&panel, x, y) == expected);
        }
    }

    /* Independent binding/display, no singleton or global tick replacement. */
    test_panel_t second = {.width = 8, .height = 8, .bpp = 2, .immediate = true};
    esp_display_present_engine_t *other = make(&second);
    esp_lv_present_native_t *other_binding;
    assert(esp_lv_present_native_create(other, 2, &other_binding) == ESP_OK);
    lv_display_t *other_display = esp_lv_present_native_get_display(other_binding);
    lv_obj_t *other_screen = lv_display_get_screen_active(other_display);
    lv_obj_remove_style_all(other_screen);
    lv_obj_set_style_bg_color(other_screen, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(other_screen, LV_OPA_COVER, 0);
    lv_refr_now(other_display);
    assert(esp_lv_present_native_get_error(other_binding) == ESP_OK);
    assert(pixel(&second, 2, 2) == 0x07E0 && pixel(&panel, 3, 2) == 0xF800);
    assert(esp_lv_present_native_destroy(other_binding) == ESP_OK);
    assert(esp_display_present_engine_stop(other, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(other) == ESP_OK);

    /* Odd-width RGB888 includes LVGL row padding and BGR channel storage. */
    test_panel_t rgb = {.width = 5, .height = 5, .bpp = 3, .immediate = true};
    other = make(&rgb);
    assert(esp_lv_present_native_create(other, 3, &other_binding) == ESP_OK);
    other_display = esp_lv_present_native_get_display(other_binding);
    other_screen = lv_display_get_screen_active(other_display);
    lv_obj_remove_style_all(other_screen);
    lv_obj_set_style_bg_color(other_screen, lv_color_hex(0x123456), 0);
    lv_obj_set_style_bg_opa(other_screen, LV_OPA_COVER, 0);
    lv_refr_now(other_display);
    assert(esp_lv_present_native_get_error(other_binding) == ESP_OK);
    for (size_t i = 0; i < 25; ++i) {
        assert(rgb.pixels[i * 3] == 0x12 && rgb.pixels[i * 3 + 1] == 0x34 && rgb.pixels[i * 3 + 2] == 0x56);
    }
    assert(esp_lv_present_native_destroy(other_binding) == ESP_OK);
    assert(esp_display_present_engine_stop(other, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(other) == ESP_OK);

    /* Reconfigure the live binding while preserving its object tree. */
    assert(esp_lv_present_native_set_rotation(binding, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, 0) == ESP_OK);
    assert(lv_display_get_screen_active(display) == screen);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_OK);
    complete_all(&panel);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    for (unsigned y = 0; y < 12; y++)
        for (unsigned x = 0; x < 16; x++) {
            uint32_t lx, ly;
            assert(esp_display_present_unrotate_point(12, 16, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, x, y, &lx, &ly) == ESP_OK);
            assert(pixel(&panel, x, y) == (lx >= 3 && lx < 7 && ly >= 2 && ly < 5 ? 0xF800 : 0xFFFF));
        }

    panel.reject = true;
    lv_obj_set_style_bg_color(box, lv_color_hex(0x0000FF), 0);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_FAIL);
    size_t submitted = panel.submitted;
    lv_tick_inc(100);
    lv_timer_handler();
    assert(panel.submitted == submitted);
    assert(esp_lv_present_native_destroy(binding) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);

    /* Pooled PARTIAL: non-square rotation must retain a safe tile capacity. */
    test_panel_t pool_panel = {.width = 16, .height = 12, .bpp = 2, .immediate = true};
    p = make(&pool_panel);
#if LVGL_VERSION_MINOR < 5
    assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_ERR_NOT_SUPPORTED);
    assert(esp_lv_present_native_create(p, 2, &binding) == ESP_OK);
#else
    assert(esp_lv_present_native_create_pooled(p, 2, &binding) == ESP_OK);
#endif
    display = esp_lv_present_native_get_display(binding);
    screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_OK);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    assert(esp_lv_present_native_set_rotation(binding, ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90, 0) == ESP_OK);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_OK);
    assert(esp_display_present_engine_drain(p, 0) == ESP_OK);
    for (unsigned y = 0; y < 12; ++y)
        for (unsigned x = 0; x < 16; ++x) {
            assert(pixel(&pool_panel, x, y) == 0xffff);
        }
    assert(esp_lv_present_native_destroy(binding) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);

    /* Error after a prior band was accepted: destroy must retain cancellation
     * state until delayed DMA is finished, then allow retry. */
    test_panel_t partial = {.width = 16, .height = 12, .bpp = 2,
                            .service_previous = true, .reject_after = 2
                           };
    p = make(&partial);
    assert(esp_lv_present_native_create(p, 4, &binding) == ESP_OK);
    display = esp_lv_present_native_get_display(binding);
    screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_refr_now(display);
    assert(esp_lv_present_native_get_error(binding) == ESP_FAIL && partial.count == 1);
    assert(esp_lv_present_native_destroy(binding) == ESP_ERR_TIMEOUT);
    complete_all(&partial);
    assert(esp_lv_present_native_destroy(binding) == ESP_OK);
    assert(esp_display_present_engine_stop(p, 0) == ESP_OK);
    assert(esp_display_present_engine_destroy(p) == ESP_OK);
    lv_deinit();
    puts("PASS real LVGL: band splitting, delayed DMA source isolation, partial image, multiple displays, error pause");
    return 0;
}
