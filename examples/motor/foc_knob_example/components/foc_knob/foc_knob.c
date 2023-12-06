/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "math.h"
#include "pid_ctrl.h"
#include "foc_knob.h"
#include "foc_knob_default.h"

static const char *TAG = "FOC-Knob";

/* Macro for checking a condition and logging an error */
#define KNOB_CHECK(a, str, action) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        action; \
    }

/* Structure to store knob state */
typedef struct {
    float idle_check_velocity_ewma;
    float current_detent_center;
    float angle_to_detent_center;
    float torque;
    uint32_t last_idle_start;
    bool initialSetupDone;
    pid_ctrl_block_handle_t pid;
} knob_state_t;

/*  Structure to store a list of knob parameters and the number of lists */
typedef struct {
    knob_param_t const **param_lists;
    uint16_t  param_list_num;
    knob_state_t state;
} _knob_indicator_t;

/* Structure for common configuration of knob */
typedef struct _knob_indicator_com_config {
    knob_param_t const **param_lists;
    uint16_t  param_list_num;
} _knob_indicator_com_config_t;

/* Function to create a knob using a common configuration */
static _knob_indicator_t *_knob_indicator_create_com(_knob_indicator_com_config_t *cfg)
{
    KNOB_CHECK(NULL != cfg, "com config can't be NULL!", return NULL);

    _knob_indicator_t *p_knob_indicator = (_knob_indicator_t *)calloc(1, sizeof(_knob_indicator_t));
    KNOB_CHECK(p_knob_indicator != NULL, "calloc indicator memory failed", return NULL);

    p_knob_indicator->param_lists = cfg->param_lists;
    p_knob_indicator->param_list_num = cfg->param_list_num;
    return p_knob_indicator;
}

/* Function to create a knob handle based on a configuration */
knob_handle_t knob_create(const knob_config_t *config)
{
    KNOB_CHECK(NULL != config, "config pointer can't be NULL!", NULL)

    _knob_indicator_com_config_t com_cfg = {0};
    _knob_indicator_t *p_knob = NULL;

    if (config->param_lists == NULL) {
        ESP_LOGW(TAG, "param_lists is null, using default param list");
        com_cfg.param_lists = default_knob_param_lst;
        com_cfg.param_list_num = DEFAULT_PARAM_LIST_NUM;
    } else {
        com_cfg.param_lists = config->param_lists;
        com_cfg.param_list_num = config->param_list_num;
    }

    p_knob = _knob_indicator_create_com(&com_cfg);

    return (knob_handle_t)p_knob;
}

/* Function to get knob parameters for a specific mode */
knob_param_t *knob_get_param(knob_handle_t handle, int mode)
{
    _knob_indicator_t *p_indicator = (_knob_indicator_t *)handle;

    if (!(p_indicator && mode >= 0 && mode < p_indicator->param_list_num)) {
        return NULL;
    }
    return (knob_param_t *)(p_indicator->param_lists[mode]);
}

/* Macro to clamp a value within a specified range */
static float CLAMP(const float value, const float low, const float high)
{
    return value < low ? low : (value > high ? high : value);
}

float knob_start(knob_handle_t handle, int mode, float shaft_velocity, float shaft_angle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Invalid knob handle");
        return 0.0;
    }

    esp_err_t ret;

    knob_param_t *motor_config = knob_get_param(handle, mode);
    _knob_indicator_t *p_indicator = (_knob_indicator_t *)handle;
    knob_state_t *state = &p_indicator->state;

    if (!state->initialSetupDone) {
#if XK_INVERT_ROTATION
        state->current_detent_center = -shaft_angle;
#else
        state->current_detent_center = shaft_angle;
#endif
        state->initialSetupDone = true;
    }

    if (state->pid == NULL) {
        // Initialize the PID controller
        pid_ctrl_config_t pid_config = {
            .init_param = {
                .kp = 5.0f,                              // Proportional gain
                .max_output = 5.0,                       // Maximum output
                .min_output = -5.0,                      // Minimum output
                .cal_type = PID_CAL_TYPE_POSITIONAL,     // Mode of calculation
            },
        };

        if (pid_new_control_block(&pid_config, &state->pid) != ESP_OK) {
            ESP_LOGI(TAG, "Failed to create PID control block");
            return 1;
        }
    }

    // Idle check and center adjustment
    state->idle_check_velocity_ewma = shaft_velocity * IDLE_VELOCITY_EWMA_ALPHA + state->idle_check_velocity_ewma * (1 - IDLE_VELOCITY_EWMA_ALPHA);

    if (fabsf(state->idle_check_velocity_ewma) > IDLE_VELOCITY_RAD_PER_SEC) {
        state->last_idle_start = 0;
    } else {
        if (state->last_idle_start == 0) {
            state->last_idle_start = esp_timer_get_time();
        }
    }

    if (state->last_idle_start > 0 && esp_timer_get_time() - state->last_idle_start > IDLE_CORRECTION_DELAY_MILLIS && fabsf(shaft_angle - state->current_detent_center) < IDLE_CORRECTION_MAX_ANGLE_RAD) {
        state->current_detent_center = shaft_angle * IDLE_CORRECTION_RATE_ALPHA + state->current_detent_center * (1 - IDLE_CORRECTION_RATE_ALPHA);
    }

    // Angle to the center adjustment
#if XK_INVERT_ROTATION
    state->angle_to_detent_center = -shaft_angle - state->current_detent_center;
#else
    state->angle_to_detent_center = shaft_angle - state->current_detent_center;
#endif

    // Snap point calculation
    if (state->angle_to_detent_center > motor_config->position_width_radians * motor_config->snap_point && (motor_config->num_positions <= 0 || motor_config->position > 0)) {
        state->current_detent_center += motor_config->position_width_radians;
        state->angle_to_detent_center -= motor_config->position_width_radians;
        motor_config->position--;
    } else if (state->angle_to_detent_center < -motor_config->position_width_radians * motor_config->snap_point && (motor_config->num_positions <= 0 || motor_config->position < motor_config->num_positions - 1)) {
        state->current_detent_center -= motor_config->position_width_radians;
        state->angle_to_detent_center += motor_config->position_width_radians;
        motor_config->position++;
    }

    // Dead zone adjustment
    float dead_zone_adjustment = CLAMP(
                                     state->angle_to_detent_center,
                                     fmaxf(-motor_config->position_width_radians * DEAD_ZONE_DETENT_PERCENT, -DEAD_ZONE_RAD),
                                     fminf(motor_config->position_width_radians * DEAD_ZONE_DETENT_PERCENT, DEAD_ZONE_RAD));

    // Out of bounds check
    bool out_of_bounds = motor_config->num_positions > 0 &&
                         ((state->angle_to_detent_center > 0 && motor_config->position == 0) || (state->angle_to_detent_center < 0 && motor_config->position == motor_config->num_positions - 1));

    // Update PID parameters
    ret = pid_update_parameters(state->pid, &(pid_ctrl_parameter_t) {
        .max_output = out_of_bounds ? 5.0 : 3.0,
        .kp = out_of_bounds ? motor_config->endstop_strength_unit * 4 : motor_config->detent_strength_unit * 4,
        .min_output = out_of_bounds ? -5.0 : -3.0,
        .cal_type = PID_CAL_TYPE_POSITIONAL,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PID parameters are not updated");
        return 0;
    }

    // Calculate torque
    if (fabsf(shaft_velocity) > 60) {
        state->torque = 0;
    } else {
        float input_error = -state->angle_to_detent_center + dead_zone_adjustment;
        float control_output;
        ret = pid_compute(state->pid, input_error, &control_output);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "PID Computation error");
            return 0;
        }
        state->torque = control_output;
#if XK_INVERT_ROTATION
        state->torque = -state->torque;
#endif
    }
    return (state->torque);
}

esp_err_t knob_delete(knob_handle_t handle)
{
    KNOB_CHECK(NULL != handle, "invalid knob handle", NULL)
    _knob_indicator_t *p_indicator = (_knob_indicator_t *)handle;
    free(p_indicator);
    ESP_LOGI(TAG, "Knob deleted successfully");
    return ESP_OK;
}
