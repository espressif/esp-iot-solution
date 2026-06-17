/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "unity.h"
#include "imu_quaternion.h"
#include "imu_quaternion_init.h"
#include "imu_quaternion_priv.h"

static float quat_norm4(float qw, float qx, float qy, float qz)
{
    return sqrtf(qw * qw + qx * qx + qy * qy + qz * qz);
}

static void assert_vec3_close(const float actual[3], const float expected[3], float tol)
{
    TEST_ASSERT_FLOAT_WITHIN(tol, expected[0], actual[0]);
    TEST_ASSERT_FLOAT_WITHIN(tol, expected[1], actual[1]);
    TEST_ASSERT_FLOAT_WITHIN(tol, expected[2], actual[2]);
}

void test_imu_quaternion_create_delete_uses_layered_defaults(void)
{
    imu_quat_handle_t handle = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(NULL, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(IMU_QUAT_INIT_CURRENT_POSE, handle->config.init_strategy);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, handle->config.heading_ref_body[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, handle->config.heading_ref_body[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, handle->config.heading_ref_body[2]);
    TEST_ASSERT_EQUAL(IMU_QUAT_AXIS_POS_Z, handle->config.accel_target_axis);
    TEST_ASSERT_EQUAL(IMU_QUAT_AXIS_NEG_X, handle->config.heading_target_axis);
    TEST_ASSERT_TRUE(handle->config.gyro_bias_enabled);
    TEST_ASSERT_TRUE(handle->config.gyro_guard_enabled);
    TEST_ASSERT_TRUE(handle->runtime_gain.warming_up);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, IMU_QUAT_INTERNAL_FEEDBACK_GAIN, handle->runtime_gain.steady_gain);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, IMU_QUAT_INTERNAL_WARMUP_GAIN, handle->runtime_gain.live_gain);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6f,
        (IMU_QUAT_INTERNAL_WARMUP_GAIN - IMU_QUAT_INTERNAL_FEEDBACK_GAIN) /
        IMU_QUAT_INTERNAL_WARMUP_PERIOD_SEC,
        handle->runtime_gain.gain_drop_per_sec);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_default_heading_ref_body_is_negative_x(void)
{
    imu_quat_config_t config = {};

    imu_quat_set_default_config(&config);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, config.heading_ref_body[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, config.heading_ref_body[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, config.heading_ref_body[2]);
}

void test_imu_quaternion_custom_heading_target_axis_is_applied(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT;
    config.heading_target_axis = IMU_QUAT_AXIS_POS_Y;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &sample, &out));

    float heading_world[3] = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_quat_rotate_body_to_world(handle, config.heading_ref_body, heading_world));
    {
        const float expected_heading[3] = {0.0f, 1.0f, 0.0f};
        assert_vec3_close(heading_world, expected_heading, 1e-4f);
    }

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_default_alignment_matches_airmouse_compatibility(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &sample, &out));

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, out.q_w);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, out.q_x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, out.q_y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, out.q_z);

    float heading_world[3] = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_quat_rotate_body_to_world(handle, config.heading_ref_body, heading_world));
    {
        const float expected_heading[3] = {-1.0f, 0.0f, 0.0f};
        assert_vec3_close(heading_world, expected_heading, 1e-4f);
    }

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_custom_accel_target_axis_is_applied(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT;
    config.accel_target_axis = IMU_QUAT_AXIS_NEG_Z;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &sample, &out));

    float accel_world[3] = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_EQUAL(
        ESP_OK,
        imu_quat_rotate_body_to_world(handle, sample.accel, accel_world));
    {
        const float expected_accel[3] = {0.0f, 0.0f, -1.0f};
        assert_vec3_close(accel_world, expected_accel, 1e-4f);
    }

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_parallel_target_axes_fail_initialization(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT;
    config.accel_target_axis = IMU_QUAT_AXIS_POS_Z;
    config.heading_target_axis = IMU_QUAT_AXIS_POS_Z;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, imu_quat_update(handle, &sample, &out));
    TEST_ASSERT_FALSE(out.updated);
    TEST_ASSERT_FALSE(out.reinitialized);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, handle->pose.last_timestamp_us);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_antiparallel_target_axes_fail_initialization(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.init_strategy = IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT;
    config.accel_target_axis = IMU_QUAT_AXIS_POS_Z;
    config.heading_target_axis = IMU_QUAT_AXIS_NEG_Z;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t sample = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, imu_quat_update(handle, &sample, &out));

    TEST_ASSERT_FALSE(out.updated);
    TEST_ASSERT_FALSE(out.reinitialized);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, handle->pose.last_timestamp_us);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_bias_disabled_leaves_bias_state_unchanged(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.gyro_bias_enabled = false;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t first = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.3f, -0.4f, 0.2f},
        .timestamp_us = 1000,
    };
    imu_quat_sample_t second = first;
    second.timestamp_us = 11000;

    imu_quat_output_t out = {};
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &first, &out));
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &second, &out));

    TEST_ASSERT_FALSE(handle->bias.rest_detected);
    TEST_ASSERT_FALSE(handle->bias.bias_lp_valid);
    TEST_ASSERT_FLOAT_WITHIN(1e-7f, 0.0f, handle->bias.rest_time_sec);
    TEST_ASSERT_FLOAT_WITHIN(1e-7f, 0.0f, handle->bias.gyro_bias[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-7f, 0.0f, handle->bias.gyro_bias[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-7f, 0.0f, handle->bias.gyro_bias[2]);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_gyro_guard_disabled_does_not_reinitialize(void)
{
    imu_quat_config_t config = {};
    imu_quat_set_default_config(&config);
    config.gyro_guard_enabled = false;

    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(&config, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    imu_quat_sample_t first = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {0.0f, 0.0f, 0.0f},
        .timestamp_us = 1000,
    };
    imu_quat_sample_t second = {
        .accel = {0.0f, 0.0f, 1.0f},
        .gyro = {500.0f, 0.0f, 0.0f},
        .timestamp_us = 11000,
    };
    imu_quat_output_t out = {};

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &first, &out));
    TEST_ASSERT_FALSE(out.reinitialized);
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &second, &out));
    TEST_ASSERT_TRUE(out.updated);
    TEST_ASSERT_FALSE(out.reinitialized);
    TEST_ASSERT_FALSE(handle->gyro_guard.gyro_reinit_armed);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_default_warmup_runtime_state_is_initialized(void)
{
    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(NULL, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    TEST_ASSERT_TRUE(handle->runtime_gain.warming_up);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, IMU_QUAT_INTERNAL_FEEDBACK_GAIN, handle->runtime_gain.steady_gain);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, IMU_QUAT_INTERNAL_WARMUP_GAIN, handle->runtime_gain.live_gain);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6f,
        (IMU_QUAT_INTERNAL_WARMUP_GAIN - IMU_QUAT_INTERNAL_FEEDBACK_GAIN) /
        IMU_QUAT_INTERNAL_WARMUP_PERIOD_SEC,
        handle->runtime_gain.gain_drop_per_sec);

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void test_imu_quaternion_remains_normalized_in_smoke_update_sequence(void)
{
    imu_quat_handle_t handle = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_create(NULL, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    const imu_quat_sample_t samples[] = {
        {.accel = {0.0f, 0.0f, 1.0f}, .gyro = {0.0f, 0.0f, 0.0f}, .timestamp_us = 1000},
        {.accel = {0.01f, 0.0f, 0.9999f}, .gyro = {2.0f, 1.0f, 0.0f}, .timestamp_us = 11000},
        {.accel = {0.02f, -0.01f, 0.9997f}, .gyro = {1.0f, -1.0f, 0.5f}, .timestamp_us = 21000},
        {.accel = {0.0f, 0.02f, 0.9998f}, .gyro = {0.5f, 0.0f, -0.5f}, .timestamp_us = 31000},
    };

    imu_quat_output_t out = {};
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        TEST_ASSERT_EQUAL(ESP_OK, imu_quat_update(handle, &samples[i], &out));
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, quat_norm4(out.q_w, out.q_x, out.q_y, out.q_z));
    }

    TEST_ASSERT_EQUAL(ESP_OK, imu_quat_delete(handle));
}

void app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_imu_quaternion_create_delete_uses_layered_defaults);
    RUN_TEST(test_imu_quaternion_default_heading_ref_body_is_negative_x);
    RUN_TEST(test_imu_quaternion_default_alignment_matches_airmouse_compatibility);
    RUN_TEST(test_imu_quaternion_custom_heading_target_axis_is_applied);
    RUN_TEST(test_imu_quaternion_custom_accel_target_axis_is_applied);
    RUN_TEST(test_imu_quaternion_parallel_target_axes_fail_initialization);
    RUN_TEST(test_imu_quaternion_antiparallel_target_axes_fail_initialization);
    RUN_TEST(test_imu_quaternion_bias_disabled_leaves_bias_state_unchanged);
    RUN_TEST(test_imu_quaternion_gyro_guard_disabled_does_not_reinitialize);
    RUN_TEST(test_imu_quaternion_default_warmup_runtime_state_is_initialized);
    RUN_TEST(test_imu_quaternion_remains_normalized_in_smoke_update_sequence);
    UNITY_END();
}
