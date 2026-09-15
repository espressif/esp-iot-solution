/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "imu_gesture.h"
#include "imu_gesture_space_switch.h"
#include "test_data.h"

static const char *TAG = "MAIN";

#define EXAMPLE_LEAK_THRESHOLD  (-512)
#define EXAMPLE_LEAK_LOOPS      64

typedef struct {
    int total_events;
    bool segment_hit[5];
    int active_label;
} space_state_t;

static imu_gesture_space_switch_config_t make_space_switch_config(void)
{
    imu_gesture_space_switch_config_t config =
        (imu_gesture_space_switch_config_t)IMU_GESTURE_SPACE_SWITCH_DEFAULT_CONFIG();

    config.horizontal_axis = IMU_GESTURE_AXIS_Z;
    config.left_sign = 1;
    config.vertical_axis = IMU_GESTURE_AXIS_Y;
    config.up_sign = 1;
    config.trigger_dps = 450.0f;
    config.release_dps = 30.0f;
    config.axis_ratio_limit = 0.50f;
    config.cooldown_ms = 1000U;
    config.sample_queue_len = 16U;
    return config;
}

static const char *space_event_name(imu_gesture_event_t event)
{
    switch (event) {
    case IMU_GESTURE_EVENT_SPACE_LEFT:
        return "SPACE_LEFT";
    case IMU_GESTURE_EVENT_SPACE_RIGHT:
        return "SPACE_RIGHT";
    case IMU_GESTURE_EVENT_SPACE_UP:
        return "SPACE_UP";
    case IMU_GESTURE_EVENT_SPACE_DOWN:
        return "SPACE_DOWN";
    default:
        return "UNKNOWN";
    }
}

static void space_event_cb(imu_gesture_detector_handle_t detector,
                           imu_gesture_event_t event,
                           void *user_data)
{
    space_state_t *state = (space_state_t *)user_data;

    (void)detector;
    if (state != NULL) {
        state->total_events++;
        if (state->active_label >= 1 && state->active_label <= 4) {
            if ((state->active_label == 1 && event == IMU_GESTURE_EVENT_SPACE_UP) ||
                    (state->active_label == 2 && event == IMU_GESTURE_EVENT_SPACE_DOWN) ||
                    (state->active_label == 3 && event == IMU_GESTURE_EVENT_SPACE_LEFT) ||
                    (state->active_label == 4 && event == IMU_GESTURE_EVENT_SPACE_RIGHT)) {
                state->segment_hit[state->active_label] = true;
            }
        }
    }
    ESP_LOGI(TAG, "%s", space_event_name(event));
}

static void assert_no_significant_leak(size_t before_free,
                                       size_t after_free,
                                       const char *label)
{
    int64_t delta = (int64_t)after_free - (int64_t)before_free;

    if (delta < EXAMPLE_LEAK_THRESHOLD) {
        ESP_LOGE(TAG, "%s leak detected: before=%u after=%u delta=%lld",
                 label,
                 (unsigned int)before_free,
                 (unsigned int)after_free,
                 (long long)delta);
        abort();
    }
}

static void run_memory_leak_check(void)
{
    const size_t before_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t before_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    const imu_gesture_space_switch_config_t config = make_space_switch_config();

    for (int i = 0; i < EXAMPLE_LEAK_LOOPS; ++i) {
        imu_gesture_detector_handle_t detector = NULL;

        ESP_ERROR_CHECK(imu_gesture_space_switch_detector_create(&config, &detector));
        ESP_ERROR_CHECK(imu_gesture_detector_reset(detector));
        ESP_ERROR_CHECK(imu_gesture_detector_del(detector));
    }

    assert_no_significant_leak(before_8bit,
                               heap_caps_get_free_size(MALLOC_CAP_8BIT),
                               "8bit");
    assert_no_significant_leak(before_32bit,
                               heap_caps_get_free_size(MALLOC_CAP_32BIT),
                               "32bit");
}

static void run_demo(void)
{
    imu_gesture_detector_handle_t detector = NULL;
    space_state_t state = { 0 };
    size_t processed = 0;
    int segment_seen[5] = { 0 };
    const imu_gesture_space_switch_config_t config = make_space_switch_config();

    ESP_ERROR_CHECK(imu_gesture_space_switch_detector_create(&config, &detector));
    ESP_ERROR_CHECK(imu_gesture_detector_register_cb(detector, space_event_cb, &state));

    for (size_t i = 0; i < imu_gesture_space_switch_test_data_count; ++i) {
        imu_gesture_sample_t sample = {
            .accel = {
                imu_gesture_space_switch_test_data[i].accel[0],
                imu_gesture_space_switch_test_data[i].accel[1],
                imu_gesture_space_switch_test_data[i].accel[2],
            },
            .gyro = {
                imu_gesture_space_switch_test_data[i].gyro[0],
                imu_gesture_space_switch_test_data[i].gyro[1],
                imu_gesture_space_switch_test_data[i].gyro[2],
            },
            .timestamp_us = (int64_t)(i + 1U) *
            IMU_GESTURE_SPACE_SWITCH_TEST_SAMPLE_PERIOD_US,
        };

        state.active_label = imu_gesture_space_switch_test_data[i].label;
        if (state.active_label >= 1 && state.active_label <= 4) {
            segment_seen[state.active_label] = 1;
        }

        ESP_ERROR_CHECK(imu_gesture_detector_push_sample(detector, &sample));
        ESP_ERROR_CHECK(imu_gesture_detector_process_pending(detector, &processed));
    }

    ESP_LOGI(TAG, "total_events=%d", state.total_events);
    ESP_LOGI(TAG,
             "segments up=%s down=%s left=%s right=%s",
             state.segment_hit[1] ? "hit" : "miss",
             state.segment_hit[2] ? "hit" : "miss",
             state.segment_hit[3] ? "hit" : "miss",
             state.segment_hit[4] ? "hit" : "miss");
    ESP_ERROR_CHECK(imu_gesture_detector_del(detector));
}

void app_main(void)
{
    run_memory_leak_check();
    run_demo();
    ESP_LOGI(TAG, "Space-switch example finished");
}
