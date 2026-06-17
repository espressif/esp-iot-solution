/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "imu_gesture_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_HORIZONTAL_AXIS IMU_GESTURE_AXIS_Z
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_LEFT_SIGN 1
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_VERTICAL_AXIS IMU_GESTURE_AXIS_Y
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_UP_SIGN 1
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_TRIGGER_DPS 250.0f
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_RELEASE_DPS 80.0f
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_AXIS_RATIO_LIMIT 0.50f
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_COOLDOWN_MS 300U
#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_SAMPLE_QUEUE_LEN 16U

/**
 * @brief Space-switch detector specific configuration
 *        The configuration controls axis mapping, direction signs, trigger
 *        thresholds, and cooldown behavior of the space-switch detector.
 */
typedef struct imu_gesture_space_switch_config_t {
    imu_gesture_axis_t horizontal_axis; /*!< Horizontal gesture axis */
    int left_sign;                      /*!< Left direction sign for the horizontal axis, must be 1 or -1 */
    imu_gesture_axis_t vertical_axis;   /*!< Vertical gesture axis */
    int up_sign;                        /*!< Up direction sign for the vertical axis, must be 1 or -1 */
    float trigger_dps;                  /*!< Trigger threshold in deg/s */
    float release_dps;                  /*!< Release threshold in deg/s */
    float axis_ratio_limit;             /*!< Maximum allowed ratio between non-primary and primary axes */
    uint32_t cooldown_ms;               /*!< Cooldown interval after one switch event, in milliseconds */
    size_t sample_queue_len;            /*!< Internal detector sample queue length, 0 disables queue allocation */
} imu_gesture_space_switch_config_t;

#define IMU_GESTURE_SPACE_SWITCH_DEFAULT_CONFIG() { \
    .horizontal_axis = IMU_GESTURE_SPACE_SWITCH_DEFAULT_HORIZONTAL_AXIS, \
    .left_sign = IMU_GESTURE_SPACE_SWITCH_DEFAULT_LEFT_SIGN, \
    .vertical_axis = IMU_GESTURE_SPACE_SWITCH_DEFAULT_VERTICAL_AXIS, \
    .up_sign = IMU_GESTURE_SPACE_SWITCH_DEFAULT_UP_SIGN, \
    .trigger_dps = IMU_GESTURE_SPACE_SWITCH_DEFAULT_TRIGGER_DPS, \
    .release_dps = IMU_GESTURE_SPACE_SWITCH_DEFAULT_RELEASE_DPS, \
    .axis_ratio_limit = IMU_GESTURE_SPACE_SWITCH_DEFAULT_AXIS_RATIO_LIMIT, \
    .cooldown_ms = IMU_GESTURE_SPACE_SWITCH_DEFAULT_COOLDOWN_MS, \
    .sample_queue_len = IMU_GESTURE_SPACE_SWITCH_DEFAULT_SAMPLE_QUEUE_LEN, \
}

/**
 * @brief Create a space-switch detector instance
 *
 * @param config: Space-switch detector configuration, or NULL to use the default config
 * @param detector_handle: Returned detector handle
 *
 * @return
 *      - ESP_OK: Create space-switch detector successfully
 *      - ESP_ERR_INVALID_ARG: Create space-switch detector failed because of invalid parameters
 *      - ESP_ERR_NO_MEM: Create space-switch detector failed because of insufficient memory
 *      - ESP_FAIL: Create space-switch detector failed because of other error
 */
esp_err_t imu_gesture_space_switch_detector_create(const imu_gesture_space_switch_config_t *config, imu_gesture_detector_handle_t *detector_handle);

#ifdef __cplusplus
}
#endif
