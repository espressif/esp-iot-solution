/*
 * SPDX-FileCopyrightText: 2026 youkorr
 *
 * SPDX-License-Identifier: MIT
 */

#include "sdkconfig.h"

#if CONFIG_ESP_LVGL_ADAPTER_ROTATE_IN_DRAW && CONFIG_ESP_LVGL_ADAPTER_ROTATE_IMAGE_CACHE_SIZE_KB > 0
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_lv_adapter.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "../../../src/display/rotate_in_draw/lv_draw_ortho.h"

/* Flash-backed pixels: the normal admission rule, without a test override. */
static const uint8_t s_pixels[256 * 128 * 2] = {0};

TEST_CASE("rotate in draw caches immutable images and excludes modifiable ones", "[adapter][ortho_cache]")
{
    /* Options are applied by lv_async_call on the LVGL task after setup. */
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ESP_OK(esp_lv_adapter_lock(pdMS_TO_TICKS(1000)));
    lv_display_t *disp = lv_display_get_default();
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_TRUE(lv_display_get_matrix_rotation(disp));
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_clean(scr);
    lv_image_dsc_t image = {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = 256,
            .h = 128,
            .stride = 256 * 2,
        },
        .data_size = sizeof(s_pixels),
        .data = s_pixels,
    };
    lv_obj_t *obj = lv_image_create(scr);
    lv_image_set_src(obj, &image);
    lv_draw_ortho_reset_stats();
    for (int i = 0; i < 3; i++) {
        lv_obj_invalidate(obj);
        lv_refr_now(disp);
    }
    const lv_draw_ortho_stats_t *stats = lv_draw_ortho_get_stats();
    TEST_ASSERT_GREATER_THAN(0, stats->image_cache_builds);
    TEST_ASSERT_GREATER_THAN(0, stats->images_cached);
    TEST_ASSERT_GREATER_THAN(0, stats->image_cache_bytes);
    TEST_ASSERT_LESS_OR_EQUAL(CONFIG_ESP_LVGL_ADAPTER_ROTATE_IMAGE_CACHE_SIZE_KB * 1024, stats->image_cache_bytes);

    image.header.flags |= LV_IMAGE_FLAGS_MODIFIABLE;
    lv_image_cache_drop(&image);
    lv_draw_ortho_reset_stats();
    lv_obj_invalidate(obj);
    lv_refr_now(disp);
    TEST_ASSERT_EQUAL_UINT32(0, lv_draw_ortho_get_stats()->images_cached);
    TEST_ASSERT_GREATER_THAN(0, lv_draw_ortho_get_stats()->images_turned);
    lv_obj_clean(scr);
    esp_lv_adapter_unlock();
    /* The shared tearDown deinitializes LVGL and checks the heap. Retained
     * cache pixels must be freed there, rather than leaking across restarts. */
}
#endif
