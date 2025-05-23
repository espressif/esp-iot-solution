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
#include "touch_button_sensor.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-300)
#define TEST_TOUCH_CHANNEL_1      9
#define TEST_TOUCH_CHANNEL_2      8

static touch_button_handle_t s_touch_button = NULL;
static size_t before_free_8bit;
static size_t before_free_32bit;

static uint64_t start_time = 0;

static uint64_t get_time_in_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t current_time = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (start_time == 0) {
        start_time = current_time;
    }
    return current_time - start_time;
}

// print touch values(raw, smooth, benchmark) of each enabled channel, every 50ms
#define PRINT_VALUE(pad_num, raw, smooth, benchmark) printf("vl,%"PRId64",%"PRIu32",%"PRIu32",%"PRIu32",%"PRIu32"\n", get_time_in_ms(), pad_num, raw, smooth, benchmark)
// print trigger if touch active/inactive happens
#define PRINT_TRIGGER(pad_num, smooth, if_active) printf("tg,%"PRId64",%"PRIu32",%"PRIu32",%d,%d,%d,%d,%d\n", get_time_in_ms(), pad_num, smooth, if_active?1:0, if_active?0:1,0,0,0)
/* run command `pip3 install tptool` to install monitor app, python3 -m tptool to start */
#define PRINT_TRIGGER_REAL(pad_num, smooth, if_active) printf("bt,%"PRId64",%"PRIu32",%"PRIu32",%d,%d,%d,%d,%d\n", get_time_in_ms(), pad_num, smooth, if_active?1:0, if_active?0:1,0,0,0)

static void touch_button_callback(touch_button_handle_t handle, uint32_t channel, touch_state_t state, void *user_data)
{
    uint32_t bitmap = 0;
    touch_button_sensor_get_state_bitmap(handle, channel, &bitmap);

    uint32_t data[SOC_TOUCH_SAMPLE_CFG_NUM] = {0};
    if (state == TOUCH_STATE_ACTIVE) {
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            touch_button_sensor_get_data(handle, channel, i, &data[i]);
        }
        PRINT_TRIGGER(channel, data[0], true);
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 1) {
            PRINT_TRIGGER(channel * 100 + 1, data[1], true);
        }
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 2) {
            PRINT_TRIGGER(channel * 100 + 2, data[2], true);
        }
        printf("Channel %"PRIu32" is Active", channel);
        // if log level is set to debug, print the bitmap
        printf(", state bitmap: 0x%08" PRIx32, bitmap);
        printf("\n");
    } else if (state == TOUCH_STATE_INACTIVE) {
        for (int i = 0; i < SOC_TOUCH_SAMPLE_CFG_NUM; i++) {
            touch_button_sensor_get_data(handle, channel, i, &data[i]);
        }
        PRINT_TRIGGER(channel, data[0], false);
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 1) {
            PRINT_TRIGGER(channel * 100 + 1, data[1], false);
        }
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 2) {
            PRINT_TRIGGER(channel * 100 + 2, data[2], false);
        }
        printf("Channel %"PRIu32" is Inactive", channel);
        printf(", state bitmap:0x%08" PRIx32, bitmap);
        printf("\n");
    }
}

TEST_CASE("touch button sensor create/delete test", "[touch_button_sensor][create]")
{
    uint32_t channel_list[] = {TEST_TOUCH_CHANNEL_1};
    float threshold[] = {0.007f};
    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .channel_gold_value = NULL,
        .debounce_times = 3,
        .skip_lowlevel_init = false
    };

    // Test successful creation
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_create(&config, &s_touch_button, touch_button_callback, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_button);

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_delete(s_touch_button));
    s_touch_button = NULL;
}

TEST_CASE("touch button sensor data and state test", "[touch_button_sensor][data]")
{
    uint32_t channel_list[] = {TEST_TOUCH_CHANNEL_1};
    float threshold[] = {0.007f};
    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .channel_gold_value = NULL,
        .debounce_times = 3,
        .skip_lowlevel_init = false
    };

    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_create(&config, &s_touch_button, touch_button_callback, NULL));

    // Test get_data API
    uint32_t data = 0;
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_get_data(s_touch_button, TEST_TOUCH_CHANNEL_1, 0, &data));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, touch_button_sensor_get_data(s_touch_button, TEST_TOUCH_CHANNEL_2, 0, &data));

    // Test get_state API
    touch_state_t state;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_button_sensor_get_state(NULL, TEST_TOUCH_CHANNEL_1, &state));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_button_sensor_get_state(s_touch_button, TEST_TOUCH_CHANNEL_1, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_get_state(s_touch_button, TEST_TOUCH_CHANNEL_1, &state));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, touch_button_sensor_get_state(s_touch_button, TEST_TOUCH_CHANNEL_2, &state));

    // Test get_state_bitmap API
    uint32_t bitmap;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_button_sensor_get_state_bitmap(NULL, TEST_TOUCH_CHANNEL_1, &bitmap));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_button_sensor_get_state_bitmap(s_touch_button, TEST_TOUCH_CHANNEL_1, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_get_state_bitmap(s_touch_button, TEST_TOUCH_CHANNEL_1, &bitmap));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, touch_button_sensor_get_state_bitmap(s_touch_button, TEST_TOUCH_CHANNEL_2, &bitmap));

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_delete(s_touch_button));
    s_touch_button = NULL;
}

TEST_CASE("touch button sensor event handling test", "[touch_button_sensor][events]")
{
    uint32_t channel_list[] = {TEST_TOUCH_CHANNEL_1};
    float threshold[] = {0.007f};
    touch_button_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = threshold,
        .channel_gold_value = NULL,
        .debounce_times = 3,
        .skip_lowlevel_init = false
    };

    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_create(&config, &s_touch_button, touch_button_callback, NULL));

    // Test handle_events API
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, touch_button_sensor_handle_events(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_handle_events(s_touch_button));

    // Let it run for a while to potentially trigger callbacks
    for (int i = 0; i < 100; i++) {
        uint32_t data;
        touch_button_sensor_handle_events(s_touch_button);
        touch_button_sensor_get_data(s_touch_button, TEST_TOUCH_CHANNEL_1, 0, &data);
        PRINT_VALUE((uint32_t)TEST_TOUCH_CHANNEL_1, data, data, data);
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 1) {
            touch_button_sensor_get_data(s_touch_button, TEST_TOUCH_CHANNEL_1, 1, &data);
            PRINT_VALUE((uint32_t)TEST_TOUCH_CHANNEL_1 * 100 + 1, data, data, data);
        }
        if (SOC_TOUCH_SAMPLE_CFG_NUM > 2) {
            touch_button_sensor_get_data(s_touch_button, TEST_TOUCH_CHANNEL_1, 2, &data);
            PRINT_VALUE((uint32_t)TEST_TOUCH_CHANNEL_1 * 100 + 2, data, data, data);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // Clean up
    TEST_ASSERT_EQUAL(ESP_OK, touch_button_sensor_delete(s_touch_button));
    s_touch_button = NULL;
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
    printf("  _____                _       ____        _   _              _____          _   \n");
    printf(" |_   _|__  _   _  ___| |__   | __ ) _   _| |_| |_ ___  _ __|_   _|__  ___| |_ \n");
    printf("   | |/ _ \\| | | |/ __| '_ \\  |  _ \\| | | | __| __/ _ \\| '_ \\ | |/ _ \\/ __| __|\n");
    printf("   | | (_) | |_| | (__| | | | | |_) | |_| | |_| || (_) | | | || |  __/\\__ \\ |_ \n");
    printf("   |_|\\___/ \\__,_|\\___|_| |_| |____/ \\__,_|\\__|\\__\\___/|_| |_||_|\\___||___/\\__|\n");
    printf("\n");
    unity_run_menu();
}
