/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "imu_quaternion.h"
#include "test_data.h"

static const char *TAG = "MAIN";

#define EXAMPLE_LEAK_THRESHOLD  (-256)
#define EXAMPLE_LEAK_LOOPS      32
#define EXAMPLE_LOG_INTERVAL_MS 1000U

static float quat_norm4(float qw, float qx, float qy, float qz)
{
    return sqrtf(qw * qw + qx * qx + qy * qy + qz * qz);
}

static imu_quat_config_t make_quat_config(void)
{
    imu_quat_config_t config = {
        .accel_target_axis = IMU_QUAT_AXIS_POS_Z,
        .heading_ref_body = {-1.0f, 0.0f, 0.0f},
        .heading_target_axis = IMU_QUAT_AXIS_NEG_X,
        .mag_input_enabled = false,
        .gyro_bias_enabled = true,
        .gyro_guard_enabled = false,
        .gyro_guard_limit_dps = 150.0f,
    };

    return config;
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

static imu_quat_sample_t make_sample(size_t index)
{
    imu_quat_sample_t sample = {
        .accel = {
            imu_quaternion_replay_test_data[index][0],
            imu_quaternion_replay_test_data[index][1],
            imu_quaternion_replay_test_data[index][2],
        },
        .gyro = {
            imu_quaternion_replay_test_data[index][3],
            imu_quaternion_replay_test_data[index][4],
            imu_quaternion_replay_test_data[index][5],
        },
        .timestamp_us = (int64_t)(index + 1U) * IMU_QUAT_REPLAY_SAMPLE_PERIOD_US,
    };

    return sample;
}

static void log_pose(size_t sample_index, const imu_quat_output_t *out)
{
    const float norm = quat_norm4(out->q_w, out->q_x, out->q_y, out->q_z);
    const uint32_t time_ms =
        (uint32_t)(((sample_index + 1U) * IMU_QUAT_REPLAY_SAMPLE_PERIOD_US) / 1000U);

    ESP_LOGI(TAG,
             "sample=%u time_ms=%u quat=(%.6f, %.6f, %.6f, %.6f)",
             (unsigned int)sample_index,
             (unsigned int)time_ms,
             out->q_w,
             out->q_x,
             out->q_y,
             out->q_z);
}

static void run_memory_leak_check(void)
{
    const size_t before_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const size_t before_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    const imu_quat_config_t config = make_quat_config();

    for (int i = 0; i < EXAMPLE_LEAK_LOOPS; ++i) {
        imu_quat_handle_t handle = NULL;
        imu_quat_output_t out = { 0 };
        imu_quat_sample_t first = make_sample(0);
        imu_quat_sample_t second = make_sample(1);

        first.timestamp_us += i;
        second.timestamp_us += i;

        ESP_ERROR_CHECK(imu_quat_create(&config, &handle));
        ESP_ERROR_CHECK(imu_quat_reinitialize_from_sample(handle, &first));
        ESP_ERROR_CHECK(imu_quat_update(handle, &second, &out));
        ESP_ERROR_CHECK(imu_quat_reset_state(handle));
        ESP_ERROR_CHECK(imu_quat_delete(handle));
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
    imu_quat_handle_t handle = NULL;
    imu_quat_output_t out = { 0 };
    imu_quat_sample_t first = make_sample(0);
    const imu_quat_config_t config = make_quat_config();
    const size_t log_interval_samples =
        (EXAMPLE_LOG_INTERVAL_MS * 1000U) / IMU_QUAT_REPLAY_SAMPLE_PERIOD_US;
    float norm = 0.0f;

    ESP_ERROR_CHECK(imu_quat_create(&config, &handle));
    ESP_ERROR_CHECK(imu_quat_reinitialize_from_sample(handle, &first));

    for (size_t i = 1; i < imu_quaternion_replay_test_data_count; ++i) {
        imu_quat_sample_t sample = make_sample(i);

        ESP_ERROR_CHECK(imu_quat_update(handle, &sample, &out));
        if (log_interval_samples > 0 && (i % log_interval_samples) == 0) {
            log_pose(i, &out);
        }
    }

    norm = quat_norm4(out.q_w, out.q_x, out.q_y, out.q_z);
    if (!isfinite(norm) || fabsf(norm - 1.0f) > 0.10f) {
        ESP_LOGE(TAG, "unexpected replay quaternion norm: %.6f", norm);
        abort();
    }

    ESP_LOGI(TAG, "replayed_samples=%u period_us=%u",
             (unsigned int)imu_quaternion_replay_test_data_count,
             (unsigned int)IMU_QUAT_REPLAY_SAMPLE_PERIOD_US);
    log_pose(imu_quaternion_replay_test_data_count - 1U, &out);
    ESP_ERROR_CHECK(imu_quat_delete(handle));
}

void app_main(void)
{
    run_memory_leak_check();
    run_demo();
    ESP_LOGI(TAG, "Replay quaternion example finished");
}
