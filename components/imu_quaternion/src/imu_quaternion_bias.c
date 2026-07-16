/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "imu_quaternion_bias.h"

esp_err_t imu_quat_update_rest_bias(
    imu_quat_handle_t handle,
    const float accel[3],
    const float gyro[3],
    float dt_sec)
{
    if (handle == NULL || accel == NULL || gyro == NULL || dt_sec <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!handle->config.gyro_bias_enabled) {
        return ESP_OK;
    }

    const bool prev_rest_detected = handle->bias.rest_detected;
    const float gyro_x_dps_for_rest =
        gyro[0] - handle->bias.gyro_bias[0] * IMU_QUAT_RAD2DEG;
    const float gyro_y_dps_for_rest =
        gyro[1] - handle->bias.gyro_bias[1] * IMU_QUAT_RAD2DEG;
    const float gyro_z_dps_for_rest =
        gyro[2] - handle->bias.gyro_bias[2] * IMU_QUAT_RAD2DEG;
    bool allow_rest_filter_update = true;

    if (handle->config.gyro_guard_enabled) {
        allow_rest_filter_update =
            fabsf(gyro_x_dps_for_rest) <= handle->config.gyro_guard_limit_dps &&
            fabsf(gyro_y_dps_for_rest) <= handle->config.gyro_guard_limit_dps &&
            fabsf(gyro_z_dps_for_rest) <= handle->config.gyro_guard_limit_dps;
    }

    if (allow_rest_filter_update) {
        float alpha = dt_sec / (IMU_QUAT_INTERNAL_REST_FILTER_TAU_SEC + dt_sec);
        if (alpha < 0.0f) {
            alpha = 0.0f;
        } else if (alpha > 1.0f) {
            alpha = 1.0f;
        }

        for (int i = 0; i < 3; ++i) {
            handle->bias.rest_gyro_lp[i] += alpha * (gyro[i] - handle->bias.rest_gyro_lp[i]);
            handle->bias.rest_acc_lp[i] += alpha * (accel[i] - handle->bias.rest_acc_lp[i]);
        }

        const float gyro_dx = gyro[0] - handle->bias.rest_gyro_lp[0];
        const float gyro_dy = gyro[1] - handle->bias.rest_gyro_lp[1];
        const float gyro_dz = gyro[2] - handle->bias.rest_gyro_lp[2];
        const float gyro_dev = sqrtf(
                                   gyro_dx * gyro_dx + gyro_dy * gyro_dy + gyro_dz * gyro_dz);

        const float acc_dx = accel[0] - handle->bias.rest_acc_lp[0];
        const float acc_dy = accel[1] - handle->bias.rest_acc_lp[1];
        const float acc_dz = accel[2] - handle->bias.rest_acc_lp[2];
        const float acc_dev = sqrtf(acc_dx * acc_dx + acc_dy * acc_dy + acc_dz * acc_dz);

        if (gyro_dev < IMU_QUAT_INTERNAL_REST_GYRO_THRESHOLD_DPS &&
                acc_dev < IMU_QUAT_INTERNAL_REST_ACC_THRESHOLD_G) {
            handle->bias.rest_time_sec += dt_sec;
            if (handle->bias.rest_time_sec >= IMU_QUAT_INTERNAL_REST_MIN_TIME_SEC) {
                handle->bias.rest_detected = true;
            }
        } else {
            handle->bias.rest_time_sec = 0.0f;
            handle->bias.rest_detected = false;
        }
    }

    if (!prev_rest_detected && handle->bias.rest_detected) {
        handle->bias.bias_gyro_lp[0] = gyro[0];
        handle->bias.bias_gyro_lp[1] = gyro[1];
        handle->bias.bias_gyro_lp[2] = gyro[2];
        handle->bias.bias_lp_valid = true;
    } else if (!handle->bias.rest_detected) {
        handle->bias.bias_lp_valid = false;
    }

    if (handle->bias.rest_detected && handle->bias.bias_lp_valid) {
        float beta = dt_sec / (IMU_QUAT_INTERNAL_BIAS_FILTER_TAU_SEC + dt_sec);
        if (beta < 0.0f) {
            beta = 0.0f;
        } else if (beta > 1.0f) {
            beta = 1.0f;
        }

        for (int i = 0; i < 3; ++i) {
            handle->bias.bias_gyro_lp[i] += beta * (gyro[i] - handle->bias.bias_gyro_lp[i]);
        }
    }

    if (handle->bias.rest_detected && handle->bias.bias_lp_valid) {
        const float bias_clip = IMU_QUAT_INTERNAL_BIAS_CLIP_DPS * IMU_QUAT_DEG2RAD;

        for (int i = 0; i < 3; ++i) {
            const float gyro_lp_rad = handle->bias.bias_gyro_lp[i] * IMU_QUAT_DEG2RAD;
            const float bias_err = gyro_lp_rad - handle->bias.gyro_bias[i];
            handle->bias.gyro_bias[i] +=
                IMU_QUAT_INTERNAL_BIAS_REST_GAIN * bias_err * dt_sec;

            if (handle->bias.gyro_bias[i] > bias_clip) {
                handle->bias.gyro_bias[i] = bias_clip;
            } else if (handle->bias.gyro_bias[i] < -bias_clip) {
                handle->bias.gyro_bias[i] = -bias_clip;
            }
        }
    }

    return ESP_OK;
}
