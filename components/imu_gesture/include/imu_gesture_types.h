/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle of an IMU gesture detector instance
 */
typedef struct imu_gesture_detector_t *imu_gesture_detector_handle_t;

/**
 * @brief One IMU sample consumed by gesture detectors
 *
 * - accel uses standard gravity unit g
 * - gyro uses degree per second (deg/s)
 */
typedef struct {
    float accel[3];
    float gyro[3];
    int64_t timestamp_us;
} imu_gesture_sample_t;

/**
 * @brief IMU axis selector
 */
typedef enum {
    IMU_GESTURE_AXIS_INVALID = -1,
    IMU_GESTURE_AXIS_X = 0,
    IMU_GESTURE_AXIS_Y,
    IMU_GESTURE_AXIS_Z,
} imu_gesture_axis_t;

/**
 * @brief Runtime gesture event reported by a detector
 */
typedef enum {
    IMU_GESTURE_EVENT_NONE = 0,

    IMU_GESTURE_EVENT_SPACE_LEFT,
    IMU_GESTURE_EVENT_SPACE_RIGHT,
    IMU_GESTURE_EVENT_SPACE_UP,
    IMU_GESTURE_EVENT_SPACE_DOWN,

    IMU_GESTURE_EVENT_KNOB_CW,
    IMU_GESTURE_EVENT_KNOB_CCW,

    IMU_GESTURE_EVENT_INFERENCE_RESULT,
    IMU_GESTURE_EVENT_INFERENCE_MODEL_FAILED,

    IMU_GESTURE_EVENT_MAX,
} imu_gesture_event_t;

/**
 * @brief Detector event callback
 *
 * @param detector: Gesture detector handle that emits the event
 * @param event: Gesture event value
 * @param user_data: User context registered with the callback
 */
typedef void (*imu_gesture_cb_t)(imu_gesture_detector_handle_t detector, imu_gesture_event_t event, void *user_data);

#ifdef __cplusplus
}
#endif
