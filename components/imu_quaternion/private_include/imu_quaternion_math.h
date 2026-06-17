/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * @brief Compute the dot product of two 3D vectors.
 */
float imu_quat_vec_dot3(const float a[3], const float b[3]);

/**
 * @brief Compute the norm of one 3D vector.
 */
float imu_quat_vec_norm3(const float v[3]);

/**
 * @brief Normalize one 3D vector in place.
 */
bool imu_quat_vec_normalize3(float v[3]);

/**
 * @brief Compute the cross product of two 3D vectors.
 */
void imu_quat_vec_cross3(const float a[3], const float b[3], float out[3]);

/**
 * @brief Normalize one quaternion in place.
 */
void imu_quat_normalize(float *qw, float *qx, float *qy, float *qz);

/**
 * @brief Multiply two quaternions.
 */
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
    float *oz);

/**
 * @brief Rotate one vector with a quaternion-derived rotation matrix.
 */
void imu_quat_rotate_vec(float qw, float qx, float qy, float qz, const float v[3], float out[3]);

/**
 * @brief Rotate one vector with the transpose of a quaternion-derived rotation matrix.
 */
void imu_quat_rotate_vec_transpose(float qw, float qx, float qy, float qz, const float v[3], float out[3]);

/**
 * @brief Build one quaternion from one 3x3 rotation matrix.
 */
esp_err_t imu_quat_from_rotation_matrix(const float rot[3][3], float *qw, float *qx, float *qy, float *qz);
