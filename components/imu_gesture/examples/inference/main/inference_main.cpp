/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stddef.h>
#include <stdlib.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "gesture_model.h"
#include "imu_gesture.h"
#include "imu_gesture_inference.h"
#include "test_data.h"

static const char *TAG = "MAIN";

#define EXAMPLE_LEAK_THRESHOLD  (-512)
#define EXAMPLE_LEAK_LOOPS      32

static inline size_t input_index(size_t sample_index,
                                 size_t input_channels,
                                 size_t axis_index)
{
    return sample_index * input_channels + axis_index;
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

static void detector_event_cb(imu_gesture_detector_handle_t detector,
                              imu_gesture_event_t event,
                              void *user_data)
{
    const char *stage = static_cast<const char *>(user_data);

    if (event == IMU_GESTURE_EVENT_INFERENCE_RESULT) {
        imu_gesture_inference_result_t result = {};
        const char *label;

        if (imu_gesture_inference_detector_get_last_result(detector, &result) != ESP_OK) {
            return;
        }

        label = gesture_inference_micro_simple_norm_gesture_model_get_label(result.label_index);
        ESP_LOGI(TAG, "[%s] label=%s(%u) score=%.4f",
                 stage,
                 label != nullptr ? label : "unknown",
                 (unsigned int)result.label_index,
                 result.score);
        return;
    }

    ESP_LOGW(TAG, "[%s] model failed", stage);
}

static esp_err_t run_realtime_demo(imu_gesture_detector_handle_t detector,
                                   size_t sample_length,
                                   size_t input_channels)
{
    for (size_t i = 0; i < sample_length; ++i) {
        imu_gesture_sample_t sample = {
            .accel = {0.0f, 0.0f, 0.0f},
            .gyro = {
                gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 0)],
                gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 1)],
                gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 2)],
            },
            .timestamp_us = (int64_t)i * 10000,
        };

        ESP_RETURN_ON_ERROR(imu_gesture_detector_push_sample(detector, &sample),
                            TAG,
                            "push sample failed");
        ESP_RETURN_ON_ERROR(imu_gesture_detector_process_pending(detector, NULL),
                            TAG,
                            "process sample failed");
    }

    return ESP_OK;
}

static esp_err_t run_single_shot_demo(imu_gesture_detector_handle_t detector,
                                      size_t sample_length,
                                      size_t input_channels)
{
    imu_gesture_sample_t *buffer = static_cast<imu_gesture_sample_t *>(
                                       calloc(sample_length, sizeof(imu_gesture_sample_t)));
    if (buffer == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < sample_length; ++i) {
        buffer[i].gyro[0] =
            gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 0)];
        buffer[i].gyro[1] =
            gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 1)];
        buffer[i].gyro[2] =
            gesture_inference_micro_simple_norm_test_input[input_index(i, input_channels, 2)];
        buffer[i].timestamp_us = (int64_t)i * 10000;
    }

    ESP_RETURN_ON_ERROR(
        imu_gesture_inference_detector_process_single_shot_buffer(detector, buffer, sample_length),
        TAG,
        "single shot failed");
    free(buffer);
    return ESP_OK;
}

static void run_memory_leak_check(const imu_gesture_inference_config_t *config)
{
    const size_t before_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t before_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);

    for (int i = 0; i < EXAMPLE_LEAK_LOOPS; ++i) {
        imu_gesture_detector_handle_t detector = nullptr;

        ESP_ERROR_CHECK(imu_gesture_inference_detector_create(config, &detector));
        ESP_ERROR_CHECK(imu_gesture_detector_register_cb(detector, detector_event_cb, (void *)"leak"));
        ESP_ERROR_CHECK(imu_gesture_detector_reset(detector));
        ESP_ERROR_CHECK(imu_gesture_detector_unregister_cb(detector));
        ESP_ERROR_CHECK(imu_gesture_detector_del(detector));
    }

    assert_no_significant_leak(before_8bit,
                               heap_caps_get_free_size(MALLOC_CAP_8BIT),
                               "8bit");
    assert_no_significant_leak(before_32bit,
                               heap_caps_get_free_size(MALLOC_CAP_32BIT),
                               "32bit");
}

extern "C" void app_main(void)
{
    const imu_gesture_inference_model_t *model =
        gesture_inference_micro_simple_norm_gesture_model_get_desc();
    const size_t input_element_count =
        sizeof(gesture_inference_micro_simple_norm_test_input) /
        sizeof(gesture_inference_micro_simple_norm_test_input[0]);
    imu_gesture_detector_handle_t detector = nullptr;
    const imu_gesture_inference_config_t config = {
        .model = model,
        .window_step = model->input_length,
        .sample_queue_len = 8,
    };

    if (model == nullptr) {
        ESP_LOGE(TAG, "model descriptor is null");
        return;
    }

    if ((model->input_length * model->input_channels) != input_element_count) {
        ESP_LOGE(TAG, "test input shape mismatch");
        return;
    }

    run_memory_leak_check(&config);

    ESP_ERROR_CHECK(imu_gesture_inference_detector_create(&config, &detector));
    ESP_ERROR_CHECK(imu_gesture_detector_register_cb(detector, detector_event_cb, (void *)"realtime"));
    ESP_ERROR_CHECK(run_realtime_demo(detector, model->input_length, model->input_channels));
    ESP_ERROR_CHECK(imu_gesture_detector_reset(detector));
    ESP_ERROR_CHECK(imu_gesture_detector_unregister_cb(detector));
    ESP_ERROR_CHECK(imu_gesture_detector_register_cb(detector, detector_event_cb, (void *)"single-shot"));
    ESP_ERROR_CHECK(run_single_shot_demo(detector, model->input_length, model->input_channels));
    ESP_ERROR_CHECK(imu_gesture_detector_del(detector));

    ESP_LOGI(TAG, "Inference example finished");
}
