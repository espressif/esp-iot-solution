/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "touch_sensor_lowlevel.h"
#include "touch_proximity_sensor.h"

const static char *TAG = "touch-proximity-sensor-test";

#define TEST_MEMORY_LEAK_THRESHOLD (-200)

static touch_proximity_handle_t s_touch_proximity_sensor = NULL;

static size_t before_free_8bit;
static size_t before_free_32bit;

static void example_proxi_callback(uint32_t channel, proxi_state_t event, void *cb_arg)
{
    switch (event) {
    case PROXI_STATE_ACTIVE:
        ESP_LOGI(TAG, "CH%"PRIu32", active!", channel);
        break;
    case PROXI_STATE_INACTIVE:
        ESP_LOGI(TAG, "CH%"PRIu32", inactive!", channel);
        break;
    default:
        break;
    }
}

TEST_CASE("touch proximity sensor loop get test", "[touch_proximity_sensor][loop]")
{
    uint32_t channel_list[] = {TOUCH_PAD_NUM8};
    float channel_threshold[] = {0.004f};
    touch_proxi_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = channel_threshold,
        .debounce_times = 2,
        .skip_lowlevel_init = false,
    };

    // Test create
    TEST_ESP_OK(touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, NULL, NULL));
    TEST_ASSERT_NOT_NULL(s_touch_proximity_sensor);

    int counter = 0;
    while (counter++ < 100) {
        uint32_t data;
        proxi_state_t state;
        TEST_ESP_OK(touch_proximity_sensor_handle_events(s_touch_proximity_sensor));
        TEST_ESP_OK(touch_proximity_sensor_get_state(s_touch_proximity_sensor, TOUCH_PAD_NUM8, &state));
        TEST_ESP_OK(touch_proximity_sensor_get_data(s_touch_proximity_sensor, TOUCH_PAD_NUM8, &data));
        printf("CH%d, state: %d, data: %"PRIu32"\n", TOUCH_PAD_NUM8, state, data);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    // Test delete
    TEST_ESP_OK(touch_proximity_sensor_delete(s_touch_proximity_sensor));
    s_touch_proximity_sensor = NULL;
}

TEST_CASE("touch proximity sensor callback test", "[touch_proximity_sensor][callback]")
{
    uint32_t channel_list[] = {TOUCH_PAD_NUM8};
    float channel_threshold[] = {0.004f};
    touch_proxi_config_t config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = channel_threshold,
        .debounce_times = 2,
        .skip_lowlevel_init = false,
    };

    TEST_ESP_OK(touch_proximity_sensor_create(&config, &s_touch_proximity_sensor, &example_proxi_callback, NULL));

    ESP_LOGI(TAG, "touch proximity sensor started - approach the touch pad to test proximity detection");
    for (int i = 0; i < 100; i++) {
        touch_proximity_sensor_handle_events(s_touch_proximity_sensor);
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    TEST_ESP_OK(touch_proximity_sensor_delete(s_touch_proximity_sensor));
    s_touch_proximity_sensor = NULL;
}

TEST_CASE("touch proximity sensor skip lowlevel init", "[touch_proximity_sensor][loop]")
{
    // First initialize the low level touch sensor
    uint32_t channel_list[] = {TOUCH_PAD_NUM8};
    touch_lowlevel_type_t channel_type[] = {TOUCH_LOWLEVEL_TYPE_PROXIMITY};
    touch_lowlevel_config_t low_config = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_type = channel_type,
        .proximity_count = CONFIG_TOUCH_PROXIMITY_MEAS_COUNT,
    };
    TEST_ESP_OK(touch_sensor_lowlevel_create(&low_config));

    // Now create two proximity sensors that use the same touch hardware
    touch_proximity_handle_t sensor1 = NULL;
    touch_proximity_handle_t sensor2 = NULL;

    float threshold1[] = {0.008f};
    float threshold2[] = {0.01f}; // Different threshold for testing

    // Configure first sensor
    touch_proxi_config_t config1 = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = threshold1,
        .debounce_times = 2,
        .skip_lowlevel_init = true, // Skip since we already initialized
    };

    // Configure second sensor
    touch_proxi_config_t config2 = {
        .channel_num = 1,
        .channel_list = channel_list,
        .channel_threshold = threshold2,
        .debounce_times = 3, // Different debounce for testing
        .skip_lowlevel_init = true,
    };

    // Create both sensors
    TEST_ESP_OK(touch_proximity_sensor_create(&config1, &sensor1, NULL, NULL));
    TEST_ESP_OK(touch_proximity_sensor_create(&config2, &sensor2, NULL, NULL));
    TEST_ASSERT_NOT_NULL(sensor1);
    TEST_ASSERT_NOT_NULL(sensor2);
    TEST_ESP_OK(touch_sensor_lowlevel_start());

    // Test both sensors for a while
    for (int i = 0; i < 100; i++) {
        uint32_t data1, data2;
        proxi_state_t state1, state2;

        // Handle events and get data from both sensors
        TEST_ESP_OK(touch_proximity_sensor_handle_events(sensor1));
        TEST_ESP_OK(touch_proximity_sensor_handle_events(sensor2));

        // Get data from both sensors
        TEST_ESP_OK(touch_proximity_sensor_get_data(sensor1, TOUCH_PAD_NUM8, &data1));
        TEST_ESP_OK(touch_proximity_sensor_get_data(sensor2, TOUCH_PAD_NUM8, &data2));

        // Get states from both sensors
        TEST_ESP_OK(touch_proximity_sensor_get_state(sensor1, TOUCH_PAD_NUM8, &state1));
        TEST_ESP_OK(touch_proximity_sensor_get_state(sensor2, TOUCH_PAD_NUM8, &state2));

        // Both sensors should get similar raw data since they use the same hardware
        TEST_ASSERT_UINT32_WITHIN(100, data1, data2);

        // States might differ due to different thresholds and debounce settings
        printf("Sensor1: data=%"PRIu32", state=%d; Sensor2: data=%"PRIu32", state=%d\n",
               data1, state1, data2, state2);

        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    // Clean up in reverse order
    TEST_ESP_OK(touch_proximity_sensor_delete(sensor2));
    TEST_ESP_OK(touch_proximity_sensor_delete(sensor1));

    // Finally clean up the low level touch sensor
    TEST_ESP_OK(touch_sensor_lowlevel_stop());
    TEST_ESP_OK(touch_sensor_lowlevel_delete());
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
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
    /*
    *   _____                _       ____                _           _ _           _____
    *  |_   _|__  _   _  ___| |__   |  _ \ _ __ _____  _(_)_ __ ___ (_) |_ _   _  |_   _|__  ___| |_
    *    | |/ _ \| | | |/ __| '_ \  | |_) | '__/ _ \ \/ / | '_ ` _ \| | __| | | |   | |/ _ \/ __| __|
    *    | | (_) | |_| | (__| | | | |  __/| | | (_) >  <| | | | | | | | |_| |_| |   | |  __/\__ \ |_
    *    |_|\___/ \__,_|\___|_| |_| |_|   |_|  \___/_/\_\_|_| |_| |_|_|\__|\__, |   |_|\___||___/\__|
    *                                                                      |___/
    */
    printf("  _____                _       ____                _           _ _           _____\n");
    printf(" |_   _|__  _   _  ___| |__   |  _ \\ _ __ _____  _(_)_ __ ___ (_) |_ _   _  |_   _|__  ___| |_\n");
    printf("   | |/ _ \\| | | |/ __| '_ \\  | |_) | '__/ _ \\ \\/ / | '_ ` _ \\| | __| | | |   | |/ _ \\/ __| __|\n");
    printf("   | | (_) | |_| | (__| | | | |  __/| | | (_) >  <| | | | | | | | |_| |_| |   | |  __/\\__ \\ |_\n");
    printf("   |_|\\___/ \\__,_|\\___|_| |_| |_|   |_|  \\___/_/\\_\\_|_| |_| |_|_|\\__|\\__, |   |_|\\___||___/\\__|\n");
    printf("                                                                     |___/\n");
    unity_run_menu();
}
