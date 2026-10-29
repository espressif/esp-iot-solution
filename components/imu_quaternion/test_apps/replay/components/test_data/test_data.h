/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_QUAT_REPLAY_SAMPLE_PERIOD_US  10000

extern const size_t imu_quaternion_replay_test_data_count;
extern const float imu_quaternion_replay_test_data[][6];

#ifdef __cplusplus
}
#endif
