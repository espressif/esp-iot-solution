/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "touch_slider_sensor.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-300)

#if CONFIG_IDF_TARGET_ESP32P4
static uint32_t channel_list[] = {3, 2, 1, 0};
static float threshold[] = {0.001f, 0.002f, 0.002f, 0.001f};
static int pad_num = 4;
#else
static uint32_t channel_list[] = {2, 4, 6, 12, 10, 8};
static float threshold[] = {0.005f, 0.005f, 0.005f, 0.005f, 0.005f, 0.005f};
static int pad_num = 6;
#endif

static touch_slider_handle_t s_touch_slider = NULL;
static size_t before_free_8bit;
static size_t before_free_32bit;

static void touch_slider_event_callback(touch_slider_handle_t handle, touch_slider_event_t event, int32_t data, void *cb_arg)
{
    if (event == TOUCH_SLIDER_EVENT_RIGHT_SWIPE) {
        printf("Right swipe (speed)\n");
    } else if (event == TOUCH_SLIDER_EVENT_LEFT_SWIPE) {
        printf("Left swipe (speed)\n");
    } else if (event == TOUCH_SLIDER_EVENT_RELEASE) {
        printf("Slide %ld\n", data);
        if (data > 1000) {
            printf("Right swipe (displacement)\n");
        } else if (data < -1000) {
            printf("Left swipe (displacement)\n");
        }
    } else if (event == TOUCH_SLIDER_EVENT_POSITION) {
        // printf("pos,%" PRId64 ",%lu\n", get_time_in_ms(), data);
    }
}

TEST_CASE("touch slider sensor create/delete test", "[touch_slider][create]")
{
    touch_slider_config_t config = {
        .channel_num = pad_num,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .position_range = 10000,
        .channel_gold_value = NULL,
        .debounce_times = 3,
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_create(&config, &s_touch_slider, touch_slider_event_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_slider);

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_delete(s_touch_slider));
    s_touch_slider = NULL;
}

TEST_CASE("touch slider sensor position test", "[touch_slider][events]")
{
    touch_slider_config_t config = {
        .channel_num = pad_num,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .filter_reset_times = 50,
        .position_range = 10000,
        .channel_gold_value = NULL,
        .debounce_times = 0,
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_create(&config, &s_touch_slider, touch_slider_event_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_slider);

    // Test handle_events API
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_slider_sensor_handle_events(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_handle_events(s_touch_slider));

    // Let it run for a while to potentially trigger callbacks
    for (int i = 0; i < 10000; i++) {
        ESP_ERROR_CHECK(touch_slider_sensor_handle_events(s_touch_slider));
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_delete(s_touch_slider));
    s_touch_slider = NULL;
}

TEST_CASE("touch slider sensor event handling test", "[touch_slider][events]")
{
    touch_slider_config_t config = {
        .channel_num = pad_num,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .filter_reset_times = 5, // Reduced reset times
        .position_range = 10000,
        .swipe_alpha = 0.9,
        .swipe_threshold = 50,
        .swipe_hysterisis = 40,
        .channel_gold_value = NULL,
        .debounce_times = 0,
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_create(&config, &s_touch_slider, touch_slider_event_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_slider);

    // Test handle_events API
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_slider_sensor_handle_events(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_handle_events(s_touch_slider));

    // Let it run for a while to potentially trigger callbacks
    for (int i = 0; i < 10000; i++) {
        ESP_ERROR_CHECK(touch_slider_sensor_handle_events(s_touch_slider));
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_slider_sensor_delete(s_touch_slider));
    s_touch_slider = NULL;
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %zu bytes free, After %zu bytes free (delta %zd)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("  _______               _        _____ _ _     _             _______        _   \n");
    printf(" |__   __|             | |      / ____| (_)   | |           |__   __|      | |  \n");
    printf("    | | ___  _   _  ___| |__   | (___ | |_  __| | ___ _ __     | | ___  ___| |_ \n");
    printf("    | |/ _ \\| | | |/ __| '_ \\   \\___ \\| | |/ _` |/ _ \\ '__|    | |/ _ \\/ __| __|\n");
    printf("    | | (_) | |_| | (__| | | |  ____) | | | (_| |  __/ |       | |  __/\\__ \\ |_ \n");
    printf("    |_|\\___/ \\__,_|\\___|_| |_| |_____/|_|_|\\__,_|\\___|_|       |_|\\___||___/\\__|\n");
    printf("                                                                                \n");
    printf("                                                                                \n");
    unity_run_menu();
}
