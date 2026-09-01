/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "airmouse_config.h"
#include "airmouse_hid.h"
#include "freertos/FreeRTOS.h"
#include "imu_gesture.h"
#include "imu_gesture_knob.h"
#include "imu_gesture_space_switch.h"

#ifdef __cplusplus
extern "C" {
#endif

bool airmouse_gesture_build_knob_detector_config(
    const airmouse_knob_config_t *config,
    imu_gesture_knob_config_t *out_config);
bool airmouse_gesture_build_space_switch_detector_config(
    const airmouse_space_switch_config_t *config,
    imu_gesture_space_switch_config_t *out_config);
esp_err_t airmouse_gesture_recreate_knob_detector(
    const airmouse_knob_config_t *config,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data);
esp_err_t airmouse_gesture_recreate_space_switch_detector(
    const airmouse_space_switch_config_t *config,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data);

typedef struct {
    portMUX_TYPE lock;
    bool event_dispatched;
    bool window_active;
    int64_t window_until_us;
} airmouse_runtime_gesture_state_t;

void airmouse_runtime_gesture_state_init(
    airmouse_runtime_gesture_state_t *state);
void airmouse_runtime_gesture_state_reset(
    airmouse_runtime_gesture_state_t *state);
void airmouse_runtime_gesture_state_open_page_window(
    airmouse_runtime_gesture_state_t *state,
    int64_t now_us,
    uint32_t window_ms);
void airmouse_runtime_gesture_state_mark_event_pending(
    airmouse_runtime_gesture_state_t *state);
bool airmouse_runtime_gesture_state_event_was_dispatched(
    airmouse_runtime_gesture_state_t *state);
bool airmouse_runtime_gesture_build_mouse_event(
    airmouse_runtime_gesture_state_t *state,
    imu_gesture_event_t event,
    int64_t now_us,
    airmouse_mouse_event_t *out_evt);

#ifdef __cplusplus
}
#endif
