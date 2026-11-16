/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_utils_memory.h"

#include "lvgl.h"
#include "lv_eaf.h"
#include "esp_eaf_player.h"

static const char *TAG = "eaf player";

#define TEST_LCD_H_RES      240
#define TEST_LCD_V_RES      240

/* Procedural fixtures generated locally at configure time. */
extern const uint8_t legacy_eaf_start[] asm("_binary_legacy_eaf_start");
extern const uint8_t legacy_eaf_end[] asm("_binary_legacy_eaf_end");
extern const uint8_t lossless_eaf4_start[] asm("_binary_lossless_eaf4_start");
extern const uint8_t lossless_eaf4_end[] asm("_binary_lossless_eaf4_end");

static SemaphoreHandle_t flush_sem;
static lv_display_t *s_disp;
static lv_color_t *s_draw_buf_mem;

static void test_flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    LV_UNUSED(px_map);
    ESP_LOGI(TAG, "flush_cb [%d,%d,%d,%d]", area->x1, area->y1, area->x2, area->y2);
    lv_display_flush_ready(disp);
    xSemaphoreGive(flush_sem);
}

static void lvgl_test_env_init(void)
{
    lv_init();

    uint32_t buffer_pixels = TEST_LCD_H_RES * TEST_LCD_V_RES;
    s_draw_buf_mem = heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    TEST_ASSERT_NOT_NULL(s_draw_buf_mem);

    s_disp = lv_display_create(TEST_LCD_H_RES, TEST_LCD_V_RES);
    TEST_ASSERT_NOT_NULL(s_disp);
    lv_display_set_flush_cb(s_disp, test_flush_callback);
    lv_display_set_buffers(s_disp, s_draw_buf_mem, NULL, buffer_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    flush_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(flush_sem);
}

static void lvgl_test_env_deinit(void)
{
    lv_display_delete(s_disp);
    s_disp = NULL;
    free(s_draw_buf_mem);
    s_draw_buf_mem = NULL;

    lv_deinit();

    vSemaphoreDelete(flush_sem);
    flush_sem = NULL;
}

static void validate_embedded_animation(const uint8_t *start, const uint8_t *end, const char *label)
{
    lvgl_test_env_init();

    size_t eaf_size = end - start;
    ESP_LOGI(TAG, "Embedded %s size: %u bytes", label, (unsigned)eaf_size);

    lv_obj_t *anim = lv_eaf_create(lv_scr_act());
    TEST_ASSERT_NOT_NULL(anim);

    lv_eaf_set_src_data(anim, start, eaf_size);
    TEST_ASSERT_TRUE(lv_eaf_is_loaded(anim));

    int32_t total_frames = lv_eaf_get_total_frames(anim);
    ESP_LOGI(TAG, "Total frames: %d", total_frames);
    TEST_ASSERT_GREATER_THAN(0, total_frames);
    TEST_ASSERT_EQUAL_INT32(0, lv_eaf_get_current_frame(anim));

    lv_refr_now(NULL);
    TEST_ASSERT_TRUE(xSemaphoreTake(flush_sem, pdMS_TO_TICKS(3000)));

    lv_obj_delete(anim);
    lvgl_test_env_deinit();
}

TEST_CASE("Load legacy EAF palette animation", "[eaf][legacy][lv_eaf_set_src]")
{
    validate_embedded_animation(legacy_eaf_start, legacy_eaf_end, "legacy EAF");
}

TEST_CASE("Load EAF4 lossless animation", "[eaf][eaf4][lossless][lv_eaf_set_src]")
{
    validate_embedded_animation(lossless_eaf4_start, lossless_eaf4_end, "EAF4 lossless");
}

TEST_CASE("Configure playback before loading a source", "[eaf][api][configuration]")
{
    lvgl_test_env_init();
    lv_obj_t *anim = lv_eaf_create(lv_screen_active());
    TEST_ASSERT_NOT_NULL(anim);

    lv_eaf_set_frame_delay(anim, 40);
    lv_eaf_set_loop_count(anim, 3);
    lv_eaf_set_loop_enabled(anim, false);
    TEST_ASSERT_EQUAL_UINT32(40, lv_eaf_get_frame_delay(anim));
    TEST_ASSERT_EQUAL_INT32(3, lv_eaf_get_loop_count(anim));
    TEST_ASSERT_FALSE(lv_eaf_get_loop_enabled(anim));

    lv_obj_delete(anim);
    lv_timer_handler();
    lvgl_test_env_deinit();
}

TEST_CASE("Decode without an LVGL widget", "[eaf][core][gui-independent]")
{
    esp_eaf_player_t *player = NULL;
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    TEST_ESP_OK(esp_eaf_player_open_memory(legacy_eaf_start,
                                           legacy_eaf_end - legacy_eaf_start,
                                           &config, &player));
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(player);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(player);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL(ESP_EAF_CONTAINER_EAF, info->container);
    TEST_ASSERT_GREATER_THAN(0, info->frame_count);
    TEST_ASSERT_NOT_NULL(frame->pixels);
    TEST_ESP_OK(esp_eaf_player_next(player));
    esp_eaf_player_close(player);
}

TEST_CASE("Playback configuration survives replay and failed source", "[eaf][playback][regression]")
{
    lvgl_test_env_init();
    lv_obj_t *anim = lv_eaf_create(lv_scr_act());
    lv_eaf_set_loop_count(anim, 1);
    lv_eaf_set_src_data(anim, lossless_eaf4_start, lossless_eaf4_end - lossless_eaf4_start);
    TEST_ASSERT_EQUAL_UINT32(67, lv_eaf_get_frame_delay(anim));
    for (int i = 0; i < 5; i++) {
        lv_tick_inc(70);
        lv_timer_handler();
    }
    TEST_ASSERT_EQUAL_INT32(1, lv_eaf_get_loop_count(anim));
    lv_eaf_restart(anim);
    TEST_ASSERT_EQUAL_INT32(0, lv_eaf_get_current_frame(anim));
    TEST_ASSERT_EQUAL_INT32(1, lv_eaf_get_loop_count(anim));
    for (int i = 0; i < 3; i++) {
        lv_tick_inc(70);
        lv_timer_handler();
        TEST_ASSERT_EQUAL_INT32((i + 1) % 2, lv_eaf_get_current_frame(anim));
    }
    lv_eaf_set_frame_delay(anim, 0);
    lv_eaf_set_src_data(anim, lossless_eaf4_start, lossless_eaf4_end - lossless_eaf4_start);
    TEST_ASSERT_EQUAL_UINT32(0, lv_eaf_get_frame_delay(anim));
    static const uint8_t invalid[] = {1, 2, 3, 4};
    lv_eaf_set_src_data(anim, invalid, sizeof(invalid));
    TEST_ASSERT_FALSE(lv_eaf_is_loaded(anim));
    lv_tick_inc(100);
    lv_timer_handler();
    TEST_ASSERT_EQUAL_INT32(-1, lv_eaf_get_current_frame(anim));
    lv_obj_delete(anim);
    lvgl_test_env_deinit();
}

TEST_CASE("Pause and resume preserve animation progress", "[eaf][playback][pause-resume]")
{
    lvgl_test_env_init();
    lv_obj_t *anim = lv_eaf_create(lv_screen_active());
    lv_eaf_set_src_data(anim, lossless_eaf4_start, lossless_eaf4_end - lossless_eaf4_start);
    TEST_ASSERT_EQUAL_INT32(2, lv_eaf_get_total_frames(anim));
    TEST_ASSERT_EQUAL_INT32(0, lv_eaf_get_current_frame(anim));
    lv_tick_inc(70);
    lv_timer_handler();
    TEST_ASSERT_EQUAL_INT32(1, lv_eaf_get_current_frame(anim));
    lv_eaf_pause(anim);
    lv_tick_inc(210);
    lv_timer_handler();
    TEST_ASSERT_EQUAL_INT32(1, lv_eaf_get_current_frame(anim));
    lv_eaf_resume(anim);
    lv_tick_inc(70);
    lv_timer_handler();
    TEST_ASSERT_EQUAL_INT32(0, lv_eaf_get_current_frame(anim));
    lv_tick_inc(70);
    lv_timer_handler();
    TEST_ASSERT_EQUAL_INT32(1, lv_eaf_get_current_frame(anim));
    lv_eaf_restart(anim);
    TEST_ASSERT_EQUAL_INT32(0, lv_eaf_get_current_frame(anim));
    lv_obj_delete(anim);
    lvgl_test_env_deinit();
}

#define TEST_MEMORY_LEAK_THRESHOLD  (700)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    unity_run_menu();
}
