/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "imu_quaternion_priv.h"

/**
 * @brief Fill one public config object with component defaults.
 */
void imu_quat_set_default_config(imu_quat_config_t *config);

/**
 * @brief Reset internal runtime gain state after initialization or reset.
 */
void imu_quat_reset_solver_runtime(imu_quat_handle_t handle);

/**
 * @brief Initialize solver pose by keeping the startup pose as identity.
 */
void imu_quat_set_current_pose_reference(imu_quat_handle_t handle, const float accel[3]);

/**
 * @brief Initialize solver pose from accel and heading constraints.
 */
bool imu_quat_set_accel_heading_alignment_reference(imu_quat_handle_t handle, const float accel[3]);

/**
 * @brief Reinitialize the solver from one IMU sample.
 */
esp_err_t imu_quat_reinitialize_from_sample(imu_quat_handle_t handle, const float accel[3], const float gyro[3], int64_t now_us);
