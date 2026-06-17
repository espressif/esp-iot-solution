/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>

#include "imu_quaternion_math.h"

float imu_quat_vec_dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float imu_quat_vec_norm3(const float v[3])
{
    return sqrtf(imu_quat_vec_dot3(v, v));
}

bool imu_quat_vec_normalize3(float v[3])
{
    const float norm = imu_quat_vec_norm3(v);
    if (norm <= 1e-6f) {
        return false;
    }

    v[0] /= norm;
    v[1] /= norm;
    v[2] /= norm;
    return true;
}

void imu_quat_vec_cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void imu_quat_normalize(float *qw, float *qx, float *qy, float *qz)
{
    const float norm =
        sqrtf((*qw) * (*qw) + (*qx) * (*qx) + (*qy) * (*qy) + (*qz) * (*qz));

    *qw /= norm;
    *qx /= norm;
    *qy /= norm;
    *qz /= norm;
}

void imu_quat_multiply(
    float aw,
    float ax,
    float ay,
    float az,
    float bw,
    float bx,
    float by,
    float bz,
    float *ow,
    float *ox,
    float *oy,
    float *oz)
{
    *ow = aw * bw - ax * bx - ay * by - az * bz;
    *ox = aw * bx + ax * bw + ay * bz - az * by;
    *oy = aw * by - ax * bz + ay * bw + az * bx;
    *oz = aw * bz + ax * by - ay * bx + az * bw;
}

void imu_quat_rotate_vec(
    float qw,
    float qx,
    float qy,
    float qz,
    const float v[3],
    float out[3])
{
    const float r00 = 1.0f - 2.0f * (qy * qy + qz * qz);
    const float r01 = 2.0f * (qx * qy - qw * qz);
    const float r02 = 2.0f * (qx * qz + qw * qy);

    const float r10 = 2.0f * (qx * qy + qw * qz);
    const float r11 = 1.0f - 2.0f * (qx * qx + qz * qz);
    const float r12 = 2.0f * (qy * qz - qw * qx);

    const float r20 = 2.0f * (qx * qz - qw * qy);
    const float r21 = 2.0f * (qy * qz + qw * qx);
    const float r22 = 1.0f - 2.0f * (qx * qx + qy * qy);

    out[0] = r00 * v[0] + r01 * v[1] + r02 * v[2];
    out[1] = r10 * v[0] + r11 * v[1] + r12 * v[2];
    out[2] = r20 * v[0] + r21 * v[1] + r22 * v[2];
}

void imu_quat_rotate_vec_transpose(
    float qw,
    float qx,
    float qy,
    float qz,
    const float v[3],
    float out[3])
{
    const float r00 = 1.0f - 2.0f * (qy * qy + qz * qz);
    const float r01 = 2.0f * (qx * qy - qw * qz);
    const float r02 = 2.0f * (qx * qz + qw * qy);

    const float r10 = 2.0f * (qx * qy + qw * qz);
    const float r11 = 1.0f - 2.0f * (qx * qx + qz * qz);
    const float r12 = 2.0f * (qy * qz - qw * qx);

    const float r20 = 2.0f * (qx * qz - qw * qy);
    const float r21 = 2.0f * (qy * qz + qw * qx);
    const float r22 = 1.0f - 2.0f * (qx * qx + qy * qy);

    out[0] = r00 * v[0] + r10 * v[1] + r20 * v[2];
    out[1] = r01 * v[0] + r11 * v[1] + r21 * v[2];
    out[2] = r02 * v[0] + r12 * v[1] + r22 * v[2];
}

esp_err_t imu_quat_from_rotation_matrix(
    const float rot[3][3],
    float *qw,
    float *qx,
    float *qy,
    float *qz)
{
    if (rot == NULL || qw == NULL || qx == NULL || qy == NULL || qz == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const float trace = rot[0][0] + rot[1][1] + rot[2][2];
    if (trace > 0.0f) {
        const float s = 2.0f * sqrtf(trace + 1.0f);
        if (s <= 1e-12f) {
            return ESP_ERR_INVALID_STATE;
        }
        *qw = 0.25f * s;
        *qx = (rot[2][1] - rot[1][2]) / s;
        *qy = (rot[0][2] - rot[2][0]) / s;
        *qz = (rot[1][0] - rot[0][1]) / s;
    } else if (rot[0][0] > rot[1][1] && rot[0][0] > rot[2][2]) {
        const float s = 2.0f * sqrtf(1.0f + rot[0][0] - rot[1][1] - rot[2][2]);
        if (s <= 1e-12f) {
            return ESP_ERR_INVALID_STATE;
        }
        *qw = (rot[2][1] - rot[1][2]) / s;
        *qx = 0.25f * s;
        *qy = (rot[0][1] + rot[1][0]) / s;
        *qz = (rot[0][2] + rot[2][0]) / s;
    } else if (rot[1][1] > rot[2][2]) {
        const float s = 2.0f * sqrtf(1.0f + rot[1][1] - rot[0][0] - rot[2][2]);
        if (s <= 1e-12f) {
            return ESP_ERR_INVALID_STATE;
        }
        *qw = (rot[0][2] - rot[2][0]) / s;
        *qx = (rot[0][1] + rot[1][0]) / s;
        *qy = 0.25f * s;
        *qz = (rot[1][2] + rot[2][1]) / s;
    } else {
        const float s = 2.0f * sqrtf(1.0f + rot[2][2] - rot[0][0] - rot[1][1]);
        if (s <= 1e-12f) {
            return ESP_ERR_INVALID_STATE;
        }
        *qw = (rot[1][0] - rot[0][1]) / s;
        *qx = (rot[0][2] + rot[2][0]) / s;
        *qy = (rot[1][2] + rot[2][1]) / s;
        *qz = 0.25f * s;
    }

    imu_quat_normalize(qw, qx, qy, qz);
    return ESP_OK;
}
