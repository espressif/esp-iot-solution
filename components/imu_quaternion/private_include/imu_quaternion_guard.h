/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "imu_quaternion_priv.h"

/**
 * @brief Run gyro over-limit guard logic for one update step.
 */
esp_err_t imu_quat_handle_gyro_guard(
    imu_quat_handle_t handle,
    const float accel[3],
    const float gyro[3],
    int64_t now_us,
    float dt_sec,
    bool *reinitialized);
