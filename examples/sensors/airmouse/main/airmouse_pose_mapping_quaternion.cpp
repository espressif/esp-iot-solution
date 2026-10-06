/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_pose_mapping_quaternion.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>

#define TAG "AIRMOUSE"
static constexpr int64_t AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US = 1000000;
static constexpr float AIRMOUSE_POSE_RELATIVE_NORMALIZED_TO_COUNT = 1000.0f;

static inline float airmouse_vec_dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline float airmouse_vec_norm3(const float v[3])
{
    return sqrtf(airmouse_vec_dot3(v, v));
}

static bool airmouse_axis_to_vec3(imu_quat_axis_t axis, float out[3])
{
    if (out == nullptr) {
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

static void airmouse_pose_mapping_set_default_axes(int *forward_axis,
                                                   int *up_axis,
                                                   int *screen_x_axis,
                                                   int *screen_y_axis)
{
    if (forward_axis != nullptr) {
        *forward_axis = IMU_QUAT_AXIS_NEG_X;
    }
    if (up_axis != nullptr) {
        *up_axis = IMU_QUAT_AXIS_POS_Z;
    }
    if (screen_x_axis != nullptr) {
        *screen_x_axis = IMU_QUAT_AXIS_POS_Y;
    }
    if (screen_y_axis != nullptr) {
        *screen_y_axis = IMU_QUAT_AXIS_NEG_Z;
    }
}

static float airmouse_pose_mapping_default_sensitivity_gain_x(void)
{
    return (float)CONFIG_AIRMOUSE_POSE_GAIN_X_CENTI / 100.0f;
}

static float airmouse_pose_mapping_default_sensitivity_gain_y(void)
{
    return (float)CONFIG_AIRMOUSE_POSE_GAIN_Y_CENTI / 100.0f;
}

static void airmouse_pose_mapping_clear_cursor_history(
    airmouse_pose_mapping_quaternion_handle_t *handle)
{
    if (handle == nullptr) {
        return;
    }

    handle->has_last_uv = false;
    handle->last_u = 0.0f;
    handle->last_v = 0.0f;
    handle->rem_x = 0.0f;
    handle->rem_y = 0.0f;
}

static esp_err_t airmouse_pose_mapping_apply_startup_gyro_bias(
    airmouse_pose_mapping_quaternion_handle_t *handle)
{
    if (handle == nullptr || handle->quat_handle == nullptr ||
            !handle->startup_gyro_bias_valid) {
        return ESP_OK;
    }

    return imu_quat_set_gyro_bias_dps(handle->quat_handle,
                                      handle->startup_gyro_bias_dps);
}

static inline bool airmouse_vec_normalize3(float v[3])
{
    const float norm = airmouse_vec_norm3(v);
    if (norm <= 1e-6f) {
        return false;
    }

    v[0] /= norm;
    v[1] /= norm;
    v[2] /= norm;
    return true;
}

static esp_err_t airmouse_pose_mapping_get_range_rad(float *range_x_out,
                                                     float *range_y_out)
{
    if (range_x_out == nullptr || range_y_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const float deg_to_rad = 0.01745329252f;
    const float range_x =
        (float)CONFIG_AIRMOUSE_POSE_RANGE_X_DEG * deg_to_rad;
    const float range_y =
        (float)CONFIG_AIRMOUSE_POSE_RANGE_Y_DEG * deg_to_rad;

    if (range_x <= 1e-6f || range_y <= 1e-6f) {
        return ESP_ERR_INVALID_STATE;
    }

    *range_x_out = range_x;
    *range_y_out = range_y;
    return ESP_OK;
}

static imu_quat_config_t airmouse_pose_mapping_build_quat_config(
    const airmouse_pose_mapping_quaternion_handle_t *handle)
{
    imu_quat_config_t quat_config = {};
    int heading_world_axis = IMU_QUAT_AXIS_NEG_X;
    int up_axis = IMU_QUAT_AXIS_POS_Z;
    float up_axis_vec[3] = {0.0f, 0.0f, 0.0f};

    if (handle != nullptr) {
        heading_world_axis = handle->heading_world_axis;
        up_axis = handle->up_axis;
    }

    if (!airmouse_axis_to_vec3((imu_quat_axis_t)up_axis, up_axis_vec)) {
        up_axis = IMU_QUAT_AXIS_POS_Z;
    }
    quat_config.accel_target_axis = (imu_quat_axis_t)up_axis;
    quat_config.heading_target_axis = (imu_quat_axis_t)heading_world_axis;
    if (!airmouse_axis_to_vec3(quat_config.heading_target_axis,
                               quat_config.heading_ref_body)) {
        quat_config.heading_target_axis = IMU_QUAT_AXIS_NEG_X;
        quat_config.heading_ref_body[0] = -1.0f;
        quat_config.heading_ref_body[1] = 0.0f;
        quat_config.heading_ref_body[2] = 0.0f;
    }

#if CONFIG_AIRMOUSE_ENABLE_BMM350
    quat_config.mag_input_enabled = true;
#else
    quat_config.mag_input_enabled = false;
#endif

    quat_config.gyro_bias_enabled = true;

#if CONFIG_AIRMOUSE_POSE_ENABLE_GYRO_OVERLIMIT_RESET
    quat_config.gyro_guard_enabled = true;
#else
    quat_config.gyro_guard_enabled = false;
#endif
    quat_config.gyro_guard_limit_dps = (float)CONFIG_AIRMOUSE_POSE_GYRO_LIMIT_DPS;
    return quat_config;
}

static void airmouse_pose_mapping_reset_stable_forward_from_quaternion(
    airmouse_pose_mapping_quaternion_handle_t *handle)
{
    if (handle == nullptr || handle->quat_handle == nullptr) {
        return;
    }

    float f_init[3] = {0.0f, 0.0f, 0.0f};
    if (imu_quat_rotate_body_to_world(handle->quat_handle, handle->f_ref, f_init) != ESP_OK ||
            !airmouse_vec_normalize3(f_init)) {
        if (!airmouse_axis_to_vec3(handle->heading_world_axis, handle->f_stable)) {
            (void)airmouse_axis_to_vec3(IMU_QUAT_AXIS_NEG_X,
                                        handle->f_stable);
        }
        return;
    }

    handle->f_stable[0] = f_init[0];
    handle->f_stable[1] = f_init[1];
    handle->f_stable[2] = f_init[2];
}

esp_err_t airmouse_pose_mapping_request_recenter(
    airmouse_pose_mapping_quaternion_handle_t *handle)
{
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->recenter_pending = true;
    airmouse_pose_mapping_clear_cursor_history(handle);

    if (handle->quat_handle != nullptr) {
        const esp_err_t ret = imu_quat_reset_state(handle->quat_handle);
        if (ret != ESP_OK) {
            return ret;
        }

        const esp_err_t bias_ret =
            airmouse_pose_mapping_apply_startup_gyro_bias(handle);
        if (bias_ret != ESP_OK) {
            return bias_ret;
        }
    }

    ESP_LOGI(TAG, "pose mapping: recenter requested");
    return ESP_OK;
}

esp_err_t airmouse_pose_mapping_set_startup_gyro_bias(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float gyro_bias_dps[3])
{
    if (handle == nullptr || gyro_bias_dps == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    handle->startup_gyro_bias_valid = true;
    handle->startup_gyro_bias_dps[0] = gyro_bias_dps[0];
    handle->startup_gyro_bias_dps[1] = gyro_bias_dps[1];
    handle->startup_gyro_bias_dps[2] = gyro_bias_dps[2];

    return airmouse_pose_mapping_apply_startup_gyro_bias(handle);
}

esp_err_t airmouse_pose_mapping_quaternion_init(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const airmouse_pose_config_t *config)
{
    if (handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_quat_handle_t quat_handle = handle->quat_handle;
    const bool startup_gyro_bias_valid = handle->startup_gyro_bias_valid;
    float startup_gyro_bias_dps[3] = {
        handle->startup_gyro_bias_dps[0],
        handle->startup_gyro_bias_dps[1],
        handle->startup_gyro_bias_dps[2],
    };
    int forward_axis = 0;
    int up_axis = 0;
    int screen_x_axis = 0;
    int screen_y_axis = 0;

    airmouse_pose_mapping_set_default_axes(&forward_axis,
                                           &up_axis,
                                           &screen_x_axis,
                                           &screen_y_axis);

    if (config != nullptr) {
        forward_axis = config->forward_axis;
        up_axis = config->up_axis;
        screen_x_axis = config->screen_x_axis;
        screen_y_axis = config->screen_y_axis;
    }

    memset(handle, 0, sizeof(*handle));
    handle->quat_handle = quat_handle;
    handle->startup_gyro_bias_valid = startup_gyro_bias_valid;
    handle->startup_gyro_bias_dps[0] = startup_gyro_bias_dps[0];
    handle->startup_gyro_bias_dps[1] = startup_gyro_bias_dps[1];
    handle->startup_gyro_bias_dps[2] = startup_gyro_bias_dps[2];
    handle->heading_world_axis = (imu_quat_axis_t)forward_axis;
    handle->up_axis = (imu_quat_axis_t)up_axis;
    handle->screen_x_axis = (imu_quat_axis_t)screen_x_axis;
    handle->screen_y_axis = (imu_quat_axis_t)screen_y_axis;
    handle->sensitivity_gain_x = airmouse_pose_mapping_default_sensitivity_gain_x();
    handle->sensitivity_gain_y = airmouse_pose_mapping_default_sensitivity_gain_y();
    if (config != nullptr && config->sensitivity_centi > 0) {
        handle->sensitivity_gain_x = (float)config->sensitivity_centi / 100.0f;
        handle->sensitivity_gain_y = (float)config->sensitivity_centi / 100.0f;
    }

    if (!airmouse_axis_to_vec3(handle->heading_world_axis, handle->f_ref)) {
        handle->heading_world_axis = IMU_QUAT_AXIS_NEG_X;
        handle->f_ref[0] = -1.0f;
        handle->f_ref[1] = 0.0f;
        handle->f_ref[2] = 0.0f;
    }
    float up_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    if (!airmouse_axis_to_vec3(handle->up_axis, up_axis_vec)) {
        handle->up_axis = IMU_QUAT_AXIS_POS_Z;
    }
    handle->f_stable[0] = handle->f_ref[0];
    handle->f_stable[1] = handle->f_ref[1];
    handle->f_stable[2] = handle->f_ref[2];

    handle->last_sample_us = 0;
    handle->recenter_pending = true;

    const imu_quat_config_t quat_config =
        airmouse_pose_mapping_build_quat_config(handle);

    esp_err_t ret = ESP_OK;
    if (handle->quat_handle == nullptr) {
        ret = imu_quat_create(&quat_config, &handle->quat_handle);
    } else {
        ret = imu_quat_set_config(handle->quat_handle, &quat_config);
        if (ret == ESP_OK) {
            ret = imu_quat_reset_state(handle->quat_handle);
        }
    }

    if (ret != ESP_OK) {
        return ret;
    }

    ret = airmouse_pose_mapping_apply_startup_gyro_bias(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    airmouse_pose_mapping_clear_cursor_history(handle);
    return ESP_OK;
}

static esp_err_t airmouse_pose_mapping_project_fcur_to_uv(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float f_cur[3],
    float *u_out,
    float *v_out)
{
    if (handle == nullptr || f_cur == nullptr || u_out == nullptr || v_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    float forward_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    float screen_x_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    float screen_y_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    if (!airmouse_axis_to_vec3(handle->heading_world_axis, forward_axis_vec) ||
            !airmouse_axis_to_vec3(handle->screen_x_axis, screen_x_axis_vec) ||
            !airmouse_axis_to_vec3(handle->screen_y_axis, screen_y_axis_vec)) {
        return ESP_ERR_INVALID_STATE;
    }

    float range_x = 0.0f;
    float range_y = 0.0f;
    esp_err_t ret = airmouse_pose_mapping_get_range_rad(&range_x, &range_y);
    if (ret != ESP_OK) {
        return ret;
    }

    const float tan_range_x = tanf(range_x);
    const float tan_range_y = tanf(range_y);
    if (fabsf(tan_range_x) <= 1e-6f || fabsf(tan_range_y) <= 1e-6f) {
        return ESP_ERR_INVALID_STATE;
    }

    const float eps = 1e-4f;
    const float depth = airmouse_vec_dot3(f_cur, forward_axis_vec);
    if (fabsf(depth) <= eps) {
        return ESP_ERR_INVALID_STATE;
    }

    const float side_x = airmouse_vec_dot3(f_cur, screen_x_axis_vec);
    const float side_y = airmouse_vec_dot3(f_cur, screen_y_axis_vec);

    *u_out = (side_x / depth) / tan_range_x;
    *v_out = (side_y / depth) / tan_range_y;
    return ESP_OK;
}

static esp_err_t airmouse_pose_mapping_project_fcur_to_angle_uv(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float f_cur[3],
    float *u_out,
    float *v_out)
{
    if (handle == nullptr || f_cur == nullptr || u_out == nullptr || v_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    float forward_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    float screen_x_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    float screen_y_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    if (!airmouse_axis_to_vec3(handle->heading_world_axis, forward_axis_vec) ||
            !airmouse_axis_to_vec3(handle->screen_x_axis, screen_x_axis_vec) ||
            !airmouse_axis_to_vec3(handle->screen_y_axis, screen_y_axis_vec)) {
        return ESP_ERR_INVALID_STATE;
    }

    float range_x = 0.0f;
    float range_y = 0.0f;
    esp_err_t ret = airmouse_pose_mapping_get_range_rad(&range_x, &range_y);
    if (ret != ESP_OK) {
        return ret;
    }

    const float depth = airmouse_vec_dot3(f_cur, forward_axis_vec);
    const float side_x = airmouse_vec_dot3(f_cur, screen_x_axis_vec);
    const float side_y = airmouse_vec_dot3(f_cur, screen_y_axis_vec);
    const float angle_x = atan2f(side_x, depth);
    const float angle_y = atan2f(side_y, depth);

    *u_out = angle_x / range_x;
    *v_out = angle_y / range_y;
    return ESP_OK;
}

static esp_err_t airmouse_pose_mapping_uv_to_dxdy(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    float u,
    float v,
    int8_t *dx_out,
    int8_t *dy_out)
{
    if (handle == nullptr || dx_out == nullptr || dy_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const float dead_zone_x =
        (float)CONFIG_AIRMOUSE_POSE_DEAD_ZONE_X_MILLI / 1000.0f;
    const float dead_zone_y =
        (float)CONFIG_AIRMOUSE_POSE_DEAD_ZONE_Y_MILLI / 1000.0f;
    if (fabsf(u) < dead_zone_x) {
        u = 0.0f;
    }
    if (fabsf(v) < dead_zone_y) {
        v = 0.0f;
    }

    const float pos_u = u * handle->sensitivity_gain_x;
    const float pos_v = v * handle->sensitivity_gain_y;

    if (!handle->has_last_uv) {
        handle->last_u = pos_u;
        handle->last_v = pos_v;
        handle->has_last_uv = true;
        *dx_out = 0;
        *dy_out = 0;
        return ESP_OK;
    }

    const float du = pos_u - handle->last_u;
    const float dv = pos_v - handle->last_v;
    handle->last_u = pos_u;
    handle->last_v = pos_v;

    float fx = du * AIRMOUSE_POSE_RELATIVE_NORMALIZED_TO_COUNT + handle->rem_x;
    float fy = dv * AIRMOUSE_POSE_RELATIVE_NORMALIZED_TO_COUNT + handle->rem_y;

    if (fx > 127.0f) {
        fx = 127.0f;
    } else if (fx < -127.0f) {
        fx = -127.0f;
    }
    if (fy > 127.0f) {
        fy = 127.0f;
    } else if (fy < -127.0f) {
        fy = -127.0f;
    }

    const int dx = (int)fx;
    const int dy = (int)fy;
    handle->rem_x = fx - (float)dx;
    handle->rem_y = fy - (float)dy;

    *dx_out = (int8_t)dx;
    *dy_out = (int8_t)dy;
    return ESP_OK;
}

static esp_err_t airmouse_pose_mapping_uv_to_absxy(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    float u,
    float v,
    uint16_t *abs_x_out,
    uint16_t *abs_y_out)
{
    if (handle == nullptr || abs_x_out == nullptr || abs_y_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const float dead_zone_x =
        (float)CONFIG_AIRMOUSE_POSE_DEAD_ZONE_X_MILLI / 1000.0f;
    const float dead_zone_y =
        (float)CONFIG_AIRMOUSE_POSE_DEAD_ZONE_Y_MILLI / 1000.0f;
    if (fabsf(u) < dead_zone_x) {
        u = 0.0f;
    }
    if (fabsf(v) < dead_zone_y) {
        v = 0.0f;
    }

    const float scaled_u = u * handle->sensitivity_gain_x;
    const float scaled_v = v * handle->sensitivity_gain_y;
    float norm_x = scaled_u * 0.5f + 0.5f;
    float norm_y = scaled_v * 0.5f + 0.5f;

    if (norm_x < 0.0f) {
        norm_x = 0.0f;
    } else if (norm_x > 1.0f) {
        norm_x = 1.0f;
    }
    if (norm_y < 0.0f) {
        norm_y = 0.0f;
    } else if (norm_y > 1.0f) {
        norm_y = 1.0f;
    }

    *abs_x_out = (uint16_t)(norm_x * 32767.0f);
    *abs_y_out = (uint16_t)(norm_y * 32767.0f);
    return ESP_OK;
}

static inline bool airmouse_pose_mapping_in_spherical_deadzone(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float f_cur[3])
{
    if (handle == nullptr || f_cur == nullptr) {
        return true;
    }

    const float dx = f_cur[0] - handle->f_stable[0];
    const float dy = f_cur[1] - handle->f_stable[1];
    const float dz = f_cur[2] - handle->f_stable[2];
    const float dist2 = dx * dx + dy * dy + dz * dz;
    const float threshold = (float)CONFIG_AIRMOUSE_POSE_SPHERICAL_DEADZONE / 10000.0f;
    return dist2 < threshold * threshold;
}

static bool airmouse_pose_mapping_forward_is_in_front_hemisphere(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float f_cur[3],
    float *depth_out)
{
    if (handle == nullptr || f_cur == nullptr) {
        return false;
    }

    float forward_axis_vec[3] = {0.0f, 0.0f, 0.0f};
    if (!airmouse_axis_to_vec3(handle->heading_world_axis, forward_axis_vec)) {
        return false;
    }

    const float depth = airmouse_vec_dot3(f_cur, forward_axis_vec);
    if (depth_out != nullptr) {
        *depth_out = depth;
    }

    return depth > 0.0f;
}

static esp_err_t airmouse_pose_mapping_update_current_forward_from_sample(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float accel[3],
    const float gyro[3],
    bool mag_valid,
    const float mag[3],
    int64_t now_us,
    float f_cur_out[3],
    bool *has_forward_out)
{
    if (handle == nullptr || accel == nullptr || gyro == nullptr ||
            f_cur_out == nullptr || has_forward_out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    f_cur_out[0] = 0.0f;
    f_cur_out[1] = 0.0f;
    f_cur_out[2] = 0.0f;
    *has_forward_out = false;

    if (handle->quat_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (handle->last_sample_us != 0 &&
            (now_us - handle->last_sample_us) > 100000) {
        handle->has_last_uv = false;
    }
    handle->last_sample_us = now_us;

    imu_quat_sample_t sample = {};
    memcpy(sample.accel, accel, sizeof(sample.accel));
    memcpy(sample.gyro, gyro, sizeof(sample.gyro));
    if (mag_valid && mag != nullptr) {
        sample.mag_valid = true;
        memcpy(sample.mag, mag, sizeof(sample.mag));
    } else {
        sample.mag_valid = false;
    }
    sample.timestamp_us = now_us;

    if (handle->recenter_pending) {
        const esp_err_t ret =
            imu_quat_reinitialize_from_sample(handle->quat_handle, &sample);
        if (ret == ESP_ERR_INVALID_STATE) {
            int64_t log_now_us = esp_timer_get_time();
            if ((log_now_us - handle->last_waiting_update_log_us) >=
                    AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
                ESP_LOGI(TAG,
                         "pose mapping: recenter pending, waiting for next valid IMU sample");
                handle->last_waiting_update_log_us = log_now_us;
            }
            return ESP_OK;
        }
        if (ret != ESP_OK) {
            return ret;
        }

        handle->recenter_pending = false;
        airmouse_pose_mapping_clear_cursor_history(handle);
        airmouse_pose_mapping_reset_stable_forward_from_quaternion(handle);
        ESP_LOGI(TAG, "pose mapping: recentered from next valid IMU sample");
        return ESP_OK;
    }

    imu_quat_output_t quat_out = {};
    esp_err_t ret = imu_quat_update(handle->quat_handle, &sample, &quat_out);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!quat_out.updated) {
        if (quat_out.reinitialized) {
            int64_t log_now_us = esp_timer_get_time();
            if ((log_now_us - handle->last_waiting_update_log_us) >=
                    AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
                ESP_LOGI(TAG,
                         "pose mapping debug: quat runtime reinitialized, waiting for next continuous update");
                handle->last_waiting_update_log_us = log_now_us;
            }
        } else {
            int64_t log_now_us = esp_timer_get_time();
            if ((log_now_us - handle->last_waiting_update_log_us) >=
                    AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
                ESP_LOGI(TAG,
                         "pose mapping debug: quat output not updated yet");
                handle->last_waiting_update_log_us = log_now_us;
            }
        }
        if (quat_out.reinitialized) {
            airmouse_pose_mapping_clear_cursor_history(handle);
        }
        airmouse_pose_mapping_reset_stable_forward_from_quaternion(handle);
        return ESP_OK;
    }

    float f_cur[3] = {0.0f, 0.0f, 0.0f};
    ret = imu_quat_rotate_body_to_world(handle->quat_handle, handle->f_ref, f_cur);
    if (ret != ESP_OK || !airmouse_vec_normalize3(f_cur)) {
        return ESP_OK;
    }

    {
        const int64_t log_now_us = esp_timer_get_time();
        if ((log_now_us - handle->last_forward_log_us) >=
                AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
            // ESP_LOGI(TAG,
            //          "pose mapping: current forward world=[%.5f %.5f %.5f]",
            //          f_cur[0],
            //          f_cur[1],
            //          f_cur[2]);
            handle->last_forward_log_us = log_now_us;
        }
    }

    {
        float forward_depth = 0.0f;
        if (!airmouse_pose_mapping_forward_is_in_front_hemisphere(handle,
                                                                  f_cur,
                                                                  &forward_depth)) {
            const int64_t log_now_us = esp_timer_get_time();
            if ((log_now_us - handle->last_projection_fail_log_us) >=
                    AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
                ESP_LOGI(TAG,
                         "pose mapping: forward vector entered back hemisphere, skip cursor mapping depth=%.5f",
                         forward_depth);
                handle->last_projection_fail_log_us = log_now_us;
            }
            return ESP_OK;
        }
    }

    if (airmouse_pose_mapping_in_spherical_deadzone(handle, f_cur)) {
        return ESP_OK;
    }

    handle->f_stable[0] = f_cur[0];
    handle->f_stable[1] = f_cur[1];
    handle->f_stable[2] = f_cur[2];

    f_cur_out[0] = f_cur[0];
    f_cur_out[1] = f_cur[1];
    f_cur_out[2] = f_cur[2];
    *has_forward_out = true;
    return ESP_OK;
}

static esp_err_t airmouse_pose_mapping_project_forward_to_cursor_output(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float f_cur[3],
    airmouse_pose_mapping_cursor_output_t *out)
{
    if (handle == nullptr || f_cur == nullptr || out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    float u = 0.0f;
    float v = 0.0f;
#if CONFIG_AIRMOUSE_POSE_CURSOR_MAPPING_ANGLE_VECTOR
    esp_err_t ret =
        airmouse_pose_mapping_project_fcur_to_angle_uv(handle, f_cur, &u, &v);
#else
    esp_err_t ret =
        airmouse_pose_mapping_project_fcur_to_uv(handle, f_cur, &u, &v);
#endif
    if (ret != ESP_OK) {
        int64_t log_now_us = esp_timer_get_time();
        if ((log_now_us - handle->last_projection_fail_log_us) >=
                AIRMOUSE_POSE_DEBUG_LOG_THROTTLE_US) {
            float forward_axis_vec[3] = {0.0f, 0.0f, 0.0f};
            float depth = 0.0f;
            if (airmouse_axis_to_vec3(handle->heading_world_axis, forward_axis_vec)) {
                depth = airmouse_vec_dot3(f_cur, forward_axis_vec);
            }
            ESP_LOGI(TAG,
                     "pose mapping debug: projection skipped near singular depth, ret=%s depth=%.5f",
                     esp_err_to_name(ret),
                     depth);
            handle->last_projection_fail_log_us = log_now_us;
        }
        return ESP_OK;
    }

#if CONFIG_AIRMOUSE_POSE_CURSOR_MODE_RELATIVE
    ret = airmouse_pose_mapping_uv_to_dxdy(handle, u, v, &out->dx, &out->dy);
    if (ret != ESP_OK) {
        return ret;
    }
    out->has_relative = true;
    return ESP_OK;
#elif CONFIG_AIRMOUSE_POSE_CURSOR_MODE_ABSOLUTE
    ret = airmouse_pose_mapping_uv_to_absxy(
              handle,
              u,
              v,
              &out->abs_x,
              &out->abs_y);
    if (ret != ESP_OK) {
        return ret;
    }
    out->has_absolute = true;
    return ESP_OK;
#else
    return ESP_ERR_INVALID_STATE;
#endif
}

esp_err_t airmouse_pose_mapping_update_cursor_from_sample(
    airmouse_pose_mapping_quaternion_handle_t *handle,
    const float accel[3],
    const float gyro[3],
    bool mag_valid,
    const float mag[3],
    int64_t now_us,
    airmouse_pose_mapping_cursor_output_t *out)
{
    if (handle == nullptr || accel == nullptr || gyro == nullptr || out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    float f_cur[3] = {0.0f, 0.0f, 0.0f};
    bool has_forward_out = false;
    const esp_err_t ret = airmouse_pose_mapping_update_current_forward_from_sample(
                              handle,
                              accel,
                              gyro,
                              mag_valid,
                              mag,
                              now_us,
                              f_cur,
                              &has_forward_out);
    if (ret != ESP_OK || !has_forward_out) {
        return ret;
    }

    return airmouse_pose_mapping_project_forward_to_cursor_output(
               handle, f_cur, out);
}
