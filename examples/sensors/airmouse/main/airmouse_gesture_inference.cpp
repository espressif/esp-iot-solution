/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "airmouse_gesture_inference.h"

#include <string.h>

#include "esp_log.h"
#include "streaming_inference.h"

static const char *TAG = "AIRMOUSE";

#if CONFIG_AIRMOUSE_GESTURE_INFER_ENABLE
static int64_t airmouse_inference_dispatch_get_v_suppress_until_us(
    airmouse_inference_dispatch_state_t *state)
{
    int64_t until_us = 0;

    if (state == NULL) {
        return 0;
    }

    portENTER_CRITICAL(&state->lock);
    until_us = state->v_label_suppress_until_us;
    portEXIT_CRITICAL(&state->lock);
    return until_us;
}

static int64_t airmouse_inference_dispatch_get_sz_suppress_until_us(
    airmouse_inference_dispatch_state_t *state)
{
    int64_t until_us = 0;

    if (state == NULL) {
        return 0;
    }

    portENTER_CRITICAL(&state->lock);
    until_us = state->sz_label_suppress_until_us;
    portEXIT_CRITICAL(&state->lock);
    return until_us;
}

const char *airmouse_gesture_infer_label_name(size_t index)
{
    const char *label =
        streaming_inference_get_label(index);
    if (label != NULL) {
        return label;
    }

    return "unknown";
}

void airmouse_gesture_infer_state_init(
    airmouse_gesture_infer_state_t *state,
    const airmouse_inference_config_t *config)
{
    if (state == NULL || config == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->enabled = config->enable;
    state->last_index = AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT;

    if (CONFIG_AIRMOUSE_GESTURE_SAMPLE_COUNT != AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH ||
            CONFIG_AIRMOUSE_GESTURE_AXES_PER_SAMPLE != AIRMOUSE_GESTURE_MODEL_INPUT_CHANNELS ||
            CONFIG_AIRMOUSE_GESTURE_LABEL_COUNT != AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT) {
        ESP_LOGW(TAG,
                 "gesture model dims are fixed to samples=%d axes=%d labels=%d; current Kconfig is samples=%d axes=%d labels=%d",
                 AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH,
                 AIRMOUSE_GESTURE_MODEL_INPUT_CHANNELS,
                 AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT,
                 CONFIG_AIRMOUSE_GESTURE_SAMPLE_COUNT,
                 CONFIG_AIRMOUSE_GESTURE_AXES_PER_SAMPLE,
                 CONFIG_AIRMOUSE_GESTURE_LABEL_COUNT);
    }

    if (CONFIG_AIRMOUSE_GESTURE_WINDOW_STEP <= 0 ||
            CONFIG_AIRMOUSE_GESTURE_WINDOW_STEP > AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH) {
        ESP_LOGE(TAG,
                 "gesture control task: invalid window step %d, expected [1, %d]",
                 CONFIG_AIRMOUSE_GESTURE_WINDOW_STEP,
                 AIRMOUSE_GESTURE_MODEL_INPUT_LENGTH);
        state->enabled = false;
    }

    if (CONFIG_AIRMOUSE_GESTURE_RESET_COUNT < CONFIG_AIRMOUSE_GESTURE_CONFIRM_COUNT) {
        ESP_LOGE(TAG,
                 "gesture control task: reset count %d must be >= confirm count %d",
                 CONFIG_AIRMOUSE_GESTURE_RESET_COUNT,
                 CONFIG_AIRMOUSE_GESTURE_CONFIRM_COUNT);
        state->enabled = false;
    }

    if (!state->enabled) {
        ESP_LOGI(TAG, "gesture inference disabled by runtime config");
        return;
    }

    if (config->sample_interval_ms >= CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS &&
            (config->sample_interval_ms % CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS) == 0) {
        state->sample_divider =
            config->sample_interval_ms / CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS;
        ESP_LOGI(TAG,
                 "gesture control sample decimation: imu=%d ms, gesture=%d ms, divider=%d",
                 CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS,
                 config->sample_interval_ms,
                 state->sample_divider);
    } else {
        ESP_LOGE(TAG,
                 "gesture sample interval %d ms must be an integer multiple of imu interval %d ms",
                 config->sample_interval_ms,
                 CONFIG_AIRMOUSE_IMU_SAMPLE_INTERVAL_MS);
        state->enabled = false;
    }
}

void airmouse_gesture_infer_state_reset(airmouse_gesture_infer_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->single_shot_capture_active = false;
    state->single_shot_draining = false;
    state->gesture_sample_index = 0;
    state->single_shot_sample_index = 0;
    state->single_shot_buffer_count = 0;
    state->last_index = AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT;
    state->same_label_count = 0;
    state->label_reported = false;
}

bool airmouse_gesture_infer_build_detector_config(
    const airmouse_inference_config_t *config,
    imu_gesture_inference_config_t *out_config)
{
    const streaming_inference_model_t *model = streaming_inference_get_desc();

    if (config == NULL || out_config == NULL) {
        return false;
    }
    if (model == NULL) {
        return false;
    }
    if (!config->enable) {
        return false;
    }

    memset(out_config, 0, sizeof(*out_config));

    out_config->model = model;
    out_config->window_step = CONFIG_AIRMOUSE_GESTURE_WINDOW_STEP;
    out_config->sample_queue_len = model->input_length;
    out_config->input_source = IMU_GESTURE_INFERENCE_INPUT_GYRO;
    return true;
}

esp_err_t airmouse_gesture_infer_recreate_detector(
    const airmouse_inference_config_t *config,
    airmouse_gesture_infer_state_t *out_state,
    imu_gesture_detector_handle_t *inout_detector,
    imu_gesture_cb_t cb,
    void *user_data,
    bool *out_detector_enabled)
{
    airmouse_gesture_infer_state_t new_state = {};
    imu_gesture_inference_config_t new_detector_config = {};
    imu_gesture_detector_handle_t new_detector = NULL;
    imu_gesture_detector_handle_t old_detector = NULL;
    bool detector_enabled = false;

    if (config == NULL || out_state == NULL || inout_detector == NULL ||
            out_detector_enabled == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    airmouse_gesture_infer_state_init(&new_state, config);
    if (new_state.enabled &&
            airmouse_gesture_infer_build_detector_config(config,
                                                         &new_detector_config)) {
        detector_enabled = true;
    } else {
        new_state.enabled = false;
    }

    if (detector_enabled) {
        esp_err_t ret = imu_gesture_inference_detector_create(
                            &new_detector_config, &new_detector);
        if (ret != ESP_OK) {
            return ret;
        }

        ret = imu_gesture_detector_register_cb(new_detector, cb, user_data);
        if (ret != ESP_OK) {
            (void)imu_gesture_detector_del(new_detector);
            return ret;
        }

        (void)imu_gesture_detector_reset(new_detector);
    }

    old_detector = *inout_detector;
    *inout_detector = new_detector;
    *out_state = new_state;
    *out_detector_enabled = detector_enabled;

    if (old_detector != NULL) {
        (void)imu_gesture_detector_del(old_detector);
    }

    return ESP_OK;
}

airmouse_gesture_infer_action_t airmouse_gesture_infer_handle_detector_event(
    airmouse_gesture_infer_state_t *state,
    imu_gesture_detector_handle_t detector,
    imu_gesture_event_t event)
{
    airmouse_gesture_infer_action_t action = AIRMOUSE_GESTURE_INFER_ACTION_NONE;

    if (state == NULL || !state->enabled) {
        return AIRMOUSE_GESTURE_INFER_ACTION_NONE;
    }

    switch (event) {
    case IMU_GESTURE_EVENT_INFERENCE_RESULT: {
        imu_gesture_inference_result_t result = {};
        airmouse_gesture_infer_action_t label_action =
            AIRMOUSE_GESTURE_INFER_ACTION_NONE;
        if (imu_gesture_inference_detector_get_last_result(detector,
                                                           &result) != ESP_OK) {
            ESP_LOGW(TAG, "failed to read latest gesture inference result");
            return AIRMOUSE_GESTURE_INFER_ACTION_NONE;
        }

        const char *label = airmouse_gesture_infer_label_name(result.label_index);
        // ESP_LOGI(TAG,
        //          "raw inference result: %s (score=%.3f)",
        //          label,
        //          (double)result.score);
        if (label != NULL) {
            if (strcmp(label, "O") == 0) {
                label_action = AIRMOUSE_GESTURE_INFER_ACTION_MEDIA_PLAY_PAUSE;
            } else if (strcmp(label, "S") == 0) {
                label_action = AIRMOUSE_GESTURE_INFER_ACTION_OPEN_TASK_VIEW;
            } else if (strcmp(label, "V") == 0) {
                label_action = AIRMOUSE_GESTURE_INFER_ACTION_ESCAPE;
            } else if (strcmp(label, "Z") == 0) {
                label_action = AIRMOUSE_GESTURE_INFER_ACTION_OPEN_WINDOWS_OSK;
            }
        }
        if (state->single_shot_draining) {
            ESP_LOGI(TAG,
                     "single-shot inference result: %s (score=%.3f)",
                     label,
                     (double)result.score);
            if (result.score >=
                    ((float)CONFIG_AIRMOUSE_GESTURE_SCORE_THRESHOLD_MILLI /
                     1000.0f) &&
                    label_action != AIRMOUSE_GESTURE_INFER_ACTION_NONE) {
                action = label_action;
            }
            break;
        }

        if (result.score <
                ((float)CONFIG_AIRMOUSE_GESTURE_SCORE_THRESHOLD_MILLI / 1000.0f)) {
            break;
        }

        if (state->last_index != result.label_index) {
            state->last_index = result.label_index;
            state->same_label_count = 1;
            state->label_reported = false;
        } else {
            state->same_label_count++;
        }

        if (state->same_label_count == CONFIG_AIRMOUSE_GESTURE_CONFIRM_COUNT &&
                !state->label_reported) {
            ESP_LOGI(TAG,
                     "inference: %s",
                     label);
            action = label_action;
            state->label_reported = true;
        }

        if (state->same_label_count >= CONFIG_AIRMOUSE_GESTURE_RESET_COUNT) {
            state->last_index = AIRMOUSE_GESTURE_MODEL_OUTPUT_COUNT;
            state->same_label_count = 0;
            state->label_reported = false;
        }
        break;
    }
    case IMU_GESTURE_EVENT_INFERENCE_MODEL_FAILED:
        ESP_LOGW(TAG, "gesture inference model failed");
        break;
    default:
        break;
    }

    return action;
}

void airmouse_inference_dispatch_state_init(
    airmouse_inference_dispatch_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->lock = portMUX_INITIALIZER_UNLOCKED;
    state->v_label_suppress_until_us = 0;
    state->sz_label_suppress_until_us = 0;
}

void airmouse_inference_dispatch_state_reset(
    airmouse_inference_dispatch_state_t *state)
{
    if (state == NULL) {
        return;
    }

    portENTER_CRITICAL(&state->lock);
    state->v_label_suppress_until_us = 0;
    state->sz_label_suppress_until_us = 0;
    portEXIT_CRITICAL(&state->lock);
}

bool airmouse_inference_dispatch_action_to_mouse_event(
    airmouse_gesture_infer_action_t action,
    airmouse_inference_dispatch_state_t *state,
    int64_t now_us,
    airmouse_mouse_event_t *out_evt)
{
    if (out_evt == NULL) {
        return false;
    }

    switch (action) {
    case AIRMOUSE_GESTURE_INFER_ACTION_OPEN_TASK_VIEW:
        if (now_us < airmouse_inference_dispatch_get_sz_suppress_until_us(state)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_TASK_VIEW;
        return true;
    case AIRMOUSE_GESTURE_INFER_ACTION_MEDIA_PLAY_PAUSE:
        out_evt->type = AIRMOUSE_MOUSE_EVENT_MEDIA_PLAY_PAUSE;
        return true;
    case AIRMOUSE_GESTURE_INFER_ACTION_ESCAPE:
        if (now_us < airmouse_inference_dispatch_get_v_suppress_until_us(state)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_ESCAPE;
        return true;
    case AIRMOUSE_GESTURE_INFER_ACTION_OPEN_WINDOWS_OSK:
        if (now_us < airmouse_inference_dispatch_get_sz_suppress_until_us(state)) {
            return false;
        }
        out_evt->type = AIRMOUSE_MOUSE_EVENT_WINDOWS_OSK;
        return true;
    default:
        return false;
    }
}

void airmouse_inference_dispatch_note_page_switch(
    airmouse_inference_dispatch_state_t *state,
    airmouse_mouse_event_type_t event_type,
    int64_t now_us,
    int64_t duration_us)
{
    if (state == NULL) {
        return;
    }

    portENTER_CRITICAL(&state->lock);
    switch (event_type) {
    case AIRMOUSE_MOUSE_EVENT_SPACE_LEFT:
    case AIRMOUSE_MOUSE_EVENT_SPACE_RIGHT:
        state->sz_label_suppress_until_us = now_us + duration_us;
        break;
    case AIRMOUSE_MOUSE_EVENT_SPACE_UP:
    case AIRMOUSE_MOUSE_EVENT_SPACE_DOWN:
        state->v_label_suppress_until_us = now_us + duration_us;
        break;
    default:
        break;
    }
    portEXIT_CRITICAL(&state->lock);
}
#endif
