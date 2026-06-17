/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "imu_gesture_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Push one IMU sample into a detector queue
 *
 * @param detector: Gesture detector handle
 * @param sample: IMU sample to enqueue
 *
 * @return
 *      - ESP_OK: Push sample successfully
 *      - ESP_ERR_INVALID_ARG: Push sample failed because of invalid parameters
 *      - ESP_ERR_INVALID_STATE: Push sample failed because detector queue is unavailable
 *      - ESP_FAIL: Push sample failed because of other error
 */
esp_err_t imu_gesture_detector_push_sample(imu_gesture_detector_handle_t detector, const imu_gesture_sample_t *sample);

/**
 * @brief Process all pending samples currently queued in a detector
 *
 * @param detector: Gesture detector handle
 * @param processed: Returned number of processed samples, can be NULL
 *
 * @return
 *      - ESP_OK: Process pending samples successfully
 *      - ESP_ERR_INVALID_ARG: Process pending samples failed because of invalid parameters
 *      - ESP_ERR_INVALID_STATE: Process pending samples failed because detector processing state is unavailable
 *      - ESP_FAIL: Process pending samples failed because of other error
 */
esp_err_t imu_gesture_detector_process_pending(imu_gesture_detector_handle_t detector, size_t *processed);

/**
 * @brief Reset a detector runtime state
 *
 * @param detector: Gesture detector handle
 *
 * @return
 *      - ESP_OK: Reset detector successfully
 *      - ESP_ERR_INVALID_ARG: Reset detector failed because of invalid parameters
 *      - ESP_FAIL: Reset detector failed because of other error
 */
esp_err_t imu_gesture_detector_reset(imu_gesture_detector_handle_t detector);

/**
 * @brief Delete a detector instance
 *
 * @param detector: Gesture detector handle
 *
 * @return
 *      - ESP_OK: Delete detector successfully
 *      - ESP_FAIL: Delete detector failed because of other error
 */
esp_err_t imu_gesture_detector_del(imu_gesture_detector_handle_t detector);

/**
 * @brief Register an event callback for a detector
 *        One detector supports only one callback at a time. Registering a new
 *        callback replaces the previously registered callback and user context.
 *
 * @param detector: Gesture detector handle
 * @param cb: Event callback
 * @param user_data: User context passed to @p cb
 *
 * @return
 *      - ESP_OK: Register callback successfully
 *      - ESP_ERR_INVALID_ARG: Register callback failed because of invalid parameters
 *      - ESP_FAIL: Register callback failed because of other error
 */
esp_err_t imu_gesture_detector_register_cb(imu_gesture_detector_handle_t detector, imu_gesture_cb_t cb, void *user_data);

/**
 * @brief Unregister the event callback of a detector
 *        This clears the single callback currently registered on the detector.
 *
 * @param detector: Gesture detector handle
 *
 * @return
 *      - ESP_OK: Unregister callback successfully
 *      - ESP_ERR_INVALID_ARG: Unregister callback failed because of invalid parameters
 *      - ESP_FAIL: Unregister callback failed because of other error
 */
esp_err_t imu_gesture_detector_unregister_cb(imu_gesture_detector_handle_t detector);

#ifdef __cplusplus
}
#endif
