/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_imu_gesture.h"

#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "app_imu_gesture";

/* Pulse reset timer period in microseconds (100ms) */
#define GESTURE_PULSE_RESET_PERIOD_US (100 * 1000)

static esp_weaver_device_t *s_imu_device;

/* Timer for non-blocking pulse reset */
static esp_timer_handle_t s_pulse_reset_timer;
static gesture_event_type_t s_pending_reset_type;

/* Parameter handles for quick access during reporting */
static esp_weaver_param_t *s_type_param;
static esp_weaver_param_t *s_confidence_param;
static esp_weaver_param_t *s_gesture_params[GESTURE_EVENT_MAX];
static esp_weaver_param_t *s_orientation_param;
static esp_weaver_param_t *s_x_param;
static esp_weaver_param_t *s_y_param;
static esp_weaver_param_t *s_z_param;
static esp_weaver_param_t *s_sensitivity_param;

typedef struct {
    const char *type_strings;
    const char *param_name;
    const char *param_type;
} gesture_info_t;

static const gesture_info_t s_gesture_info[] = {
    [GESTURE_EVENT_TOSS] = {GESTURE_TYPE_TOSS, ESP_WEAVER_DEF_GESTURE_TOSS, ESP_WEAVER_PARAM_GESTURE_TOSS},
    [GESTURE_EVENT_FLIP] = {GESTURE_TYPE_FLIP, ESP_WEAVER_DEF_GESTURE_FLIP, ESP_WEAVER_PARAM_GESTURE_FLIP},
    [GESTURE_EVENT_SHAKE] = {GESTURE_TYPE_SHAKE, ESP_WEAVER_DEF_GESTURE_SHAKE, ESP_WEAVER_PARAM_GESTURE_SHAKE},
    [GESTURE_EVENT_ROTATION] = {GESTURE_TYPE_ROTATION, ESP_WEAVER_DEF_GESTURE_ROTATION, ESP_WEAVER_PARAM_GESTURE_ROTATION},
    [GESTURE_EVENT_PUSH] = {GESTURE_TYPE_PUSH, ESP_WEAVER_DEF_GESTURE_PUSH, ESP_WEAVER_PARAM_GESTURE_PUSH},
    [GESTURE_EVENT_CIRCLE] = {GESTURE_TYPE_CIRCLE, ESP_WEAVER_DEF_GESTURE_CIRCLE, ESP_WEAVER_PARAM_GESTURE_CIRCLE},
    [GESTURE_EVENT_CLAP_SINGLE] = {GESTURE_TYPE_CLAP_SINGLE, ESP_WEAVER_DEF_GESTURE_CLAP_SINGLE, ESP_WEAVER_PARAM_GESTURE_CLAP_SINGLE},
    [GESTURE_EVENT_CLAP_DOUBLE] = {GESTURE_TYPE_CLAP_DOUBLE, ESP_WEAVER_DEF_GESTURE_CLAP_DOUBLE, ESP_WEAVER_PARAM_GESTURE_CLAP_DOUBLE},
    [GESTURE_EVENT_CLAP_TRIPLE] = {GESTURE_TYPE_CLAP_TRIPLE, ESP_WEAVER_DEF_GESTURE_CLAP_TRIPLE, ESP_WEAVER_PARAM_GESTURE_CLAP_TRIPLE},
};

/* Orientation type strings indexed by orientation_type_t */
static const char *s_orientation_strings[] = {
    [ORIENTATION_NORMAL] = "normal",
    [ORIENTATION_UPSIDE_DOWN] = "upside_down",
    [ORIENTATION_LEFT_SIDE] = "left_side",
    [ORIENTATION_RIGHT_SIDE] = "right_side",
};

/* Timer callback to reset gesture event parameter to false (creates pulse
 * effect) */
static void gesture_pulse_reset_cb(void *arg)
{
    if (s_pending_reset_type < GESTURE_EVENT_MAX &&
            s_gesture_params[s_pending_reset_type]) {
        /* esp_weaver_param_update_and_report() automatically sends to connected
         * clients */
        esp_weaver_param_update_and_report(s_gesture_params[s_pending_reset_type],
                                           esp_weaver_bool(false));
    }
}

esp_weaver_device_t *app_imu_device_create(void)
{
    s_imu_device = esp_weaver_device_create("IMU Gesture Sensor",
                                            ESP_WEAVER_DEVICE_IMU_GESTURE, NULL);
    if (!s_imu_device) {
        ESP_LOGE(TAG, "Could not create IMU gesture device");
        return NULL;
    }

    s_type_param = esp_weaver_param_create(
                       ESP_WEAVER_DEF_GESTURE_TYPE, ESP_WEAVER_PARAM_GESTURE_TYPE,
                       esp_weaver_str("Idle"), PROP_FLAG_READ);
    if (s_type_param) {
        esp_weaver_param_add_ui_type(s_type_param, ESP_WEAVER_UI_TEXT);
        esp_weaver_device_add_param(s_imu_device, s_type_param);
    }

    s_confidence_param = esp_weaver_param_create(
                             ESP_WEAVER_DEF_GESTURE_CONFIDENCE, ESP_WEAVER_PARAM_GESTURE_CONFIDENCE,
                             esp_weaver_int(0), PROP_FLAG_READ);
    if (s_confidence_param) {
        esp_weaver_param_add_ui_type(s_confidence_param, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(s_confidence_param, esp_weaver_int(0),
                                    esp_weaver_int(100), esp_weaver_int(1));
        esp_weaver_device_add_param(s_imu_device, s_confidence_param);
    }

    for (int i = 0; i < GESTURE_EVENT_MAX; i++) {
        s_gesture_params[i] = esp_weaver_param_create(
                                  s_gesture_info[i].param_name, s_gesture_info[i].param_type,
                                  esp_weaver_bool(false), PROP_FLAG_READ);
        if (s_gesture_params[i]) {
            esp_weaver_param_add_ui_type(s_gesture_params[i], ESP_WEAVER_UI_TOGGLE);
            esp_weaver_device_add_param(s_imu_device, s_gesture_params[i]);
        }
    }

    s_orientation_param = esp_weaver_param_create(
                              ESP_WEAVER_DEF_GESTURE_ORIENTATION, ESP_WEAVER_PARAM_ORIENTATION_CHANGE,
                              esp_weaver_str("normal"), PROP_FLAG_READ);
    if (s_orientation_param) {
        esp_weaver_param_add_ui_type(s_orientation_param, ESP_WEAVER_UI_TEXT);
        esp_weaver_device_add_param(s_imu_device, s_orientation_param);
    }

    s_x_param = esp_weaver_param_create(ESP_WEAVER_DEF_GESTURE_X_ORIENTATION,
                                        ESP_WEAVER_PARAM_ORIENTATION_X,
                                        esp_weaver_float(0.0), PROP_FLAG_READ);
    if (s_x_param) {
        esp_weaver_param_add_ui_type(s_x_param, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(s_x_param, esp_weaver_float(-180.0),
                                    esp_weaver_float(180.0), esp_weaver_float(0.1));
        esp_weaver_device_add_param(s_imu_device, s_x_param);
    }

    s_y_param = esp_weaver_param_create(ESP_WEAVER_DEF_GESTURE_Y_ORIENTATION,
                                        ESP_WEAVER_PARAM_ORIENTATION_Y,
                                        esp_weaver_float(0.0), PROP_FLAG_READ);
    if (s_y_param) {
        esp_weaver_param_add_ui_type(s_y_param, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(s_y_param, esp_weaver_float(-180.0),
                                    esp_weaver_float(180.0), esp_weaver_float(0.1));
        esp_weaver_device_add_param(s_imu_device, s_y_param);
    }

    s_z_param = esp_weaver_param_create(ESP_WEAVER_DEF_GESTURE_Z_ORIENTATION,
                                        ESP_WEAVER_PARAM_ORIENTATION_Z,
                                        esp_weaver_float(0.0), PROP_FLAG_READ);
    if (s_z_param) {
        esp_weaver_param_add_ui_type(s_z_param, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(s_z_param, esp_weaver_float(-180.0),
                                    esp_weaver_float(180.0), esp_weaver_float(0.1));
        esp_weaver_device_add_param(s_imu_device, s_z_param);
    }

    s_sensitivity_param = esp_weaver_param_create(
                              ESP_WEAVER_DEF_GESTURE_SENSITIVITY, ESP_WEAVER_PARAM_SENSITIVITY,
                              esp_weaver_int(50), PROP_FLAG_READ);
    if (s_sensitivity_param) {
        esp_weaver_param_add_ui_type(s_sensitivity_param, ESP_WEAVER_UI_SLIDER);
        esp_weaver_param_add_bounds(s_sensitivity_param, esp_weaver_int(0),
                                    esp_weaver_int(100), esp_weaver_int(1));
        esp_weaver_device_add_param(s_imu_device, s_sensitivity_param);
    }

    const esp_timer_create_args_t pulse_reset_timer_args = {
        .callback = gesture_pulse_reset_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "gesture_pulse_reset"
    };
    esp_err_t err =
        esp_timer_create(&pulse_reset_timer_args, &s_pulse_reset_timer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not create pulse reset timer: %s",
                 esp_err_to_name(err));
    }

    return s_imu_device;
}

esp_err_t app_gesture_event_report(const gesture_event_t *event)
{
    if (!event) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_imu_device) {
        ESP_LOGE(TAG, "IMU device not created");
        return ESP_ERR_INVALID_STATE;
    }

    if (event->type >= GESTURE_EVENT_MAX) {
        ESP_LOGE(TAG, "Invalid gesture type: %d", event->type);
        return ESP_ERR_INVALID_ARG;
    }

    /* Update gesture type parameter (use esp_weaver_param_update for batching) */
    if (s_type_param) {
        esp_weaver_param_update(
            s_type_param,
            esp_weaver_str((char *)s_gesture_info[event->type].type_strings));
    }

    /* Update confidence parameter */
    if (s_confidence_param) {
        esp_weaver_param_update(s_confidence_param,
                                esp_weaver_int(event->confidence));
    }

    /* Update orientation data */
    if (event->has_orientation) {
        if (s_orientation_param && event->orientation < ORIENTATION_MAX) {
            esp_weaver_param_update(
                s_orientation_param,
                esp_weaver_str((char *)s_orientation_strings[event->orientation]));
        }

        /* Always report all three axes when orientation data is included.
         * Axes without detected activity are reported as 0.
         */
        if (s_x_param) {
            esp_weaver_param_update(s_x_param, esp_weaver_float(event->x_angle));
        }

        if (s_y_param) {
            esp_weaver_param_update(s_y_param, esp_weaver_float(event->y_angle));
        }

        if (s_z_param) {
            esp_weaver_param_update(s_z_param, esp_weaver_float(event->z_angle));
        }
    }
    /* Note: If no orientation data, no orientation params are reported.
     * HA will preserve its current state until new data arrives.
     */

    /* Trigger the gesture event using non-blocking timer for pulse effect */
    if (s_gesture_params[event->type]) {
        /* If a different gesture's pulse reset is pending, clear it now before
         * setting the new one */
        if (s_pulse_reset_timer && esp_timer_is_active(s_pulse_reset_timer) &&
                s_pending_reset_type != event->type) {
            esp_timer_stop(s_pulse_reset_timer);
            esp_weaver_param_update(s_gesture_params[s_pending_reset_type],
                                    esp_weaver_bool(false));
        }

        /* Set to true and send all batched updates to HA */
        esp_weaver_param_update_and_report(s_gesture_params[event->type],
                                           esp_weaver_bool(true));

        /* Schedule non-blocking reset to false after 100ms (creates pulse effect
         * for automation triggers) */
        if (s_pulse_reset_timer) {
            esp_timer_stop(s_pulse_reset_timer);
            s_pending_reset_type = event->type;
            esp_timer_start_once(s_pulse_reset_timer, GESTURE_PULSE_RESET_PERIOD_US);
        }
    }

    ESP_LOGI(TAG, "Gesture event reported: %s (confidence: %d%%)",
             s_gesture_info[event->type].type_strings, event->confidence);

    /* Log orientation data */
    if (event->has_orientation && event->orientation < ORIENTATION_MAX) {
        ESP_LOGI(TAG, "Orientation: %s", s_orientation_strings[event->orientation]);
        ESP_LOGI(TAG, "Angles: X = %.1f deg Y = %.1f deg Z = %.1f deg", event->x_angle,
                 event->y_angle, event->z_angle);
    }

    return ESP_OK;
}
