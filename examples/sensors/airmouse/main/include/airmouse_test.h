/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define AIRMOUSE_TEST_MODE_ENABLE 0
#define AIRMOUSE_TEST_SYNC_GPIO   4

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t airmouse_test_init(void);

void airmouse_test_update_pose(float yaw_deg,
                               float pitch_deg);

void airmouse_test_realtime_update_imu(const float gyro[3],
                                       int64_t sample_timestamp_us);

void airmouse_test_realtime_mark_pose(int64_t sample_timestamp_us,
                                      int64_t pose_done_us,
                                      bool has_mouse_event);

void airmouse_test_realtime_mark_hid(int64_t source_timestamp_us,
                                     int64_t hid_us);

#ifdef __cplusplus
}
#endif
