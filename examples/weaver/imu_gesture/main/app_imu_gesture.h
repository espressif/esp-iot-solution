/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <esp_err.h>
#include <esp_weaver.h>
#include <esp_weaver_standard_params.h>
#include <esp_weaver_standard_types.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GESTURE_TYPE_TOSS "toss"
#define GESTURE_TYPE_FLIP "flip"
#define GESTURE_TYPE_SHAKE "shake"
#define GESTURE_TYPE_ROTATION "rotation"
#define GESTURE_TYPE_PUSH "push"
#define GESTURE_TYPE_CIRCLE "circle"
#define GESTURE_TYPE_CLAP_SINGLE "clap_single"
#define GESTURE_TYPE_CLAP_DOUBLE "clap_double"
#define GESTURE_TYPE_CLAP_TRIPLE "clap_triple"

/**
 * @brief Gesture event type enumeration
 */
typedef enum {
    GESTURE_EVENT_TOSS = 0,
    GESTURE_EVENT_FLIP,
    GESTURE_EVENT_SHAKE,
    GESTURE_EVENT_ROTATION,
    GESTURE_EVENT_PUSH,
    GESTURE_EVENT_CIRCLE,
    GESTURE_EVENT_CLAP_SINGLE,
    GESTURE_EVENT_CLAP_DOUBLE,
    GESTURE_EVENT_CLAP_TRIPLE,
    GESTURE_EVENT_MAX
} gesture_event_type_t;

/**
 * @brief Orientation type enumeration
 */
typedef enum {
    ORIENTATION_NORMAL = 0,
    ORIENTATION_UPSIDE_DOWN,
    ORIENTATION_LEFT_SIDE,
    ORIENTATION_RIGHT_SIDE,
    ORIENTATION_MAX
} orientation_type_t;

/**
 * @brief Gesture event data structure
 */
typedef struct {
    gesture_event_type_t type;      /**< Gesture event type */
    int confidence;                 /**< Confidence level (0-100) */
    bool has_orientation;           /**< Whether orientation data is included */
    orientation_type_t orientation; /**< Orientation type */
    float x_angle;                  /**< X-axis angle (-180.0 to 180.0) */
    float y_angle;                  /**< Y-axis angle (-180.0 to 180.0) */
    float z_angle;                  /**< Z-axis angle (-180.0 to 180.0) */
} gesture_event_t;

/**
 * @brief Create and configure the IMU gesture device
 *
 * This function creates an IMU gesture sensor device with all parameters
 * including gesture events, orientation, and sensitivity.
 *
 * @return Pointer to the created device, or NULL on failure
 */
esp_weaver_device_t *app_imu_device_create(void);

/**
 * @brief Report gesture event
 *
 * This function updates and notifies all relevant parameters in the
 * device based on the gesture event data. The gesture event parameter
 * is set to true and automatically reset to false after 100ms using
 * a non-blocking timer (creating a pulse effect for automation triggers).
 *
 * @param[in] event Pointer to gesture_event_t structure containing event data
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if event is NULL,
 *         ESP_ERR_INVALID_STATE if device not created
 */
esp_err_t app_gesture_event_report(const gesture_event_t *event);

#ifdef __cplusplus
}
#endif /* __cplusplus */
