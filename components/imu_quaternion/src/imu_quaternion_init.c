/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "imu_quaternion_init.h"
#include "imu_quaternion_math.h"

static bool imu_quat_axis_to_vec3(imu_quat_axis_t axis, float out[3])
{
    if (out == NULL) {
        return false;
    }

    switch (axis) {
    case IMU_QUAT_AXIS_POS_X:
        out[0] = 1.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
        return true;
    case IMU_QUAT_AXIS_NEG_X:
        out[0] = -1.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
        return true;
    case IMU_QUAT_AXIS_POS_Y:
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
        return true;
    case IMU_QUAT_AXIS_NEG_Y:
        out[0] = 0.0f;
        out[1] = -1.0f;
        out[2] = 0.0f;
        return true;
    case IMU_QUAT_AXIS_POS_Z:
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = 1.0f;
        return true;
    case IMU_QUAT_AXIS_NEG_Z:
        out[0] = 0.0f;
        out[1] = 0.0f;
        out[2] = -1.0f;
        return true;
    default:
        return false;
    }
}

void imu_quat_set_default_config(imu_quat_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->accel_target_axis = IMU_QUAT_AXIS_POS_Z;
    config->heading_ref_body[0] = -1.0f;
    config->heading_ref_body[1] = 0.0f;
    config->heading_ref_body[2] = 0.0f;
    config->heading_target_axis = IMU_QUAT_AXIS_NEG_X;
    config->mag_input_enabled = false;

    config->gyro_bias_enabled = true;

    config->gyro_guard_enabled = true;
    config->gyro_guard_limit_dps = 150.0f;
}

void imu_quat_reset_solver_runtime(imu_quat_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

    const float feedback_gain = IMU_QUAT_INTERNAL_FEEDBACK_GAIN;
    const float warmup_gain = IMU_QUAT_INTERNAL_WARMUP_GAIN;
    const float warmup_period_sec = IMU_QUAT_INTERNAL_WARMUP_PERIOD_SEC;

    handle->runtime_gain.steady_gain = feedback_gain;

    if (warmup_period_sec > 0.0f && warmup_gain > feedback_gain) {
        handle->runtime_gain.warming_up = true;
        handle->runtime_gain.live_gain = warmup_gain;
        handle->runtime_gain.gain_drop_per_sec =
            (warmup_gain - feedback_gain) / warmup_period_sec;
    } else {
        handle->runtime_gain.warming_up = false;
        handle->runtime_gain.live_gain = feedback_gain;
        handle->runtime_gain.gain_drop_per_sec = 0.0f;
    }
}

static bool imu_quat_initialize_from_gravity_heading(
    imu_quat_handle_t handle,
    const float accel_body[3],
    const float heading_body[3],
    imu_quat_axis_t accel_target_axis,
    imu_quat_axis_t heading_target_axis)
{
    if (handle == NULL || accel_body == NULL || heading_body == NULL) {
        return false;
    }

    /* Initialization uses a unified gravity-heading alignment path.
     * The heading source is always config.heading_ref_body, so the initial
     * device forward direction defines the zero pose. Magnetic input, if
     * available, is recorded separately as a world-frame reference after
     * initialization.
     */
    float p_body[3] = {accel_body[0], accel_body[1], accel_body[2]};
    if (!imu_quat_vec_normalize3(p_body)) {
        return false;
    }

    float r_body[3] = {
        heading_body[0],
        heading_body[1],
        heading_body[2],
    };
    if (!imu_quat_vec_normalize3(r_body)) {
        return false;
    }

    const float r_dot_p = imu_quat_vec_dot3(r_body, p_body);
    float s_body[3] = {
        r_body[0] - r_dot_p * p_body[0],
        r_body[1] - r_dot_p * p_body[1],
        r_body[2] - r_dot_p * p_body[2],
    };
    if (!imu_quat_vec_normalize3(s_body)) {
        return false;
    }

    float t_body[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_vec_cross3(p_body, s_body, t_body);
    if (!imu_quat_vec_normalize3(t_body)) {
        return false;
    }

    imu_quat_vec_cross3(t_body, p_body, s_body);
    if (!imu_quat_vec_normalize3(s_body)) {
        return false;
    }

    float p_world[3] = {0.0f, 0.0f, 0.0f};
    if (!imu_quat_axis_to_vec3(accel_target_axis, p_world)) {
        return false;
    }

    float s_world[3] = {0.0f, 0.0f, 0.0f};
    if (!imu_quat_axis_to_vec3(heading_target_axis, s_world)) {
        return false;
    }

    if (fabsf(imu_quat_vec_dot3(p_world, s_world)) > 1e-6f) {
        return false;
    }

    float t_world[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_vec_cross3(p_world, s_world, t_world);
    if (!imu_quat_vec_normalize3(t_world)) {
        return false;
    }

    float rot_bw[3][3] = {{0.0f}};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            rot_bw[i][j] =
                s_world[i] * s_body[j] +
                t_world[i] * t_body[j] +
                p_world[i] * p_body[j];
        }
    }

    if (imu_quat_from_rotation_matrix(
                rot_bw,
                &handle->pose.q_w,
                &handle->pose.q_x,
                &handle->pose.q_y,
                &handle->pose.q_z) != ESP_OK) {
        return false;
    }

    handle->pose.up_ref[0] = p_world[0];
    handle->pose.up_ref[1] = p_world[1];
    handle->pose.up_ref[2] = p_world[2];
    return true;
}

static void imu_quat_try_set_initial_mag_reference(
    imu_quat_handle_t handle,
    const imu_quat_sample_t *sample)
{
    if (handle == NULL || sample == NULL) {
        return;
    }

    handle->pose.mag_ref_valid = false;
    handle->pose.mag_ref[0] = 0.0f;
    handle->pose.mag_ref[1] = 0.0f;
    handle->pose.mag_ref[2] = 0.0f;

    if (!handle->config.mag_input_enabled || !sample->mag_valid) {
        return;
    }

    float mag_world[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_rotate_vec(
        handle->pose.q_w,
        handle->pose.q_x,
        handle->pose.q_y,
        handle->pose.q_z,
        sample->mag,
        mag_world);

    const float dot_up = imu_quat_vec_dot3(mag_world, handle->pose.up_ref);
    float mag_horizontal[3] = {
        mag_world[0] - dot_up * handle->pose.up_ref[0],
        mag_world[1] - dot_up * handle->pose.up_ref[1],
        mag_world[2] - dot_up * handle->pose.up_ref[2],
    };

    if (!imu_quat_vec_normalize3(mag_horizontal)) {
        return;
    }

    handle->pose.mag_ref[0] = mag_horizontal[0];
    handle->pose.mag_ref[1] = mag_horizontal[1];
    handle->pose.mag_ref[2] = mag_horizontal[2];
    handle->pose.mag_ref_valid = true;
}

esp_err_t imu_quat_reinitialize_from_sample(
    imu_quat_handle_t handle,
    const imu_quat_sample_t *sample)
{
    if (handle == NULL || sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool initialized = imu_quat_initialize_from_gravity_heading(
                                 handle,
                                 sample->accel,
                                 handle->config.heading_ref_body,
                                 handle->config.accel_target_axis,
                                 handle->config.heading_target_axis);
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    imu_quat_try_set_initial_mag_reference(handle, sample);
    handle->pose.last_timestamp_us = sample->timestamp_us;

    handle->bias.rest_detected = false;
    handle->bias.bias_lp_valid = false;
    handle->bias.rest_time_sec = 0.0f;
    handle->gyro_guard.gyro_over_limit_time_sec = 0.0f;
    handle->gyro_guard.gyro_recover_time_sec = 0.0f;
    handle->gyro_guard.gyro_reinit_armed = false;
    handle->rejection.accel_reject_count = 0;
    handle->rejection.mag_reject_count = 0;
    handle->rejection.accel_ignored = false;
    handle->rejection.mag_ignored = false;
    handle->rejection.mag_error_lp_valid = false;
    handle->rejection.mag_error_lp[0] = 0.0f;
    handle->rejection.mag_error_lp[1] = 0.0f;
    handle->rejection.mag_error_lp[2] = 0.0f;

    handle->bias.rest_gyro_lp[0] = sample->gyro[0];
    handle->bias.rest_gyro_lp[1] = sample->gyro[1];
    handle->bias.rest_gyro_lp[2] = sample->gyro[2];
    handle->bias.rest_acc_lp[0] = sample->accel[0];
    handle->bias.rest_acc_lp[1] = sample->accel[1];
    handle->bias.rest_acc_lp[2] = sample->accel[2];

    imu_quat_reset_solver_runtime(handle);
    return ESP_OK;
}
