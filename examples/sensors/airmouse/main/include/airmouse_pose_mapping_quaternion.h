/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "airmouse_config.h"
#include "esp_err.h"
#include "imu_quaternion.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool has_relative;
    int8_t dx;
    int8_t dy;

    bool has_absolute;
    uint16_t abs_x;
    uint16_t abs_y;
} airmouse_pose_mapping_cursor_output_t;

typedef struct {
    imu_quat_handle_t quat_handle;
    imu_quat_axis_t heading_world_axis;
    imu_quat_axis_t up_axis;
    imu_quat_axis_t screen_x_axis;
    imu_quat_axis_t screen_y_axis;

    float f_ref[3];
    float f_stable[3];

    float sensitivity_gain_x;
    float sensitivity_gain_y;

    float last_u;
    float last_v;
    bool has_last_uv;

    float rem_x;
    float rem_y;
    bool startup_gyro_bias_valid;
    float startup_gyro_bias_dps[3];
    bool recenter_pending;
    int64_t last_sample_us;
    int64_t last_forward_log_us;
    int64_t last_waiting_update_log_us;
    int64_t last_deadzone_log_us;
    int64_t last_projection_fail_log_us;
} airmouse_pose_mapping_quaternion_handle_t;

esp_err_t airmouse_pose_mapping_quaternion_init(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const airmouse_pose_config_t *config);

esp_err_t airmouse_pose_mapping_request_recenter(
    airmouse_pose_mapping_quaternion_handle_t *handle);

esp_err_t airmouse_pose_mapping_set_startup_gyro_bias(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float gyro_bias_dps[3]);

esp_err_t airmouse_pose_mapping_update_cursor_from_sample(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float accel[3],
    const float gyro[3],
    bool mag_valid,
    const float mag[3],
    int64_t now_us,
    airmouse_pose_mapping_cursor_output_t *out);

#ifdef __cplusplus
}
#endif
