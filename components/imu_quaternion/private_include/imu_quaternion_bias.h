/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "imu_quaternion_priv.h"

/**
 * @brief Update internal rest detection and gyro bias state.
 */
esp_err_t imu_quat_update_rest_bias(imu_quat_handle_t handle, const float accel[3], const float gyro[3], float dt_sec);
