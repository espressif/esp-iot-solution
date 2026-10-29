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
 * @brief Reinitialize the solver from one IMU sample.
 */
esp_err_t imu_quat_reinitialize_from_sample(imu_quat_handle_t handle, const imu_quat_sample_t *sample);
