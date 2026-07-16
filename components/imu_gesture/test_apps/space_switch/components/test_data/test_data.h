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

#define IMU_GESTURE_SPACE_SWITCH_TEST_SAMPLE_PERIOD_US  10000

typedef struct {
    float accel[3];
    float gyro[3];
    int label;
} imu_gesture_space_switch_test_row_t;

extern const size_t imu_gesture_space_switch_test_data_count;
extern const imu_gesture_space_switch_test_row_t imu_gesture_space_switch_test_data[];

#ifdef __cplusplus
}
#endif
