/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "imu_quaternion_priv.h"

#include "imu_quaternion_math.h"

esp_err_t imu_quat_complementary_update(
    imu_quat_handle_t handle,
    const float accel[3],
    const float gyro[3],
    float dt_sec)
{
    if (handle == NULL || accel == NULL || gyro == NULL || dt_sec <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    float qw = handle->pose.q_w;
    float qx = handle->pose.q_x;
    float qy = handle->pose.q_y;
    float qz = handle->pose.q_z;

    const float gx = gyro[0] * IMU_QUAT_DEG2RAD - handle->bias.gyro_bias[0];
    const float gy = gyro[1] * IMU_QUAT_DEG2RAD - handle->bias.gyro_bias[1];
    const float gz = gyro[2] * IMU_QUAT_DEG2RAD - handle->bias.gyro_bias[2];

    float dq_w = 0.0f;
    float dq_x = 0.0f;
    float dq_y = 0.0f;
    float dq_z = 0.0f;
    imu_quat_multiply(
        qw, qx, qy, qz,
        0.0f, gx, gy, gz,
        &dq_w, &dq_x, &dq_y, &dq_z);

    qw += 0.5f * dq_w * dt_sec;
    qx += 0.5f * dq_x * dt_sec;
    qy += 0.5f * dq_y * dt_sec;
    qz += 0.5f * dq_z * dt_sec;
    imu_quat_normalize(&qw, &qx, &qy, &qz);

    float measured_up_body[3] = {accel[0], accel[1], accel[2]};
    const float acc_norm = imu_quat_vec_norm3(measured_up_body);
    if (acc_norm <= 0.85f || acc_norm >= 1.15f) {
        handle->pose.q_w = qw;
        handle->pose.q_x = qx;
        handle->pose.q_y = qy;
        handle->pose.q_z = qz;
        return ESP_OK;
    }

    imu_quat_vec_normalize3(measured_up_body);

    float predicted_up_body[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_rotate_vec_transpose(qw, qx, qy, qz, handle->pose.up_ref, predicted_up_body);
    imu_quat_vec_normalize3(predicted_up_body);

    float error[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_vec_cross3(measured_up_body, predicted_up_body, error);

    if (handle->runtime_gain.warming_up) {
        handle->runtime_gain.live_gain -= handle->runtime_gain.gain_drop_per_sec * dt_sec;
        if (handle->runtime_gain.live_gain <= handle->runtime_gain.steady_gain ||
                handle->runtime_gain.steady_gain <= 0.0f) {
            handle->runtime_gain.live_gain = handle->runtime_gain.steady_gain;
            handle->runtime_gain.warming_up = false;
        }
    }

    const float correction_gain = handle->runtime_gain.live_gain;
    const float gx_corr = gx + correction_gain * error[0];
    const float gy_corr = gy + correction_gain * error[1];
    const float gz_corr = gz + correction_gain * error[2];

    qw = handle->pose.q_w;
    qx = handle->pose.q_x;
    qy = handle->pose.q_y;
    qz = handle->pose.q_z;
    imu_quat_multiply(
        qw, qx, qy, qz,
        0.0f, gx_corr, gy_corr, gz_corr,
        &dq_w, &dq_x, &dq_y, &dq_z);

    qw += 0.5f * dq_w * dt_sec;
    qx += 0.5f * dq_x * dt_sec;
    qy += 0.5f * dq_y * dt_sec;
    qz += 0.5f * dq_z * dt_sec;
    imu_quat_normalize(&qw, &qx, &qy, &qz);

    handle->pose.q_w = qw;
    handle->pose.q_x = qx;
    handle->pose.q_y = qy;
    handle->pose.q_z = qz;
    return ESP_OK;
}

void imu_quat_fill_output(
    imu_quat_handle_t handle,
    imu_quat_output_t *out)
{
    if (out == NULL) {
        return;
    }

    out->q_w = handle->pose.q_w;
    out->q_x = handle->pose.q_x;
    out->q_y = handle->pose.q_y;
    out->q_z = handle->pose.q_z;
    out->rest_detected = handle->bias.rest_detected;
}
