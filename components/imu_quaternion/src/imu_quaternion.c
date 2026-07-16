/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>

#include "imu_quaternion.h"
#include "imu_quaternion_bias.h"
#include "imu_quaternion_guard.h"
#include "imu_quaternion_init.h"
#include "imu_quaternion_math.h"
#include "imu_quaternion_priv.h"

static bool imu_quat_reset_axis_to_vec3(imu_quat_axis_t axis, float out[3])
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

esp_err_t imu_quat_create(
    const imu_quat_config_t *config,
    imu_quat_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_quat_handle_t handle = (imu_quat_handle_t)calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    imu_quat_set_default_config(&handle->config);
    if (config != NULL) {
        handle->config = *config;
    }

    const esp_err_t ret = imu_quat_reset_state(handle);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }

    *out_handle = handle;
    return ESP_OK;
}

esp_err_t imu_quat_delete(imu_quat_handle_t handle)
{
    if (handle == NULL) {
        return ESP_OK;
    }

    free(handle);
    return ESP_OK;
}

esp_err_t imu_quat_set_config(
    imu_quat_handle_t handle,
    const imu_quat_config_t *config)
{
    if (handle == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->config = *config;
    return ESP_OK;
}

esp_err_t imu_quat_reset_state(imu_quat_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->pose.q_w = 1.0f;
    handle->pose.q_x = 0.0f;
    handle->pose.q_y = 0.0f;
    handle->pose.q_z = 0.0f;

    if (!imu_quat_reset_axis_to_vec3(handle->config.accel_target_axis,
                                     handle->pose.up_ref)) {
        handle->pose.up_ref[0] = 0.0f;
        handle->pose.up_ref[1] = 0.0f;
        handle->pose.up_ref[2] = 1.0f;
    }
    handle->pose.mag_ref_valid = false;
    handle->pose.mag_ref[0] = 0.0f;
    handle->pose.mag_ref[1] = 0.0f;
    handle->pose.mag_ref[2] = 0.0f;
    handle->pose.last_timestamp_us = 0;

    handle->bias.gyro_bias[0] = 0.0f;
    handle->bias.gyro_bias[1] = 0.0f;
    handle->bias.gyro_bias[2] = 0.0f;

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
    memset(handle->rejection.mag_error_lp,
           0,
           sizeof(handle->rejection.mag_error_lp));

    memset(handle->bias.rest_gyro_lp, 0, sizeof(handle->bias.rest_gyro_lp));
    memset(handle->bias.rest_acc_lp, 0, sizeof(handle->bias.rest_acc_lp));
    memset(handle->bias.bias_gyro_lp, 0, sizeof(handle->bias.bias_gyro_lp));

    imu_quat_reset_solver_runtime(handle);
    return ESP_OK;
}

esp_err_t imu_quat_set_gyro_bias_dps(
    imu_quat_handle_t handle,
    const float gyro_bias_dps[3])
{
    if (handle == NULL || gyro_bias_dps == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->bias.gyro_bias[0] = gyro_bias_dps[0] * IMU_QUAT_DEG2RAD;
    handle->bias.gyro_bias[1] = gyro_bias_dps[1] * IMU_QUAT_DEG2RAD;
    handle->bias.gyro_bias[2] = gyro_bias_dps[2] * IMU_QUAT_DEG2RAD;
    return ESP_OK;
}

esp_err_t imu_quat_get_gyro_bias_dps(
    imu_quat_handle_t handle,
    float gyro_bias_dps[3])
{
    if (handle == NULL || gyro_bias_dps == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gyro_bias_dps[0] = handle->bias.gyro_bias[0] * IMU_QUAT_RAD2DEG;
    gyro_bias_dps[1] = handle->bias.gyro_bias[1] * IMU_QUAT_RAD2DEG;
    gyro_bias_dps[2] = handle->bias.gyro_bias[2] * IMU_QUAT_RAD2DEG;
    return ESP_OK;
}

esp_err_t imu_quat_update(
    imu_quat_handle_t handle,
    const imu_quat_sample_t *sample,
    imu_quat_output_t *out)
{
    if (handle == NULL || sample == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    if (handle->pose.last_timestamp_us == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t dt_us = sample->timestamp_us - handle->pose.last_timestamp_us;
    if (dt_us <= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    handle->pose.last_timestamp_us = sample->timestamp_us;
    if (dt_us > 100000) {
        dt_us = 100000;
    }

    const float dt_sec = (float)dt_us * 1e-6f;

    bool reinitialized = false;
    esp_err_t ret = imu_quat_handle_gyro_guard(handle,
                                               sample,
                                               dt_sec,
                                               &reinitialized);
    if (ret != ESP_OK) {
        return ret;
    }
    if (reinitialized) {
        out->reinitialized = true;
        imu_quat_fill_output(handle, out);
        return ESP_OK;
    }

    ret = imu_quat_update_rest_bias(handle,
                                    sample->accel,
                                    sample->gyro,
                                    dt_sec);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = imu_quat_complementary_update(handle, sample, dt_sec);
    if (ret != ESP_OK) {
        return ret;
    }

    out->updated = true;
    imu_quat_fill_output(handle, out);
    return ESP_OK;
}

esp_err_t imu_quat_rotate_body_to_world(
    imu_quat_handle_t handle,
    const float v_body[3],
    float v_world[3])
{
    if (handle == NULL || v_body == NULL || v_world == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_quat_rotate_vec(
        handle->pose.q_w,
        handle->pose.q_x,
        handle->pose.q_y,
        handle->pose.q_z,
        v_body,
        v_world);
    return ESP_OK;
}

esp_err_t imu_quat_rotate_world_to_body(
    imu_quat_handle_t handle,
    const float v_world[3],
    float v_body[3])
{
    if (handle == NULL || v_world == NULL || v_body == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_quat_rotate_vec_transpose(
        handle->pose.q_w,
        handle->pose.q_x,
        handle->pose.q_y,
        handle->pose.q_z,
        v_world,
        v_body);
    return ESP_OK;
}
