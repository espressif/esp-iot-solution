/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "esp_log.h"
#include "imu_quaternion_guard.h"
#include "imu_quaternion_init.h"

#define TAG "IMU_QUAT"

esp_err_t imu_quat_handle_gyro_guard(
    imu_quat_handle_t handle,
    const imu_quat_sample_t *sample,
    float dt_sec,
    bool *reinitialized_out)
{
    if (handle == NULL || sample == NULL || reinitialized_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *reinitialized_out = false;

    if (!handle->config.gyro_guard_enabled) {
        return ESP_OK;
    }

    const float gyro_x_dps = sample->gyro[0] - handle->bias.gyro_bias[0] * IMU_QUAT_RAD2DEG;
    const float gyro_y_dps = sample->gyro[1] - handle->bias.gyro_bias[1] * IMU_QUAT_RAD2DEG;
    const float gyro_z_dps = sample->gyro[2] - handle->bias.gyro_bias[2] * IMU_QUAT_RAD2DEG;

    const bool gyro_over_limit =
        fabsf(gyro_x_dps) > handle->config.gyro_guard_limit_dps ||
        fabsf(gyro_y_dps) > handle->config.gyro_guard_limit_dps ||
        fabsf(gyro_z_dps) > handle->config.gyro_guard_limit_dps;
    const bool gyro_recovered =
        fabsf(gyro_x_dps) < IMU_QUAT_INTERNAL_GYRO_RECOVER_DPS &&
        fabsf(gyro_y_dps) < IMU_QUAT_INTERNAL_GYRO_RECOVER_DPS &&
        fabsf(gyro_z_dps) < IMU_QUAT_INTERNAL_GYRO_RECOVER_DPS;

    const float reinit_window_sec =
        (float)IMU_QUAT_INTERNAL_GYRO_REINIT_WINDOW_MS / 1000.0f;
    const float recover_window_sec =
        (float)IMU_QUAT_INTERNAL_GYRO_RECOVER_WINDOW_MS / 1000.0f;

    if (gyro_over_limit) {
        handle->gyro_guard.gyro_over_limit_time_sec += dt_sec;
        handle->gyro_guard.gyro_recover_time_sec = 0.0f;
        if (!handle->gyro_guard.gyro_reinit_armed &&
                handle->gyro_guard.gyro_over_limit_time_sec >= reinit_window_sec) {
            handle->gyro_guard.gyro_reinit_armed = true;
            ESP_LOGW(
                TAG,
                "gyro over limit for %.3f s, quat reinit armed",
                (double)handle->gyro_guard.gyro_over_limit_time_sec);
        }
    }

    if (handle->gyro_guard.gyro_reinit_armed) {
        if (gyro_recovered) {
            handle->gyro_guard.gyro_recover_time_sec += dt_sec;
            if (handle->gyro_guard.gyro_recover_time_sec >= recover_window_sec) {
                ESP_LOGW(
                    TAG,
                    "gyro recovered below %.1f dps for %.3f s, reinitializing quat state",
                    (double)IMU_QUAT_INTERNAL_GYRO_RECOVER_DPS,
                    (double)handle->gyro_guard.gyro_recover_time_sec);
                const esp_err_t ret = imu_quat_reinitialize_from_sample(handle, sample);
                if (ret != ESP_OK) {
                    return ret;
                }
                *reinitialized_out = true;
                return ESP_OK;
            }
        } else {
            handle->gyro_guard.gyro_recover_time_sec = 0.0f;
        }
    }

    if (!gyro_over_limit && !handle->gyro_guard.gyro_reinit_armed) {
        handle->gyro_guard.gyro_over_limit_time_sec = 0.0f;
        handle->gyro_guard.gyro_recover_time_sec = 0.0f;
    }

    return ESP_OK;
}
