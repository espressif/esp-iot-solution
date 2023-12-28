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
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "foc_knob.h"
#include "foc_knob_default.h"
#include "pid_ctrl.h"
#include "math.h"

static const char *TAG = "FOC_Knob";

#define IDLE_VELOCITY_EWMA_ALPHA  0.001
#define IDLE_VELOCITY_RAD_PER_SEC  0.05
#define IDLE_CORRECTION_DELAY_MILLIS  500
#define IDLE_CORRECTION_MAX_ANGLE_RAD  (5 * PI / 180)
#define IDLE_CORRECTION_RATE_ALPHA  0.0005
#define DEAD_ZONE_DETENT_PERCENT  0.2
#define DEAD_ZONE_RAD  (1 * PI / 180)
#define MAX_VELOCITY_CONTROL          CONFIG_FOC_KNOB_MAX_VELOCITY

#define CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

#define CALL_EVENT_CB(ev)   if(p_knob->cb[ev])p_knob->cb[ev](p_knob, p_knob->usr_data[ev])

/* Structure to store a list of knob parameters and the number of lists */
typedef struct {
    foc_knob_param_t const   **param_lists;
    uint16_t                 param_list_num;
    pid_ctrl_block_handle_t  pid;
    float                    max_torque_out_limit;
    float                    max_torque;
    float                    idle_check_velocity_ewma;
    float                    current_detent_center;
    uint32_t                 last_idle_start;
    int                      current_mode;
    bool                     center_adjusted;
    float                    angle_to_detent_center;
    foc_knob_event_t         event;
    SemaphoreHandle_t        mutex;                           /*!< Mutex to achieve thread-safe */
    int32_t                  *position;                       /*!< Positions for all mode */
    void                     *usr_data[FOC_KNOB_EVENT_MAX];   /*!< User data for event */
    foc_knob_cb_t            cb[FOC_KNOB_EVENT_MAX];          /*!< Event callback */
} foc_knob_t;

/* Function to create a knob handle based on a configuration */
foc_knob_handle_t foc_knob_create(const foc_knob_config_t *config)
{
    ESP_RETURN_ON_FALSE(NULL != config, NULL, TAG, "config pointer can't be NULL!");

    foc_knob_t *p_knob = (foc_knob_t *)calloc(1, sizeof(foc_knob_t));
    ESP_RETURN_ON_FALSE(NULL != p_knob, NULL, TAG, "calloc failed");

    if (config->param_lists == NULL) {
        ESP_LOGI(TAG, "param_lists is null, using default param list");
        p_knob->param_lists = default_foc_knob_param_lst;
        p_knob->param_list_num = DEFAULT_PARAM_LIST_NUM;
    } else {
        p_knob->param_lists = config->param_lists;
        p_knob->param_list_num = config->param_list_num;
    }

    esp_err_t ret = ESP_OK;
    p_knob->position = (int32_t *)calloc(p_knob->param_list_num, sizeof(int32_t));
    ESP_GOTO_ON_FALSE(NULL != p_knob->position, ESP_ERR_NO_MEM, deinit, TAG, "calloc failed");

    for (int i = 0; i < p_knob->param_list_num; i++) {
        p_knob->position[i] = p_knob->param_lists[i]->position;
    }

    p_knob->max_torque_out_limit = config->max_torque_out_limit;
    p_knob->max_torque = config->max_torque;

    const pid_ctrl_config_t pid_ctrl_cfg = {
        .init_param = {
            .max_output = config->max_torque,
            .kp = 0,
            .kd = 0,
            .min_output = -config->max_torque,
            .cal_type = PID_CAL_TYPE_POSITIONAL,
        },
    };
    ret = pid_new_control_block(&pid_ctrl_cfg, &p_knob->pid);
    ESP_GOTO_ON_ERROR(ret, deinit, TAG, "Failed to create PID control block");
    p_knob->mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(p_knob->mutex != NULL, ESP_ERR_INVALID_ARG, deinit, TAG, "create mutex failed");

    return (foc_knob_handle_t)p_knob;
deinit:
    if (p_knob->position) {
        free(p_knob->position);
    }
    if (p_knob) {
        free(p_knob);
    }
    return NULL;
}

esp_err_t foc_knob_change_mode(foc_knob_handle_t handle, uint16_t mode)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    ESP_RETURN_ON_FALSE(mode < p_knob->param_list_num, ESP_ERR_INVALID_ARG, TAG, "mode out of range");
    ESP_RETURN_ON_FALSE(NULL != p_knob->param_lists[mode], ESP_ERR_INVALID_ARG, TAG, "undefined mode");
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    p_knob->current_mode = mode;
    p_knob->center_adjusted = false;
    xSemaphoreGive(p_knob->mutex);
    return ESP_OK;
}

float foc_knob_run(foc_knob_handle_t handle, float shaft_velocity, float shaft_angle)
{
    ESP_RETURN_ON_FALSE(NULL != handle, 0.0, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    const foc_knob_param_t *motor_config = p_knob->param_lists[p_knob->current_mode];
    ESP_RETURN_ON_FALSE(NULL != motor_config, 0.0, TAG, "invalid motor config");
    esp_err_t ret = ESP_OK;
    float torque = 0.0;
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    if (p_knob->center_adjusted == false) {
        p_knob->current_detent_center = shaft_angle;
        p_knob->center_adjusted = true;
    }

    /*!< Idle check and center adjustment */
    p_knob->idle_check_velocity_ewma = shaft_velocity * IDLE_VELOCITY_EWMA_ALPHA + p_knob->idle_check_velocity_ewma * (1 - IDLE_VELOCITY_EWMA_ALPHA);

    if (fabsf(p_knob->idle_check_velocity_ewma) > IDLE_VELOCITY_RAD_PER_SEC) {
        p_knob->last_idle_start = 0;
    } else {
        if (p_knob->last_idle_start == 0) {
            p_knob->last_idle_start = esp_timer_get_time();
        }
    }

    if (p_knob->last_idle_start > 0 && esp_timer_get_time() - p_knob->last_idle_start > IDLE_CORRECTION_DELAY_MILLIS && fabsf(shaft_angle - p_knob->current_detent_center) < IDLE_CORRECTION_MAX_ANGLE_RAD) {
        p_knob->current_detent_center = shaft_angle * IDLE_CORRECTION_RATE_ALPHA + p_knob->current_detent_center * (1 - IDLE_CORRECTION_RATE_ALPHA);
    }

    /*!<  Angle to the center adjustment */
    float angle_to_detent_center = shaft_angle - p_knob->current_detent_center;
    if (p_knob->angle_to_detent_center != angle_to_detent_center) {
        p_knob->angle_to_detent_center = angle_to_detent_center;
        p_knob->event = FOC_KNOB_ANGLE_CHANGE;
        CALL_EVENT_CB(FOC_KNOB_ANGLE_CHANGE);
    }

    /*!< Snap point calculation */
    if (p_knob->angle_to_detent_center > motor_config->position_width_radians * motor_config->snap_point && (motor_config->num_positions <= 0 || p_knob->position[p_knob->current_mode] > 0)) {
        p_knob->current_detent_center += motor_config->position_width_radians;
        p_knob->angle_to_detent_center -= motor_config->position_width_radians;
        p_knob->position[p_knob->current_mode]--;
        p_knob->event = FOC_KNOB_DEC;
        CALL_EVENT_CB(FOC_KNOB_DEC);
        if (p_knob->position[p_knob->current_mode] == 0 && motor_config->num_positions > 0) {
            p_knob->event = FOC_KNOB_L_LIM;
            CALL_EVENT_CB(FOC_KNOB_L_LIM);
        }
    } else if (p_knob->angle_to_detent_center < -motor_config->position_width_radians * motor_config->snap_point && (motor_config->num_positions <= 0 || p_knob->position[p_knob->current_mode] < motor_config->num_positions - 1)) {
        p_knob->current_detent_center -= motor_config->position_width_radians;
        p_knob->angle_to_detent_center += motor_config->position_width_radians;
        p_knob->position[p_knob->current_mode]++;
        p_knob->event = FOC_KNOB_INC;
        CALL_EVENT_CB(FOC_KNOB_INC);
        if (p_knob->position[p_knob->current_mode] == motor_config->num_positions - 1 && motor_config->num_positions > 0) {
            p_knob->event = FOC_KNOB_H_LIM;
            CALL_EVENT_CB(FOC_KNOB_H_LIM);
        }
    }

    /*!< Dead zone adjustment */
    float dead_zone_adjustment = CLAMP(
                                     p_knob->angle_to_detent_center,
                                     fmaxf(-motor_config->position_width_radians * DEAD_ZONE_DETENT_PERCENT, -DEAD_ZONE_RAD),
                                     fminf(motor_config->position_width_radians * DEAD_ZONE_DETENT_PERCENT, DEAD_ZONE_RAD));

    /*!< Out of bounds check */
    bool out_of_bounds = motor_config->num_positions > 0 &&
                         ((p_knob->angle_to_detent_center > 0 && p_knob->position[p_knob->current_mode] == 0) || (p_knob->angle_to_detent_center < 0 && p_knob->position[p_knob->current_mode] == motor_config->num_positions - 1));

    /*!< Update PID parameters */
    ret = pid_update_parameters(p_knob->pid, &(pid_ctrl_parameter_t) {
        .max_output = out_of_bounds ? p_knob->max_torque_out_limit : p_knob->max_torque,
        .kp = out_of_bounds ? motor_config->endstop_strength_unit * 4 : motor_config->detent_strength_unit * 4,
        .kd = 0.01,
        .min_output = out_of_bounds ? -p_knob->max_torque_out_limit : -p_knob->max_torque,
        .cal_type = PID_CAL_TYPE_POSITIONAL,
    });
    ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, fail, TAG, "PID parameters are not updated");

    /*!< Calculate torque */
    if (fabsf(shaft_velocity) > MAX_VELOCITY_CONTROL) {
        torque = 0;
    } else {
        float input_error = -p_knob->angle_to_detent_center + dead_zone_adjustment;
        float control_output;
        ret = pid_compute(p_knob->pid, input_error, &control_output);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_FAIL, fail, TAG, "PID Computation error");
        torque = control_output;
    }
    xSemaphoreGive(p_knob->mutex);
    return (torque);
fail:
    xSemaphoreGive(p_knob->mutex);
    return 0;
}

esp_err_t foc_knob_delete(foc_knob_handle_t handle)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    pid_del_control_block(p_knob->pid);
    xSemaphoreGive(p_knob->mutex);
    vSemaphoreDelete(p_knob->mutex);
    p_knob->mutex = NULL;
    free(p_knob);
    ESP_LOGI(TAG, "Knob deleted successfully");
    return ESP_OK;
}

esp_err_t foc_knob_register_cb(foc_knob_handle_t handle, foc_knob_event_t event, foc_knob_cb_t cb, void *usr_data)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    ESP_RETURN_ON_FALSE(event < FOC_KNOB_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid event");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    p_knob->cb[event] = cb;
    p_knob->usr_data[event] = usr_data;
    xSemaphoreGive(p_knob->mutex);
    return ESP_OK;
}

esp_err_t foc_knob_unregister_cb(foc_knob_handle_t handle, foc_knob_event_t event)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    ESP_RETURN_ON_FALSE(event < FOC_KNOB_EVENT_MAX, ESP_ERR_INVALID_ARG, TAG, "invalid event");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    p_knob->cb[event] = NULL;
    p_knob->usr_data[event] = NULL;
    xSemaphoreGive(p_knob->mutex);
    return ESP_OK;
}

esp_err_t foc_knob_get_state(foc_knob_handle_t handle, foc_knob_state_t *state)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    state->angle_to_detent_center = p_knob->angle_to_detent_center;
    state->position_width_radians = p_knob->param_lists[p_knob->current_mode]->position_width_radians;
    state->num_positions = p_knob->param_lists[p_knob->current_mode]->num_positions;
    state->position = p_knob->position[p_knob->current_mode];
    state->descriptor = p_knob->param_lists[p_knob->current_mode]->descriptor;
    return ESP_OK;
}

esp_err_t foc_knob_get_event(foc_knob_handle_t handle, foc_knob_event_t *event)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    *event = p_knob->event;
    return ESP_OK;
}

esp_err_t foc_knob_set_currect_mode_position(foc_knob_handle_t handle, int32_t position)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    ESP_RETURN_ON_FALSE(position >= 0 && position < p_knob->param_lists[p_knob->current_mode]->num_positions, ESP_ERR_INVALID_ARG, TAG, "invalid position");
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    p_knob->angle_to_detent_center = 0;
    p_knob->center_adjusted = false;
    p_knob->position[p_knob->current_mode] = position;
    xSemaphoreGive(p_knob->mutex);
    return ESP_OK;
}

esp_err_t foc_knob_get_current_mode_position(foc_knob_handle_t handle, int32_t *position)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    *position = p_knob->position[p_knob->current_mode];
    return ESP_OK;
}

esp_err_t foc_knob_set_position(foc_knob_handle_t handle, uint16_t mode, int32_t position)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    ESP_RETURN_ON_FALSE(mode < p_knob->param_list_num, ESP_ERR_INVALID_ARG, TAG, "mode out of range");
    ESP_RETURN_ON_FALSE(NULL != p_knob->param_lists[mode], ESP_ERR_INVALID_ARG, TAG, "undefined mode");
    ESP_RETURN_ON_FALSE(position >= 0 && position < p_knob->param_lists[mode]->num_positions, ESP_ERR_INVALID_ARG, TAG, "invalid position");
    xSemaphoreTake(p_knob->mutex, portMAX_DELAY);
    p_knob->angle_to_detent_center = 0;
    p_knob->center_adjusted = false;
    p_knob->position[mode] = position;
    xSemaphoreGive(p_knob->mutex);
    return ESP_OK;
}

esp_err_t foc_knob_get_position(foc_knob_handle_t handle, uint16_t mode, int32_t *position)
{
    ESP_RETURN_ON_FALSE(NULL != handle, ESP_ERR_INVALID_ARG, TAG, "invalid foc knob handle");
    foc_knob_t *p_knob = (foc_knob_t *)handle;
    ESP_RETURN_ON_FALSE(mode < p_knob->param_list_num, ESP_ERR_INVALID_ARG, TAG, "mode out of range");
    ESP_RETURN_ON_FALSE(NULL != p_knob->param_lists[mode], ESP_ERR_INVALID_ARG, TAG, "undefined mode");
    *position = p_knob->position[mode];
    return ESP_OK;
}
