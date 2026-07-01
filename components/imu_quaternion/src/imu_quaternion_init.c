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

    config->init_strategy = IMU_QUAT_INIT_CURRENT_POSE;
    config->accel_target_axis = IMU_QUAT_AXIS_POS_Z;
    config->heading_ref_body[0] = -1.0f;
    config->heading_ref_body[1] = 0.0f;
    config->heading_ref_body[2] = 0.0f;
    config->heading_target_axis = IMU_QUAT_AXIS_NEG_X;

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

void imu_quat_set_current_pose_reference(
    imu_quat_handle_t handle,
    const float accel[3])
{
    handle->pose.up_ref[0] = accel[0];
    handle->pose.up_ref[1] = accel[1];
    handle->pose.up_ref[2] = accel[2];

    if (!imu_quat_vec_normalize3(handle->pose.up_ref)) {
        handle->pose.up_ref[0] = 0.0f;
        handle->pose.up_ref[1] = 0.0f;
        handle->pose.up_ref[2] = 1.0f;
    }

    handle->pose.q_w = 1.0f;
    handle->pose.q_x = 0.0f;
    handle->pose.q_y = 0.0f;
    handle->pose.q_z = 0.0f;
}

bool imu_quat_set_accel_heading_alignment_reference(
    imu_quat_handle_t handle,
    const float accel[3])
{
    if (handle == NULL || accel == NULL) {
        return false;
    }

    /* Historical function name kept for compatibility with the local
     * component migration. The current behavior performs generic axis-based
     * accel and heading alignment rather than a fixed world-Z/world-neg-X
     * mapping.
     */

    // This initialization builds a full body-to-world frame using two constraints:
    //
    // 1. The measured accel/up direction in body frame is aligned to
    //    config.accel_target_axis in world frame.
    //
    // 2. config.heading_ref_body is projected onto the plane perpendicular
    //    to accel/up direction, and the projected heading is aligned to
    //    config.heading_target_axis in world frame.
    //
    // The target axes are selected from +-X / +-Y / +-Z.
    // accel_target_axis and heading_target_axis must be orthogonal.
    //
    // The resulting rotation satisfies:
    //     R * p_body = p_world
    //     R * s_body = s_world
    //     R * t_body = t_world
    //
    // with:
    //     B = [s_body, t_body, p_body]
    //     W = [s_world, t_world, p_world]
    //     R = W * B^T

    float p_body[3] = {accel[0], accel[1], accel[2]};
    if (!imu_quat_vec_normalize3(p_body)) {
        return false;
    }

    float r_body[3] = {
        handle->config.heading_ref_body[0],
        handle->config.heading_ref_body[1],
        handle->config.heading_ref_body[2],
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
    if (!imu_quat_axis_to_vec3(handle->config.accel_target_axis, p_world)) {
        return false;
    }

    float s_world[3] = {0.0f, 0.0f, 0.0f};
    if (!imu_quat_axis_to_vec3(handle->config.heading_target_axis, s_world)) {
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

esp_err_t imu_quat_reinitialize_from_sample(
    imu_quat_handle_t handle,
    const float accel[3],
    const float gyro[3],
    int64_t now_us)
{
    if (handle == NULL || accel == NULL || gyro == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->config.init_strategy == IMU_QUAT_INIT_ACCEL_HEADING_ALIGNMENT) {
        if (!imu_quat_set_accel_heading_alignment_reference(handle, accel)) {
            return ESP_ERR_INVALID_STATE;
        }
    } else {
        imu_quat_set_current_pose_reference(handle, accel);
    }

    handle->pose.last_timestamp_us = now_us;

    handle->bias.rest_detected = false;
    handle->bias.bias_lp_valid = false;
    handle->bias.rest_time_sec = 0.0f;
    handle->gyro_guard.gyro_over_limit_time_sec = 0.0f;
    handle->gyro_guard.gyro_recover_time_sec = 0.0f;
    handle->gyro_guard.gyro_reinit_armed = false;

    handle->bias.rest_gyro_lp[0] = gyro[0];
    handle->bias.rest_gyro_lp[1] = gyro[1];
    handle->bias.rest_gyro_lp[2] = gyro[2];
    handle->bias.rest_acc_lp[0] = accel[0];
    handle->bias.rest_acc_lp[1] = accel[1];
    handle->bias.rest_acc_lp[2] = accel[2];

    imu_quat_reset_solver_runtime(handle);
    return ESP_OK;
}
