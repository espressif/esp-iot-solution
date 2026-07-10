/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "esp_log.h"
#include "imu_quaternion_priv.h"

#include "imu_quaternion_math.h"

static const char *TAG = "IMU_QUAT";
static const int64_t IMU_QUAT_MAG_DEBUG_LOG_THROTTLE_US = 1000000;
static int64_t s_last_mag_feedback_log_us = 0;

esp_err_t imu_quat_complementary_update(
    imu_quat_handle_t handle,
    const imu_quat_sample_t *sample,
    float dt_sec)
{
    if (handle == NULL || sample == NULL || dt_sec <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    const float *accel = sample->accel;
    const float *gyro = sample->gyro;

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

    float predicted_up_body[3] = {0.0f, 0.0f, 0.0f};
    imu_quat_rotate_vec_transpose(qw,
                                  qx,
                                  qy,
                                  qz,
                                  handle->pose.up_ref,
                                  predicted_up_body);
    imu_quat_vec_normalize3(predicted_up_body);

    float acc_error[3] = {0.0f, 0.0f, 0.0f};
    bool accel_ok = false;
    if (acc_norm > 0.85f && acc_norm < 1.15f &&
            imu_quat_vec_normalize3(measured_up_body)) {
        const float dot_up =
            imu_quat_vec_dot3(measured_up_body, predicted_up_body);
        const float dot_up_clamped = fminf(1.0f, fmaxf(-1.0f, dot_up));
        const float accel_error_deg =
            acosf(dot_up_clamped) * IMU_QUAT_RAD2DEG;

        if (accel_error_deg <= IMU_QUAT_INTERNAL_ACCEL_REJECTION_DEG ||
                handle->rejection.accel_reject_count >=
                IMU_QUAT_INTERNAL_REJECTION_RECOVERY_COUNT) {
            accel_ok = true;
            handle->rejection.accel_reject_count = 0;
        } else if (handle->rejection.accel_reject_count < UINT16_MAX) {
            handle->rejection.accel_reject_count++;
        }
    } else if (handle->rejection.accel_reject_count < UINT16_MAX) {
        handle->rejection.accel_reject_count++;
    }
    handle->rejection.accel_ignored = !accel_ok;

    if (accel_ok) {
        imu_quat_vec_cross3(measured_up_body, predicted_up_body, acc_error);
    }

    float mag_error[3] = {0.0f, 0.0f, 0.0f};
    const bool mag_available =
        handle->config.mag_input_enabled &&
        handle->pose.mag_ref_valid &&
        sample->mag_valid;
    bool mag_ok = false;
    if (mag_available) {
        float measured_mag_body[3] = {
            sample->mag[0],
            sample->mag[1],
            sample->mag[2],
        };

        if (imu_quat_vec_norm3(measured_mag_body) >
                IMU_QUAT_INTERNAL_MAG_FIELD_MIN &&
                imu_quat_vec_normalize3(measured_mag_body)) {
            float predicted_mag_body[3] = {0.0f, 0.0f, 0.0f};
            imu_quat_rotate_vec_transpose(
                qw, qx, qy, qz,
                handle->pose.mag_ref,
                predicted_mag_body);

            if (imu_quat_vec_normalize3(predicted_mag_body)) {
                float measured_heading_body[3] = {0.0f, 0.0f, 0.0f};
                float predicted_heading_body[3] = {0.0f, 0.0f, 0.0f};

                imu_quat_vec_cross3(predicted_up_body,
                                    measured_mag_body,
                                    measured_heading_body);
                imu_quat_vec_cross3(predicted_up_body,
                                    predicted_mag_body,
                                    predicted_heading_body);

                if (imu_quat_vec_normalize3(measured_heading_body) &&
                        imu_quat_vec_normalize3(predicted_heading_body)) {
                    const float dot_heading =
                        imu_quat_vec_dot3(measured_heading_body,
                                          predicted_heading_body);
                    const float dot_heading_clamped =
                        fminf(1.0f, fmaxf(-1.0f, dot_heading));
                    const float mag_error_deg =
                        acosf(dot_heading_clamped) * IMU_QUAT_RAD2DEG;

                    if (mag_error_deg <= IMU_QUAT_INTERNAL_MAG_REJECTION_DEG ||
                            handle->rejection.mag_reject_count >=
                            IMU_QUAT_INTERNAL_REJECTION_RECOVERY_COUNT) {
                        float raw_mag_error[3] = {0.0f, 0.0f, 0.0f};
                        imu_quat_vec_cross3(measured_heading_body,
                                            predicted_heading_body,
                                            raw_mag_error);

                        const float yaw_component =
                            imu_quat_vec_dot3(raw_mag_error, predicted_up_body);

                        mag_error[0] = yaw_component * predicted_up_body[0];
                        mag_error[1] = yaw_component * predicted_up_body[1];
                        mag_error[2] = yaw_component * predicted_up_body[2];

                        mag_ok = true;
                        handle->rejection.mag_reject_count = 0;

                        const int64_t now_us = sample->timestamp_us;
                        if ((now_us - s_last_mag_feedback_log_us) >=
                                IMU_QUAT_MAG_DEBUG_LOG_THROTTLE_US) {
                            ESP_LOGI(
                                TAG,
                                "mag yaw feedback: h_meas=[%.3f %.3f %.3f] "
                                "h_pred=[%.3f %.3f %.3f] err_deg=%.2f "
                                "yaw_comp=%.5f mag_err=[%.5f %.5f %.5f] "
                                "acc_ignored=%s mag_ignored=false",
                                measured_heading_body[0],
                                measured_heading_body[1],
                                measured_heading_body[2],
                                predicted_heading_body[0],
                                predicted_heading_body[1],
                                predicted_heading_body[2],
                                mag_error_deg,
                                yaw_component,
                                mag_error[0],
                                mag_error[1],
                                mag_error[2],
                                handle->rejection.accel_ignored ? "true" : "false");
                            s_last_mag_feedback_log_us = now_us;
                        }
                    }
                }
            }
        }
    }

    if (mag_available && !mag_ok &&
            handle->rejection.mag_reject_count < UINT16_MAX) {
        handle->rejection.mag_reject_count++;
    }
    handle->rejection.mag_ignored = mag_available && !mag_ok;

    if (handle->runtime_gain.warming_up) {
        handle->runtime_gain.live_gain -=
            handle->runtime_gain.gain_drop_per_sec * dt_sec;
        if (handle->runtime_gain.live_gain <= handle->runtime_gain.steady_gain ||
                handle->runtime_gain.steady_gain <= 0.0f) {
            handle->runtime_gain.live_gain = handle->runtime_gain.steady_gain;
            handle->runtime_gain.warming_up = false;
        }
    }

    const float accel_gain = handle->runtime_gain.live_gain;
    const float mag_gain = IMU_QUAT_INTERNAL_MAG_FEEDBACK_GAIN;
    const float gx_corr =
        gx + accel_gain * acc_error[0] + mag_gain * mag_error[0];
    const float gy_corr =
        gy + accel_gain * acc_error[1] + mag_gain * mag_error[1];
    const float gz_corr =
        gz + accel_gain * acc_error[2] + mag_gain * mag_error[2];

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
