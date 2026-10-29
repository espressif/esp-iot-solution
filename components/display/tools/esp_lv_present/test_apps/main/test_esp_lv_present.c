/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_display_present.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_log.h"
#include "esp_lv_present.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

static const char *TAG = "lv_present_test";

#define TEST_LCD_H_RES              100
#define TEST_LCD_V_RES              100
#define TEST_MEMORY_LEAK_THRESHOLD  (2000)
#define TEST_QUIESCE_TIMEOUT_MS     (2000)

typedef struct {
    esp_lcd_panel_io_t base;
    esp_lcd_panel_io_color_trans_done_cb_t on_color_trans_done;
    void *user_ctx;
    esp_timer_handle_t done_timer;
} dummy_io_t;

typedef struct {
    esp_lcd_panel_t base;
} dummy_panel_t;

static dummy_io_t s_dummy_io;
static dummy_panel_t s_dummy_panel;

static void IRAM_ATTR dummy_color_done_timer(void *arg)
{
    dummy_io_t *io = arg;
    if (io->on_color_trans_done != NULL) {
        (void)io->on_color_trans_done(&io->base, NULL, io->user_ctx);
    }
}

static esp_err_t dummy_io_register_event_callbacks(
    esp_lcd_panel_io_t *io,
    const esp_lcd_panel_io_callbacks_t *cbs,
    void *user_ctx)
{
    dummy_io_t *dummy = (dummy_io_t *)io;
    dummy->on_color_trans_done = (cbs != NULL) ? cbs->on_color_trans_done : NULL;
    dummy->user_ctx = user_ctx;
    return ESP_OK;
}

static esp_err_t dummy_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start,
                                   int x_end, int y_end, const void *color_data)
{
    (void)panel;
    (void)x_start;
    (void)y_start;
    (void)x_end;
    (void)y_end;
    (void)color_data;
    (void)esp_timer_stop(s_dummy_io.done_timer);
    (void)esp_timer_start_once(s_dummy_io.done_timer, 0);
    return ESP_OK;
}

static void dummy_panel_init(void)
{
    memset(&s_dummy_io, 0, sizeof(s_dummy_io));
    memset(&s_dummy_panel, 0, sizeof(s_dummy_panel));
    s_dummy_io.base.register_event_callbacks = dummy_io_register_event_callbacks;
    s_dummy_panel.base.draw_bitmap = dummy_draw_bitmap;

    const esp_timer_create_args_t timer_args = {
        .callback = dummy_color_done_timer,
        .arg = &s_dummy_io,
        .dispatch_method = ESP_TIMER_ISR,
        .name = "dummy_lcd_done",
    };
    TEST_ESP_OK(esp_timer_create(&timer_args, &s_dummy_io.done_timer));
}

static void dummy_panel_deinit(void)
{
    if (s_dummy_io.done_timer != NULL) {
        (void)esp_timer_stop(s_dummy_io.done_timer);
        TEST_ESP_OK(esp_timer_delete(s_dummy_io.done_timer));
        s_dummy_io.done_timer = NULL;
    }
}

static void test_presenter_create(esp_display_presenter_t **out_presenter)
{
    dummy_panel_init();

    const esp_display_present_target_config_t target = {
        .hw = {
            .panel = &s_dummy_panel.base,
            .io = &s_dummy_io.base,
            .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
            .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = ESP_DISPLAY_PRESENT_ROTATE_0,
            .swap_bytes = false,
        },
        .fb = {
            .mode = ESP_DISPLAY_PRESENT_MODE_NONE,
        },
    };
    const esp_display_presenter_config_t config = {
        .width = TEST_LCD_H_RES,
        .height = TEST_LCD_V_RES,
        .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        .max_damage_areas = 8,
        .target = target,
    };
    TEST_ESP_OK(esp_display_presenter_create(&config, out_presenter));
    TEST_ASSERT_NOT_NULL(*out_presenter);
}

static void test_presenter_delete(esp_display_presenter_t *presenter)
{
    if (presenter != NULL) {
        TEST_ESP_OK(esp_display_presenter_delete(presenter));
    }
    dummy_panel_deinit();
}

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

TEST_CASE("lv present start/stop arguments", "[lv_present][basic]")
{
    esp_display_presenter_t *presenter = NULL;
    lv_display_t *disp = NULL;

    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_lv_present_start(NULL, &disp));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, esp_lv_present_stop());

    test_presenter_create(&presenter);
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, esp_lv_present_start(presenter, NULL));
    TEST_ESP_OK(esp_lv_present_start(presenter, &disp));
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, esp_lv_present_start(presenter, &disp));

    TEST_ESP_OK(esp_display_presenter_quiesce(presenter, TEST_QUIESCE_TIMEOUT_MS));
    TEST_ESP_OK(esp_lv_present_stop());
    test_presenter_delete(presenter);
}

TEST_CASE("lv present flushes through presenter", "[lv_present][basic]")
{
    esp_display_presenter_t *presenter = NULL;
    lv_display_t *disp = NULL;
    test_presenter_create(&presenter);

    TEST_ESP_OK(esp_lv_present_start(presenter, &disp));
    TEST_ASSERT_NOT_NULL(disp);

    lv_obj_t *label = lv_label_create(lv_display_get_screen_active(disp));
    lv_label_set_text(label, "esp_lv_present");
    lv_obj_center(label);
    lv_refr_now(disp);

    TEST_ASSERT_TRUE(esp_lv_present_get_frame_count() > 0);
    ESP_LOGI(TAG, "committed %" PRIu32 " frame(s)", esp_lv_present_get_frame_count());

    TEST_ESP_OK(esp_display_presenter_quiesce(presenter, TEST_QUIESCE_TIMEOUT_MS));
    TEST_ESP_OK(esp_lv_present_stop());
    test_presenter_delete(presenter);
}

void app_main(void)
{
    printf("ESP LV Present TEST\n");
    unity_run_menu();
}
