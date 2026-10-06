/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "airmouse_config.h"
#include "airmouse_hid.h"
#include "freertos/FreeRTOS.h"
#include "imu_gesture.h"
#include "imu_gesture_inference.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float accel[3];
    float gyr[3];
#if CONFIG_AIRMOUSE_ENABLE_BMM350
    bool mag_valid;
    float mag[3];
#endif
    int64_t timestamp_us;
} airmouse_imu_sample_t;

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
enum {
    AIRMOUSE_GESTURE_MODEL_INPUT_CHANNELS = 3,
    AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH = 150,
    AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT = 5,
};

typedef struct {
    bool enabled;
    volatile bool single_shot_capture_active;
    volatile bool single_shot_draining;
    int sample_divider;
    int gesture_sample_index;
    int single_shot_sample_index;
    size_t single_shot_buffer_count;
    imu_gesture_sample_t
    single_shot_buffer[AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH];
    size_t last_index;
    int same_label_count;
    bool label_reported;
} airmouse_gesture_infer_state_t;

typedef enum {
    AIRMOUSE_GESTURE_INFER_ACTION_NONE = 0,
    AIRMOUSE_GESTURE_INFER_ACTION_REQUEST_RUNTIME_CONFIG,
    AIRMOUSE_GESTURE_INFER_ACTION_OPEN_TASK_VIEW,
    AIRMOUSE_GESTURE_INFER_ACTION_MEDIA_PLAY_PAUSE,
    AIRMOUSE_GESTURE_INFER_ACTION_ESCAPE,
    AIRMOUSE_GESTURE_INFER_ACTION_OPEN_WINDOWS_OSK,
} airmouse_gesture_infer_action_t;

typedef struct airmouse_inference_dispatch_state {
    portMUX_TYPE lock;
    int64_t v_label_suppress_until_us;
    int64_t sz_label_suppress_until_us;
} airmouse_inference_dispatch_state_t;

void airmouse_gesture_infer_state_init(
    airmouse_gesture_infer_state_t *state,
    const airmouse_inference_config_t *config);
void airmouse_gesture_infer_state_reset(airmouse_gesture_infer_state_t *state);
bool airmouse_gesture_infer_build_detector_config(
    const airmouse_inference_config_t *config,
    imu_gesture_inference_config_t *out_config);
esp_err_t airmouse_gesture_infer_recreate_detector(
    const airmouse_inference_config_t *config,
    airmouse_gesture_infer_state_t *out_state,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data,
    bool *out_detector_enabled);
airmouse_gesture_infer_action_t airmouse_gesture_infer_handle_detector_event(
    airmouse_gesture_infer_state_t *state,
    imu_gesture_detector_handle_t detector,
    imu_gesture_event_t event);
const char *airmouse_gesture_infer_label_name(size_t index);
void airmouse_inference_dispatch_state_init(
    airmouse_inference_dispatch_state_t *state);
void airmouse_inference_dispatch_state_reset(
    airmouse_inference_dispatch_state_t *state);
bool airmouse_inference_dispatch_action_to_mouse_event(
    airmouse_gesture_infer_action_t action,
    airmouse_inference_dispatch_state_t *state,
    int64_t now_us,
    airmouse_mouse_event_t *out_evt);
void airmouse_inference_dispatch_note_page_switch(
    airmouse_inference_dispatch_state_t *state,
    airmouse_mouse_event_type_t event_type,
    int64_t now_us,
    int64_t duration_us);
#endif

#ifdef __cplusplus
}
#endif
