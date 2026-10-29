/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "airmouse_bmi270.h"
#if CONFIG_AIRMOUSE_ENABLE_BMM350
#include "airmouse_bmm350.h"
#endif
#include "airmouse_ble.h"
#include "airmouse_btn.h"
#include "airmouse_gesture.h"
#include "airmouse_gesture_inference.h"
#include "airmouse_hid.h"
#include "airmouse_runtime_config.h"
#include "imu_gesture.h"
#include "imu_gesture_knob.h"
#include "imu_gesture_space_switch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>
#include "airmouse_server.h"

#include "airmouse_pose_mapping_quaternion.h"

typedef airmouse_pose_mapping_quaternion_handle_t airmouse_active_pose_handle_t;

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
enum {
    AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_START = BIT0,
    AIRMOUSE_INFERENCE_NOTIFY_SINGLE_SHOT_FINISH = BIT1,
    AIRMOUSE_INFERENCE_NOTIFY_RESET = BIT2,
};
#endif

enum {
    AIRMOUSE_RUNTIME_GESTURE_NOTIFY_OPEN_PAGE_WINDOW = BIT0,
};

enum {
    AIRMOUSE_RUNTIME_CONFIG_NOTIFY_ENTER = BIT0,
};

typedef struct airmouse_function_handle {
    airmouse_bmi270_handle_t *imu_handle;
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    airmouse_bmm350_handle_t *bmm_handle;
#endif
    airmouse_ble_handle_t *ble_handle;
    airmouse_active_pose_handle_t *pose_handle;
    airmouse_btn_handle_t *btn_handle;
    imu_gesture_detector_handle_t knob_detector;
    imu_gesture_detector_handle_t space_switch_detector;
#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
    imu_gesture_detector_handle_t inference_detector;
#endif
    SemaphoreHandle_t pose_reset_sem;
    bool pose_reconfigure_pending;
    airmouse_config_t config;
} airmouse_function_handle_t;

QueueHandle_t airmouse_get_imu_queue(void);
QueueHandle_t airmouse_get_mouse_event_queue(void);
TaskHandle_t *airmouse_get_runtime_gesture_task_handle_ref(void);
TaskHandle_t *airmouse_get_inference_task_handle_ref(void);
TaskHandle_t *airmouse_get_runtime_config_task_handle_ref(void);
void airmouse_post_mouse_event(const airmouse_mouse_event_t *evt);
void airmouse_runtime_config_request(void);

esp_err_t airmouse_function_prepare_resources(airmouse_function_handle_t *ctx);
esp_err_t airmouse_function_start(airmouse_function_handle_t *ctx);

#ifdef __cplusplus
}
#endif
