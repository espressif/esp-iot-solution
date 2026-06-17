/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "imu_gesture.h"

/**
 * @brief Internal detector implementation type
 */
typedef enum {
    IMU_GESTURE_DETECTOR_TYPE_KNOB = 0,    /*!< Knob detector implementation */
    IMU_GESTURE_DETECTOR_TYPE_SPACE_SWITCH, /*!< Space-switch detector implementation */
    IMU_GESTURE_DETECTOR_TYPE_INFERENCE,    /*!< Inference detector implementation */
} imu_gesture_detector_type_t;

typedef struct imu_gesture_detector_t imu_gesture_detector_t;

/**
 * @brief Process all queued samples currently owned by a detector
 *
 * @param detector Internal detector object
 * @param processed Returned number of processed samples, can be NULL
 *
 * @return
 *      - ESP_OK: Pending samples processed successfully
 *      - ESP_FAIL: Processing failed
 *      - ESP_ERR_INVALID_ARG: Input parameters are invalid
 *      - ESP_ERR_INVALID_STATE: Detector runtime state is unavailable
 */
typedef esp_err_t (*imu_gesture_detector_process_pending_fn)(imu_gesture_detector_t *detector, size_t *processed);

/**
 * @brief Reset detector-private runtime state
 *
 * @param detector Internal detector object
 *
 * @return
 *      - ESP_OK: Detector reset successfully
 *      - ESP_FAIL: Reset failed
 *      - ESP_ERR_INVALID_ARG: Input parameters are invalid
 */
typedef esp_err_t (*imu_gesture_detector_reset_fn)(imu_gesture_detector_t *detector);

/**
 * @brief Destroy a detector-private object
 *
 * @param detector Internal detector object
 */
typedef void (*imu_gesture_detector_destroy_fn)(imu_gesture_detector_t *detector);

/**
 * @brief Shared base object embedded by all detector implementations
 */
struct imu_gesture_detector_t {
    imu_gesture_detector_type_t type;                        /*!< Concrete detector implementation type */
    imu_gesture_detector_process_pending_fn process_pending; /*!< Implementation-specific queue consumer */
    imu_gesture_detector_reset_fn reset;                     /*!< Implementation-specific reset hook */
    imu_gesture_detector_destroy_fn destroy;                 /*!< Implementation-specific destroy hook */
    QueueHandle_t sample_queue;                              /*!< Internal queue that stores pending IMU samples */
    imu_gesture_cb_t cb;                                     /*!< Application callback registered on this detector */
    void *cb_user_data;                                      /*!< Application context passed back to @p cb */
};
