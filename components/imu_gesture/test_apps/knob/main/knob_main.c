/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "imu_gesture.h"
#include "imu_gesture_knob.h"
#include "test_data.h"

static const char *TAG = "MAIN";

#define EXAMPLE_LEAK_THRESHOLD  (-512)
#define EXAMPLE_LEAK_LOOPS      64

typedef struct {
    int total_events;
    int active_segment;
    bool segment_hit[8];
} knob_state_t;

static const char *knob_event_name(imu_gesture_event_t event)
{
    switch (event) {
    case IMU_GESTURE_EVENT_KNOB_CW:
        return "KNOB_CW";
    case IMU_GESTURE_EVENT_KNOB_CCW:
        return "KNOB_CCW";
    default:
        return "UNKNOWN";
    }
}

static void knob_event_cb(imu_gesture_detector_handle_t detector,
                          imu_gesture_event_t event,
                          void *user_data)
{
    knob_state_t *state = (knob_state_t *)user_data;

    (void)detector;
    if (state != NULL) {
        state->total_events++;
        if (state->active_segment > 0 &&
                state->active_segment < (int)(sizeof(state->segment_hit) / sizeof(state->segment_hit[0]))) {
            state->segment_hit[state->active_segment] = true;
        }
    }
    ESP_LOGI(TAG, "%s", knob_event_name(event));
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

    for (int i = 0; i < EXAMPLE_LEAK_LOOPS; ++i) {
        imu_gesture_detector_handle_t detector = NULL;

        ESP_ERROR_CHECK(imu_gesture_knob_detector_create(NULL, &detector));
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
    knob_state_t state = { 0 };
    size_t processed = 0;
    int segment_count = 0;
    int previous_label = 0;

    ESP_ERROR_CHECK(imu_gesture_knob_detector_create(NULL, &detector));
    ESP_ERROR_CHECK(imu_gesture_detector_register_cb(detector, knob_event_cb, &state));

    for (size_t i = 0; i < imu_gesture_knob_test_data_count; ++i) {
        const int label = imu_gesture_knob_test_data[i].label;
        imu_gesture_sample_t sample = {
            .accel = {
                imu_gesture_knob_test_data[i].accel[0],
                imu_gesture_knob_test_data[i].accel[1],
                imu_gesture_knob_test_data[i].accel[2],
            },
            .gyro = {
                imu_gesture_knob_test_data[i].gyro[0],
                imu_gesture_knob_test_data[i].gyro[1],
                imu_gesture_knob_test_data[i].gyro[2],
            },
            .timestamp_us = (int64_t)(i + 1U) *
            IMU_GESTURE_KNOB_TEST_SAMPLE_PERIOD_US,
        };

        if ((label == 1 || label == 2) && previous_label != label) {
            segment_count++;
        }

        state.active_segment = (label == 1 || label == 2) ? segment_count : 0;
        previous_label = label;

        ESP_ERROR_CHECK(imu_gesture_detector_push_sample(detector, &sample));
        ESP_ERROR_CHECK(imu_gesture_detector_process_pending(detector, &processed));
    }

    for (int i = 1; i <= segment_count; ++i) {
        if (!state.segment_hit[i]) {
            ESP_LOGE(TAG, "knob segment %d had no event", i);
            abort();
        }
    }

    ESP_LOGI(TAG, "total_events=%d segments=%d", state.total_events, segment_count);
    ESP_ERROR_CHECK(imu_gesture_detector_del(detector));
}

void app_main(void)
{
    run_memory_leak_check();
    run_demo();
    ESP_LOGI(TAG, "Knob example finished");
}
