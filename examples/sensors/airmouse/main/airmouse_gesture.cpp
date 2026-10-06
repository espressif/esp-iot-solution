/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_gesture.h"

#include "sdkconfig.h"

static bool airmouse_runtime_gesture_consume_page_window_if_active(
    airmouse_runtime_gesture_state_t *state,
    int64_t now_us)
{
    bool accepted = false;

    if (state == NULL) {
        return false;
    }

    portENTER_CRITICAL(&state->lock);
    if (state->window_active && now_us <= state->window_until_us) {
        accepted = true;
        state->event_dispatched = true;
    }
    state->window_active = false;
    state->window_until_us = 0;
    portEXIT_CRITICAL(&state->lock);
    return accepted;
}

bool airmouse_gesture_build_knob_detector_config(
    const airmouse_knob_config_t *config,
    imu_gesture_knob_config_t *out_config)
{
    if (config == NULL || out_config == NULL) {
        return false;
    }

    out_config->axis = (imu_gesture_axis_t)config->axis;
    out_config->cw_sign = config->cw_sign;
    out_config->atan2_first_axis = (imu_gesture_axis_t)config->atan2_first_axis;
    out_config->atan2_second_axis =
        (imu_gesture_axis_t)config->atan2_second_axis;
    out_config->trigger_angle_deg =
        (float)CONFIG_AIRMOUSE_GESTURE_KNOB_TRIGGER_ANGLE_DEG_MILLI / 1000.0f;
    out_config->enter_dps =
        (float)CONFIG_AIRMOUSE_GESTURE_KNOB_ENTER_DPS_MILLI / 1000.0f;
    out_config->axis_ratio_limit =
        (float)CONFIG_AIRMOUSE_GESTURE_KNOB_AXIS_RATIO_LIMIT_MILLI / 1000.0f;
    out_config->stable_dps = (float)config->hold_angle_milli / 1000.0f;
    out_config->non_axis_reject_dps =
        (float)CONFIG_AIRMOUSE_GESTURE_KNOB_NON_AXIS_REJECT_DPS_MILLI /
        1000.0f;
    out_config->candidate_timeout_ms =
        CONFIG_AIRMOUSE_GESTURE_KNOB_CANDIDATE_TIMEOUT_MS;
    out_config->repeat_ms = CONFIG_AIRMOUSE_GESTURE_KNOB_REPEAT_MS;
    out_config->sample_queue_len = CONFIG_AIRMOUSE_GESTURE_KNOB_SAMPLE_QUEUE_LEN;
    return true;
}

bool airmouse_gesture_build_space_switch_detector_config(
    const airmouse_space_switch_config_t *config,
    imu_gesture_space_switch_config_t *out_config)
{
    if (config == NULL || out_config == NULL) {
        return false;
    }

    out_config->horizontal_axis = (imu_gesture_axis_t)config->horizontal_axis;
    out_config->left_sign = config->left_sign;
    out_config->vertical_axis = (imu_gesture_axis_t)config->vertical_axis;
    out_config->up_sign = config->up_sign;
    out_config->trigger_dps =
        (float)CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_TRIGGER_DPS_MILLI /
        1000.0f;
    out_config->release_dps =
        (float)CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_RELEASE_DPS_MILLI /
        1000.0f;
    out_config->axis_ratio_limit =
        (float)CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_AXIS_RATIO_LIMIT_MILLI /
        1000.0f;
    out_config->cooldown_ms = CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_COOLDOWN_MS;
    out_config->sample_queue_len =
        CONFIG_AIRMOUSE_GESTURE_SPACE_SWITCH_SAMPLE_QUEUE_LEN;
    return true;
}

esp_err_t airmouse_gesture_recreate_knob_detector(
    const airmouse_knob_config_t *config,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data)
{
    imu_gesture_knob_config_t detector_config = {};
    imu_gesture_detector_handle_t new_detector = NULL;
    imu_gesture_detector_handle_t old_detector = NULL;

    if (!airmouse_gesture_build_knob_detector_config(config, &detector_config) ||
            inout_detector == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = imu_gesture_knob_detector_create(&detector_config,
                                                     &new_detector);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = imu_gesture_detector_register_cb(new_detector, cb, user_data);
    if (ret != ESP_OK) {
        (void)imu_gesture_detector_del(new_detector);
        return ret;
    }

    (void)imu_gesture_detector_reset(new_detector);

    old_detector = *inout_detector;
    *inout_detector = new_detector;

    if (old_detector != NULL) {
        (void)imu_gesture_detector_del(old_detector);
    }

    return ESP_OK;
}

esp_err_t airmouse_gesture_recreate_space_switch_detector(
    const airmouse_space_switch_config_t *config,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data)
{
    imu_gesture_space_switch_config_t detector_config = {};
    imu_gesture_detector_handle_t new_detector = NULL;
    imu_gesture_detector_handle_t old_detector = NULL;

    if (!airmouse_gesture_build_space_switch_detector_config(config,
                                                             &detector_config) ||
            inout_detector == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = imu_gesture_space_switch_detector_create(&detector_config,
                                                             &new_detector);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = imu_gesture_detector_register_cb(new_detector, cb, user_data);
    if (ret != ESP_OK) {
        (void)imu_gesture_detector_del(new_detector);
        return ret;
    }

    (void)imu_gesture_detector_reset(new_detector);

    old_detector = *inout_detector;
    *inout_detector = new_detector;

    if (old_detector != NULL) {
        (void)imu_gesture_detector_del(old_detector);
    }

    return ESP_OK;
}

void airmouse_runtime_gesture_state_init(
    airmouse_runtime_gesture_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->lock = portMUX_INITIALIZER_UNLOCKED;
    state->event_dispatched = false;
    state->window_active = false;
    state->window_until_us = 0;
}

void airmouse_runtime_gesture_state_reset(
    airmouse_runtime_gesture_state_t *state)
{
    if (state == NULL) {
        return;
    }

    portENTER_CRITICAL(&state->lock);
    state->event_dispatched = false;
    state->window_active = false;
    state->window_until_us = 0;
    portEXIT_CRITICAL(&state->lock);
}

void airmouse_runtime_gesture_state_open_page_window(
    airmouse_runtime_gesture_state_t *state,
    int64_t now_us,
    uint32_t window_ms)
{
    if (state == NULL) {
        return;
    }

    portENTER_CRITICAL(&state->lock);
    state->window_active = true;
    state->window_until_us = now_us + (int64_t)window_ms * 1000LL;
    portEXIT_CRITICAL(&state->lock);
}

void airmouse_runtime_gesture_state_mark_event_pending(
    airmouse_runtime_gesture_state_t *state)
{
    if (state == NULL) {
        return;
    }

    portENTER_CRITICAL(&state->lock);
    state->event_dispatched = false;
    portEXIT_CRITICAL(&state->lock);
}

bool airmouse_runtime_gesture_state_event_was_dispatched(
    airmouse_runtime_gesture_state_t *state)
{
    bool dispatched = false;

    if (state == NULL) {
        return false;
    }

    portENTER_CRITICAL(&state->lock);
    dispatched = state->event_dispatched;
    portEXIT_CRITICAL(&state->lock);
    return dispatched;
}

bool airmouse_runtime_gesture_build_mouse_event(
    airmouse_runtime_gesture_state_t *state,
    imu_gesture_event_t event,
    int64_t now_us,
    airmouse_mouse_event_t *out_evt)
{
    if (out_evt == NULL) {
        return false;
    }

    switch (event) {
    case IMU_GESTURE_EVENT_SPACE_LEFT:
        if (!airmouse_runtime_gesture_consume_page_window_if_active(state, now_us)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_SPACE_LEFT;
        return true;
    case IMU_GESTURE_EVENT_SPACE_RIGHT:
        if (!airmouse_runtime_gesture_consume_page_window_if_active(state, now_us)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_SPACE_RIGHT;
        return true;
    case IMU_GESTURE_EVENT_SPACE_UP:
        if (!airmouse_runtime_gesture_consume_page_window_if_active(state, now_us)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_SPACE_UP;
        return true;
    case IMU_GESTURE_EVENT_SPACE_DOWN:
        if (!airmouse_runtime_gesture_consume_page_window_if_active(state, now_us)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_SPACE_DOWN;
        return true;
    case IMU_GESTURE_EVENT_KNOB_CW:
        out_evt->type = AIRMOUSE_MOUSE_EVENT_KNOB_CW;
        return true;
    case IMU_GESTURE_EVENT_KNOB_CCW:
        out_evt->type = AIRMOUSE_MOUSE_EVENT_KNOB_CCW;
        return true;
    default:
        return false;
    }
}
