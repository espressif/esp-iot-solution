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

#define IMU_GESTURE_KNOB_DEFAULT_AXIS IMU_GESTURE_AXIS_X
#define IMU_GESTURE_KNOB_DEFAULT_CW_SIGN 1
#define IMU_GESTURE_KNOB_DEFAULT_ATAN2_FIRST_AXIS IMU_GESTURE_AXIS_INVALID
#define IMU_GESTURE_KNOB_DEFAULT_ATAN2_SECOND_AXIS IMU_GESTURE_AXIS_INVALID
#define IMU_GESTURE_KNOB_DEFAULT_TRIGGER_ANGLE_DEG 30.0f
#define IMU_GESTURE_KNOB_DEFAULT_ENTER_DPS 45.0f
#define IMU_GESTURE_KNOB_DEFAULT_NON_AXIS_REJECT_DPS 250.0f
#define IMU_GESTURE_KNOB_DEFAULT_AXIS_RATIO_LIMIT 0.60f
#define IMU_GESTURE_KNOB_DEFAULT_STABLE_DPS 30.0f
#define IMU_GESTURE_KNOB_DEFAULT_CANDIDATE_TIMEOUT_MS 1000U
#define IMU_GESTURE_KNOB_DEFAULT_REPEAT_MS 200U
#define IMU_GESTURE_KNOB_DEFAULT_SAMPLE_QUEUE_LEN 16U

/**
 * @brief Knob detector specific configuration
 *        The configuration controls candidate detection, slow-down confirmation,
 *        and repeat behavior of the knob gesture detector.
 */
typedef struct imu_gesture_knob_config_t {
    imu_gesture_axis_t axis;       /*!< Primary rotation axis */
    int cw_sign;                   /*!< Clockwise sign for the primary gyro axis, must be 1 or -1 */
    imu_gesture_axis_t atan2_first_axis; /*!< First accel axis used in atan2(accel[first], accel[second]) */
    imu_gesture_axis_t atan2_second_axis; /*!< Second accel axis used in atan2(accel[first], accel[second]) */
    float trigger_angle_deg;       /*!< Trigger angle threshold in degree */
    float enter_dps;               /*!< Candidate enter threshold in deg/s */
    float axis_ratio_limit;        /*!< Maximum ratio of non-primary gyro axes to the primary axis */
    float stable_dps;              /*!< Slow-down angular velocity threshold in deg/s */
    float non_axis_reject_dps;     /*!< Reject threshold for non-primary gyro axes in deg/s */
    uint32_t candidate_timeout_ms; /*!< Candidate timeout in milliseconds */
    uint32_t repeat_ms;            /*!< Repeat interval while holding the knob gesture, in milliseconds */
    size_t sample_queue_len;       /*!< Internal detector sample queue length, 0 disables queue allocation */
} imu_gesture_knob_config_t;

#define IMU_GESTURE_KNOB_DEFAULT_CONFIG() { \
    .axis = IMU_GESTURE_KNOB_DEFAULT_AXIS, \
    .cw_sign = IMU_GESTURE_KNOB_DEFAULT_CW_SIGN, \
    .atan2_first_axis = IMU_GESTURE_KNOB_DEFAULT_ATAN2_FIRST_AXIS, \
    .atan2_second_axis = IMU_GESTURE_KNOB_DEFAULT_ATAN2_SECOND_AXIS, \
    .trigger_angle_deg = IMU_GESTURE_KNOB_DEFAULT_TRIGGER_ANGLE_DEG, \
    .enter_dps = IMU_GESTURE_KNOB_DEFAULT_ENTER_DPS, \
    .axis_ratio_limit = IMU_GESTURE_KNOB_DEFAULT_AXIS_RATIO_LIMIT, \
    .stable_dps = IMU_GESTURE_KNOB_DEFAULT_STABLE_DPS, \
    .non_axis_reject_dps = IMU_GESTURE_KNOB_DEFAULT_NON_AXIS_REJECT_DPS, \
    .candidate_timeout_ms = IMU_GESTURE_KNOB_DEFAULT_CANDIDATE_TIMEOUT_MS, \
    .repeat_ms = IMU_GESTURE_KNOB_DEFAULT_REPEAT_MS, \
    .sample_queue_len = IMU_GESTURE_KNOB_DEFAULT_SAMPLE_QUEUE_LEN, \
}

/**
 * @brief Create a knob detector instance
 *
 * @param config: Knob detector configuration, or NULL to use the default config
 * @param detector_handle: Returned detector handle
 *
 * @return
 *      - ESP_OK: Create knob detector successfully
 *      - ESP_ERR_INVALID_ARG: Create knob detector failed because of invalid parameters
 *      - ESP_ERR_NO_MEM: Create knob detector failed because of insufficient memory
 *      - ESP_FAIL: Create knob detector failed because of other error
 */
esp_err_t imu_gesture_knob_detector_create(const imu_gesture_knob_config_t *config, imu_gesture_detector_handle_t *detector_handle);

#ifdef __cplusplus
}
#endif
