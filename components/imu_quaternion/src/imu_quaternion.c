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

    handle->pose.up_ref[0] = 0.0f;
    handle->pose.up_ref[1] = 0.0f;
    handle->pose.up_ref[2] = 1.0f;
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

    memset(handle->bias.rest_gyro_lp, 0, sizeof(handle->bias.rest_gyro_lp));
    memset(handle->bias.rest_acc_lp, 0, sizeof(handle->bias.rest_acc_lp));
    memset(handle->bias.bias_gyro_lp, 0, sizeof(handle->bias.bias_gyro_lp));

    imu_quat_reset_solver_runtime(handle);
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
        const esp_err_t ret = imu_quat_reinitialize_from_sample(
                                  handle, sample->accel, sample->gyro, sample->timestamp_us);
        if (ret != ESP_OK) {
            return ESP_ERR_INVALID_STATE;
        }
        imu_quat_fill_output(handle, out);
        return ESP_OK;
    }

    int64_t dt_us = sample->timestamp_us - handle->pose.last_timestamp_us;
    handle->pose.last_timestamp_us = sample->timestamp_us;
    if (dt_us <= 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (dt_us > 100000) {
        dt_us = 100000;
    }

    const float dt_sec = (float)dt_us * 1e-6f;

    bool reinitialized = false;
    esp_err_t ret = imu_quat_handle_gyro_guard(
                        handle,
                        sample->accel,
                        sample->gyro,
                        sample->timestamp_us,
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

    ret = imu_quat_update_rest_bias(handle, sample->accel, sample->gyro, dt_sec);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = imu_quat_complementary_update(handle, sample->accel, sample->gyro, dt_sec);
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
