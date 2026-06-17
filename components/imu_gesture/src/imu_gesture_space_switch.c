/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "imu_gesture_space_switch.h"
#include "imu_gesture_private.h"

typedef struct {
    imu_gesture_detector_t base;

    imu_gesture_space_switch_config_t config;

    bool armed;
    bool release_warned;
    int64_t last_trigger_us;
    int64_t last_release_us;
    imu_gesture_axis_t active_axis;
} imu_gesture_space_switch_obj_t;

static bool imu_gesture_is_space_switch_config_valid(
    const imu_gesture_space_switch_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    if (config->horizontal_axis < IMU_GESTURE_AXIS_X ||
            config->horizontal_axis > IMU_GESTURE_AXIS_Z ||
            config->vertical_axis < IMU_GESTURE_AXIS_X ||
            config->vertical_axis > IMU_GESTURE_AXIS_Z) {
        return false;
    }

    if ((config->left_sign != 1 && config->left_sign != -1) ||
            (config->up_sign != 1 && config->up_sign != -1)) {
        return false;
    }

    if (config->horizontal_axis == config->vertical_axis ||
            config->axis_ratio_limit < 0.0f) {
        return false;
    }

    if (!isfinite(config->trigger_dps) ||
            !isfinite(config->release_dps)) {
        return false;
    }

    return true;
}

static void imu_gesture_space_switch_reset_obj(
    imu_gesture_space_switch_obj_t *space)
{
    space->armed = true;
    space->release_warned = false;
    space->last_trigger_us = 0;
    space->last_release_us = 0;
    space->active_axis = space->config.horizontal_axis;
}

static esp_err_t imu_gesture_space_switch_reset_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_SPACE_SWITCH) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_space_switch_obj_t *space =
        __containerof(detector, imu_gesture_space_switch_obj_t, base);
    imu_gesture_space_switch_reset_obj(space);
    return ESP_OK;
}

static void imu_gesture_space_switch_destroy_detector(
    imu_gesture_detector_t *detector)
{
    if (detector == NULL) {
        return;
    }

    imu_gesture_space_switch_obj_t *space =
        __containerof(detector, imu_gesture_space_switch_obj_t, base);
    free(space);
}

static esp_err_t imu_gesture_space_switch_process_detector(
    imu_gesture_detector_t *detector,
    const imu_gesture_sample_t *sample,
    imu_gesture_event_t *out_event)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_SPACE_SWITCH ||
            sample == NULL || out_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_space_switch_obj_t *space =
        __containerof(detector, imu_gesture_space_switch_obj_t, base);

    // Derive the dominant motion axis and the thresholds for this frame.
    const int64_t now_us = sample->timestamp_us;
    const imu_gesture_axis_t horizontal_axis = space->config.horizontal_axis;
    const imu_gesture_axis_t vertical_axis = space->config.vertical_axis;
    const float horizontal_gyro_dps =
        sample->gyro[(int)horizontal_axis];
    const float vertical_gyro_dps =
        sample->gyro[(int)vertical_axis];
    const float horizontal_abs_dps = fabsf(horizontal_gyro_dps);
    const float vertical_abs_dps = fabsf(vertical_gyro_dps);
    const bool horizontal_is_primary = horizontal_abs_dps >= vertical_abs_dps;
    const imu_gesture_axis_t primary_axis =
        horizontal_is_primary ? horizontal_axis : vertical_axis;
    const imu_gesture_axis_t secondary_axis =
        horizontal_is_primary ? vertical_axis : horizontal_axis;
    const float primary_abs_dps =
        horizontal_is_primary ? horizontal_abs_dps : vertical_abs_dps;
    const float trigger_dps = space->config.trigger_dps;
    float release_dps = space->config.release_dps;
    if (release_dps >= trigger_dps) {
        release_dps = trigger_dps * 0.5f;
        space->release_warned = true;
    }
    const int64_t cooldown_us = (int64_t)space->config.cooldown_ms * 1000LL;

    *out_event = IMU_GESTURE_EVENT_NONE;

    // While disarmed, wait until motion falls below the release threshold,
    // then keep a cooldown window before re-arming.
    if (!space->armed) {
        const float active_axis_abs =
            fabsf(sample->gyro[(int)space->active_axis]);

        if (space->last_release_us == 0) {
            if (active_axis_abs <= release_dps) {
                space->last_release_us = now_us;
            }
            return ESP_OK;
        }

        if ((now_us - space->last_release_us) >= cooldown_us) {
            space->armed = true;
        }
        return ESP_OK;
    }

    // Ignore triggers that arrive during the post-release cooldown gap.
    if (space->last_release_us != 0 &&
            space->last_release_us >= space->last_trigger_us &&
            (now_us - space->last_release_us) < cooldown_us) {
        return ESP_OK;
    }

    // A trigger requires enough speed on the dominant axis and a clean
    // separation from the other axes.
    if (primary_abs_dps < trigger_dps) {
        return ESP_OK;
    }

    const float secondary_abs_dps = fabsf(sample->gyro[(int)secondary_axis]);
    if ((secondary_abs_dps / primary_abs_dps) >=
            space->config.axis_ratio_limit) {
        return ESP_OK;
    }

    for (int i = 0; i < 3; ++i) {
        if (i == (int)primary_axis || i == (int)secondary_axis) {
            continue;
        }

        if ((fabsf(sample->gyro[i]) / primary_abs_dps) >=
                space->config.axis_ratio_limit) {
            return ESP_OK;
        }
    }

    // Latch the trigger state before mapping the dominant axis to a direction.
    space->armed = false;
    space->last_trigger_us = now_us;
    space->last_release_us = 0;
    space->active_axis = primary_axis;

    if (horizontal_is_primary) {
        *out_event =
            horizontal_gyro_dps * space->config.left_sign >= 0.0f
            ? IMU_GESTURE_EVENT_SPACE_LEFT
            : IMU_GESTURE_EVENT_SPACE_RIGHT;
        return ESP_OK;
    }

    *out_event =
        vertical_gyro_dps * space->config.up_sign >= 0.0f
        ? IMU_GESTURE_EVENT_SPACE_UP
        : IMU_GESTURE_EVENT_SPACE_DOWN;
    return ESP_OK;
}

static esp_err_t imu_gesture_space_switch_process_pending_internal(
    imu_gesture_detector_t *detector,
    size_t *out_processed)
{
    if (detector == NULL ||
            detector->type != IMU_GESTURE_DETECTOR_TYPE_SPACE_SWITCH ||
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
            imu_gesture_space_switch_process_detector(detector, &sample, &event);
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

esp_err_t imu_gesture_space_switch_detector_create(
    const imu_gesture_space_switch_config_t *config,
    imu_gesture_detector_handle_t *out_detector)
{
    imu_gesture_space_switch_config_t effective_config =
        (imu_gesture_space_switch_config_t)IMU_GESTURE_SPACE_SWITCH_DEFAULT_CONFIG();

    if (config != NULL) {
        effective_config = *config;
    }

    if (out_detector == NULL ||
            !imu_gesture_is_space_switch_config_valid(&effective_config)) {
        return ESP_ERR_INVALID_ARG;
    }

    imu_gesture_space_switch_obj_t *space = calloc(1, sizeof(*space));
    if (space == NULL) {
        return ESP_ERR_NO_MEM;
    }

    space->config = effective_config;
    imu_gesture_space_switch_reset_obj(space);

    space->base.type = IMU_GESTURE_DETECTOR_TYPE_SPACE_SWITCH;
    space->base.process_pending = imu_gesture_space_switch_process_pending_internal;
    space->base.reset = imu_gesture_space_switch_reset_detector;
    space->base.destroy = imu_gesture_space_switch_destroy_detector;
    if (effective_config.sample_queue_len > 0) {
        space->base.sample_queue = xQueueCreate(
                                       effective_config.sample_queue_len,
                                       sizeof(imu_gesture_sample_t));
        if (space->base.sample_queue == NULL) {
            free(space);
            return ESP_ERR_NO_MEM;
        }
    }

    *out_detector = &space->base;
    return ESP_OK;
}
