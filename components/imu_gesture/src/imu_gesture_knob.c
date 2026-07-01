/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "imu_gesture_knob.h"
#include "imu_gesture_private.h"

typedef struct {
    imu_gesture_detector_t base;

    imu_gesture_knob_config_t config;

    bool candidate_active;
    bool hold_active;
    int candidate_dir;
    int64_t candidate_begin_us;
    int64_t last_repeat_us;
    float axis_angle_rad;
} imu_gesture_knob_obj_t;

static void imu_gesture_get_knob_plane_components(
    const float accel[3],
    imu_gesture_axis_t first_axis,
    imu_gesture_axis_t second_axis,
    float *out_first,
    float *out_second)
{
    *out_first = accel[(int)first_axis];
    *out_second = accel[(int)second_axis];
}

static void imu_gesture_knob_resolve_atan2_axes(
    imu_gesture_axis_t primary_axis,
    imu_gesture_axis_t *out_first_axis,
    imu_gesture_axis_t *out_second_axis)
{
    switch (primary_axis) {
    case IMU_GESTURE_AXIS_X:
        *out_first_axis = IMU_GESTURE_AXIS_Y;
        *out_second_axis = IMU_GESTURE_AXIS_Z;
        break;
    case IMU_GESTURE_AXIS_Y:
        *out_first_axis = IMU_GESTURE_AXIS_Z;
        *out_second_axis = IMU_GESTURE_AXIS_X;
        break;
    case IMU_GESTURE_AXIS_Z:
    default:
        *out_first_axis = IMU_GESTURE_AXIS_X;
        *out_second_axis = IMU_GESTURE_AXIS_Y;
        break;
    }
}

static float imu_gesture_compute_knob_axis_angle(
    const float accel[3],
    imu_gesture_axis_t first_axis,
    imu_gesture_axis_t second_axis)
{
    float first = 0.0f;
    float second = 0.0f;

    imu_gesture_get_knob_plane_components(
        accel, first_axis, second_axis, &first, &second);
    return atan2f(first, second);
}

static bool imu_gesture_is_knob_config_valid(
    const imu_gesture_knob_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    if (config->axis < IMU_GESTURE_AXIS_X ||
            config->axis > IMU_GESTURE_AXIS_Z) {
        return false;
    }

    if ((config->atan2_first_axis != IMU_GESTURE_AXIS_INVALID &&
            (config->atan2_first_axis < IMU_GESTURE_AXIS_X ||
             config->atan2_first_axis > IMU_GESTURE_AXIS_Z)) ||
            (config->atan2_second_axis != IMU_GESTURE_AXIS_INVALID &&
             (config->atan2_second_axis < IMU_GESTURE_AXIS_X ||
              config->atan2_second_axis > IMU_GESTURE_AXIS_Z))) {
        return false;
    }

    if (config->atan2_first_axis != IMU_GESTURE_AXIS_INVALID &&
            config->atan2_second_axis != IMU_GESTURE_AXIS_INVALID &&
            config->atan2_first_axis == config->atan2_second_axis) {
        return false;
    }

    if (config->cw_sign != 1 && config->cw_sign != -1) {
        return false;
    }

    if (!isfinite(config->axis_ratio_limit) ||
            !isfinite(config->non_axis_reject_dps) ||
            config->axis_ratio_limit < 0.0f ||
            config->non_axis_reject_dps < 0.0f ||
            config->sample_queue_len == 0) {
        return false;
    }

    if (!isfinite(config->trigger_angle_deg) || config->trigger_angle_deg <= 0.0f ||
            !isfinite(config->enter_dps) || config->enter_dps <= 0.0f ||
            !isfinite(config->stable_dps) || config->stable_dps < 0.0f) {
        return false;
    }

    return true;
}

static void imu_gesture_knob_reset_obj(imu_gesture_knob_obj_t *knob)
{
    knob->candidate_active = false;
    knob->hold_active = false;
    knob->candidate_dir = 0;
    knob->candidate_begin_us = 0;
    knob->last_repeat_us = 0;
    knob->axis_angle_rad = 0.0f;
}

static esp_err_t imu_gesture_knob_reset_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_KNOB) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_knob_obj_t *knob =
        __containerof(detector, imu_gesture_knob_obj_t, base);
    imu_gesture_knob_reset_obj(knob);
    return ESP_OK;
}

static void imu_gesture_knob_destroy_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    imu_gesture_knob_obj_t *knob =
        __containerof(detector, imu_gesture_knob_obj_t, base);
    free(knob);
}

static esp_err_t imu_gesture_knob_process_detector(
    imu_gesture_detector_t *detector,
    const imu_gesture_sample_t *sample,
    imu_gesture_event_t *out_event)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_KNOB ||
            sample == NULL || out_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_knob_obj_t *knob =
        __containerof(detector, imu_gesture_knob_obj_t, base);
    const imu_gesture_knob_config_t *config = &knob->config;
    const imu_gesture_axis_t knob_axis = config->axis;
    const float primary_gyro_dps = sample->gyro[(int)knob_axis];
    const float abs_primary_gyro_dps = fabsf(primary_gyro_dps);
    const int64_t now_us = sample->timestamp_us;

    *out_event = IMU_GESTURE_EVENT_NONE;
    knob->axis_angle_rad =
        imu_gesture_compute_knob_axis_angle(sample->accel,
                                            config->atan2_first_axis,
                                            config->atan2_second_axis);

    // Global rejection before entering any state-specific path.
    for (int i = 0; i < 3; ++i) {
        if (i == (int)knob_axis) {
            continue;
        }

        if (config->non_axis_reject_dps > 0.0f &&
                fabsf(sample->gyro[i]) >= config->non_axis_reject_dps) {
            imu_gesture_knob_reset_obj(knob);
            return ESP_OK;
        }
    }

    const float trigger_angle_rad =
        config->trigger_angle_deg * 3.14159265f / 180.0f;
    const float hold_angle_rad = trigger_angle_rad * 0.75f;
    const int64_t candidate_timeout_us =
        (int64_t)config->candidate_timeout_ms * 1000LL;
    const int64_t repeat_us = (int64_t)config->repeat_ms * 1000LL;

    bool slow_enough_gyro = true;
    bool hold_gyro_ok = true;
    for (int i = 0; i < 3; ++i) {
        const float abs_gyro = fabsf(sample->gyro[i]);
        if (abs_gyro > config->stable_dps) {
            slow_enough_gyro = false;
        }
        if (abs_gyro > (config->stable_dps * 2.0f)) {
            hold_gyro_ok = false;
        }
    }

    // Idle state: wait for a dominant primary-axis rotation to start a candidate.
    if (!knob->candidate_active) {
        if (abs_primary_gyro_dps < config->enter_dps) {
            return ESP_OK;
        }

        if (abs_primary_gyro_dps <= 1e-6f) {
            return ESP_OK;
        }

        for (int i = 0; i < 3; ++i) {
            if (i == (int)knob_axis) {
                continue;
            }

            const float ratio = fabsf(sample->gyro[i]) / abs_primary_gyro_dps;
            if (ratio >= config->axis_ratio_limit) {
                return ESP_OK;
            }
        }

        knob->candidate_active = true;
        knob->hold_active = false;
        knob->candidate_dir =
            primary_gyro_dps * config->cw_sign >= 0.0f ? 1 : -1;
        knob->candidate_begin_us = now_us;
        knob->last_repeat_us = 0;
        return ESP_OK;
    }

    const bool angle_matches_threshold =
        fabsf(knob->axis_angle_rad) >= trigger_angle_rad;
    const bool angle_holds_threshold =
        fabsf(knob->axis_angle_rad) >= hold_angle_rad;

    // Hold state: keep repeating until the posture or stability is lost.
    if (knob->hold_active) {
        if (!hold_gyro_ok || !angle_holds_threshold) {
            imu_gesture_knob_reset_obj(knob);
            return ESP_OK;
        }

        if (knob->last_repeat_us == 0 ||
                (now_us - knob->last_repeat_us) >= repeat_us) {
            knob->last_repeat_us = now_us;
            *out_event = knob->candidate_dir > 0 ? IMU_GESTURE_EVENT_KNOB_CW
                         : IMU_GESTURE_EVENT_KNOB_CCW;
            return ESP_OK;
        }

        return ESP_OK;
    }

    // Candidate state: abort stale candidates before slow-down confirmation.
    if (knob->candidate_begin_us != 0 &&
            (now_us - knob->candidate_begin_us) > candidate_timeout_us) {
        imu_gesture_knob_reset_obj(knob);
        return ESP_OK;
    }

    // Candidate state: once the target angle is reached, allow an immediate
    // promotion into hold as soon as the motion has slowed down enough.
    if (angle_matches_threshold && slow_enough_gyro) {
        knob->hold_active = true;
        knob->candidate_begin_us = 0;
        knob->last_repeat_us = now_us;

        *out_event = knob->candidate_dir > 0 ? IMU_GESTURE_EVENT_KNOB_CW
                     : IMU_GESTURE_EVENT_KNOB_CCW;
        return ESP_OK;
    }
    return ESP_OK;
}

static esp_err_t imu_gesture_knob_process_pending_internal(
    imu_gesture_detector_t *detector,
    size_t *out_processed)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_KNOB ||
            detector->sample_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t processed = 0;

    while (true) {
        imu_gesture_sample_t sample = {};
        if (xQueueReceive(detector->sample_queue, &sample, 0) != pdPASS) {
            break;
        }

        imu_gesture_event_t event = IMU_GESTURE_EVENT_NONE;
        const esp_err_t ret =
            imu_gesture_knob_process_detector(detector, &sample, &event);
        if (ret != ESP_OK) {
            if (out_processed != NULL) {
                *out_processed = processed;
            }
            return ret;
        }

        if (event != IMU_GESTURE_EVENT_NONE && detector->cb != NULL) {
            detector->cb(detector, event, detector->cb_user_data);
        }

        processed++;
    }

    if (out_processed != NULL) {
        *out_processed = processed;
    }

    return ESP_OK;
}

esp_err_t imu_gesture_knob_detector_create(
    const imu_gesture_knob_config_t *config,
    imu_gesture_detector_handle_t *out_detector)
{
    imu_gesture_knob_config_t effective_config =
        (imu_gesture_knob_config_t)IMU_GESTURE_KNOB_DEFAULT_CONFIG();

    if (config != NULL) {
        effective_config = *config;
    }

    if (effective_config.atan2_first_axis == IMU_GESTURE_AXIS_INVALID ||
            effective_config.atan2_second_axis == IMU_GESTURE_AXIS_INVALID) {
        imu_gesture_knob_resolve_atan2_axes(effective_config.axis,
                                            &effective_config.atan2_first_axis,
                                            &effective_config.atan2_second_axis);
    }

    if (out_detector == NULL ||
            !imu_gesture_is_knob_config_valid(&effective_config)) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_knob_obj_t *knob = calloc(1, sizeof(*knob));
    if (knob == NULL) {
        return ESP_ERR_NO_MEM;
    }

    knob->config = effective_config;
    imu_gesture_knob_reset_obj(knob);

    knob->base.type = IMU_GESTURE_DETECTOR_TYPE_KNOB;
    knob->base.process_pending = imu_gesture_knob_process_pending_internal;
    knob->base.reset = imu_gesture_knob_reset_detector;
    knob->base.destroy = imu_gesture_knob_destroy_detector;
    if (effective_config.sample_queue_len > 0) {
        knob->base.sample_queue = xQueueCreate(
                                      effective_config.sample_queue_len,
                                      sizeof(imu_gesture_sample_t));
        if (knob->base.sample_queue == NULL) {
            free(knob);
            return ESP_ERR_NO_MEM;
        }
    }

    *out_detector = &knob->base;
    return ESP_OK;
}
