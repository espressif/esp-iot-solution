/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bldc_control.h"
#include "bldc_control_param.h"
#include "bldc_config.h"
#include "bldc_common.h"

static const char *TAG = "bldc_control";

ESP_EVENT_DEFINE_BASE(BLDC_CONTROL_EVENT);

static portMUX_TYPE s_control_lock = portMUX_INITIALIZER_UNLOCKED;
#define BLDC_CONTROL_ENTER_CRITICAL() portENTER_CRITICAL(&s_control_lock)
#define BLDC_CONTROL_EXIT_CRITICAL() portEXIT_CRITICAL(&s_control_lock)

typedef struct {
    gptimer_handle_t gptimer;
    SemaphoreHandle_t xSemaphore;     /*!< semaphore handle */
    control_param_t control_param;
    control_mode_t control_mode;      /*!< control mode */
    speed_mode_t speed_mode;          /*!< speed mode */
    pid_ctrl_block_handle_t pid;
    uint8_t (*change_phase)(void *handle);
    void *change_phase_handle;
    uint8_t (*zero_cross)(void *handle);
    void *zero_cross_handle;
    esp_err_t (*control_operation)(void *handle);
    int delayCnt;                     /*!< Delay count, each plus one is an interrupt function time */
    bool is_running;                  /*!< avoid duplicate start */
    mcpwm_timer_event_callbacks_t *cbs; /*!< dynamically allocated timer callbacks */
} bldc_control_t;

// Table to lookup bldc control event name
static const char *bldc_control_event_name_table[] = {
    "BLDC_CONTROL_START",
    "BLDC_CONTROL_STOP",
    "BLDC_CONTROL_ALIGNMENT",
    "BLDC_CONTROL_CLOSED_LOOP",
    "BLDC_CONTROL_DRAG",
    "BLDC_CONTROL_BLOCKED",
};

static esp_err_t bldc_control_operation(void *handle);

static void bldc_control_dispatch_event(int32_t event_id, const void *event_data, size_t event_data_size)
{
    if (esp_event_post(BLDC_CONTROL_EVENT, event_id, event_data, event_data_size, portMAX_DELAY) != ESP_OK) {
        // please call esp_event_loop_create_default() to create event loop to receive event if this happens
        ESP_LOGE(TAG, "Failed to post bldc_control event: %s", bldc_control_event_name_table[event_id]);
    }
}

static bool IRAM_ATTR bldc_control_gptimer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    bldc_control_t *control = (bldc_control_t *)user_ctx;
    /*!< Determine if xSemaphore exists */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /*!< Floating-point numbers cannot be computed in an interrupt, informing the task to start an arithmetic control */
    xSemaphoreGiveFromISR(control->xSemaphore, &xHigherPriorityTaskWoken);
    return (xHigherPriorityTaskWoken == pdTRUE);
}

static void bldc_control_task(void *args)
{
    bldc_control_t *control = (bldc_control_t *)args;
    while (1) {
        if (xSemaphoreTake(control->xSemaphore, portMAX_DELAY) == pdTRUE) {
            control->control_operation(args);
        }
    }
}

esp_err_t bldc_control_init(bldc_control_handle_t *handle, bldc_control_config_t *config)
{
    BLDC_CHECK(config != NULL, "config is NULL", ESP_ERR_INVALID_ARG);
    bldc_control_t *control = (bldc_control_t *)calloc(1, sizeof(bldc_control_t));
    BLDC_CHECK(control, "calloc failed", ESP_ERR_NO_MEM);
    esp_err_t ret = ESP_OK;

    // Init gptimer
    bldc_gptimer_config_t gptimer_config = {
        .gptimer = &control->gptimer,
        .cbs.on_alarm = &bldc_control_gptimer_cb,
        .user_data = (void *)control,
        .alarm_count_us = ALARM_COUNT_US,
    };
    ret = bldc_gptimer_init(&gptimer_config);
    BLDC_CHECK_GOTO(ret == ESP_OK, "Failed to init gptimer", deinit);

    control->xSemaphore = xSemaphoreCreateBinary();
    BLDC_CHECK_GOTO(control->xSemaphore != NULL, "xSemaphoreCreateBinary failed", deinit);

    if (config->debug_config.if_debug) {
        control->control_operation = config->debug_config.debug_operation;
    } else {
        control->control_operation = &bldc_control_operation;
    }

    xTaskCreate(bldc_control_task, "bldc_control_task", 1024 * 4, control, 10, NULL);

    switch (config->alignment_mode) {
    case ALIGNMENT_COMPARER: {
        ret = bldc_zero_cross_comparer_init(&control->zero_cross_handle, &config->zero_cross_comparer_config, &control->control_param);
        BLDC_CHECK_GOTO(ret == ESP_OK, "bldc_zero_cross_comparer_init failed", deinit);
        control->zero_cross = &bldc_zero_cross_comparer_operation;

        control->cbs = malloc(sizeof(mcpwm_timer_event_callbacks_t));
        if (!control->cbs) {
            ESP_LOGE(TAG, "malloc cbs failed");
            goto deinit;
        }
        memset(control->cbs, 0, sizeof(mcpwm_timer_event_callbacks_t));
        control->cbs->on_full = &read_comparer_on_full;
        config->six_step_config.upper_switch_config.bldc_mcpwm.cbs = control->cbs;
        config->six_step_config.upper_switch_config.bldc_mcpwm.timer_cb_user_data = (void *)control->zero_cross_handle;

        break;
    }
#if CONFIG_SOC_MCPWM_SUPPORTED
    case ALIGNMENT_ADC: {
        ret = bldc_zero_cross_adc_init(&control->zero_cross_handle, &config->zero_cross_adc_config, &control->control_param);
        BLDC_CHECK_GOTO(ret == ESP_OK, "bldc_zero_cross_comparer_init failed", deinit);
        control->zero_cross = &bldc_zero_cross_adc_operation;
        if (config->six_step_config.upper_switch_config.control_type != CONTROL_TYPE_MCPWM) {
            ESP_LOGE(TAG, "error: control type must be mcpwm");
            goto deinit;
        }

        control->cbs = malloc(sizeof(mcpwm_timer_event_callbacks_t));
        if (!control->cbs) {
            ESP_LOGE(TAG, "malloc cbs failed");
            goto deinit;
        }
        memset(control->cbs, 0, sizeof(mcpwm_timer_event_callbacks_t));
        control->cbs->on_full = &read_adc_on_full;
        config->six_step_config.upper_switch_config.bldc_mcpwm.cbs = control->cbs;
        config->six_step_config.upper_switch_config.bldc_mcpwm.timer_cb_user_data = (void *)control->zero_cross_handle;

        break;
    }
#endif
    default:
        ESP_LOGE(TAG, "control_mode error");
        goto deinit;
        break;
    }
    control->control_param.alignment_mode = config->alignment_mode;

    switch (config->control_mode) {
    case BLDC_SIX_STEP:
        ret = bldc_six_step_init(&control->change_phase_handle, &config->six_step_config, &control->control_param);
        BLDC_CHECK_GOTO(ret == ESP_OK, "bldc_six_step_init failed", deinit);
        control->change_phase = &bldc_six_step_operation;
        break;
    default:
        ESP_LOGE(TAG, "control_mode error");
        goto deinit;
        break;
    }
    control->control_mode = config->control_mode;

    if (config->speed_mode == SPEED_CLOSED_LOOP) {
        pid_ctrl_config_t pid_ctrl_config = {
            .init_param = PID_CTRL_PARAMETER_DEFAULT(),
        };
        ret = pid_new_control_block(&pid_ctrl_config, &control->pid);
        if (ret != ESP_OK || control->pid == NULL) {
            ESP_LOGE(TAG, "pid_new_control_block failed: %d", ret);
            ret = (ret != ESP_OK) ? ret : ESP_FAIL;
            goto deinit;
        }
    }

    control->speed_mode = config->speed_mode;

    *handle = (bldc_control_handle_t)control;

    return ESP_OK;
deinit:
    if (control->cbs) {
        free(control->cbs);
    }
    if (control->change_phase_handle) {
        // todo change_phase_deinit
        free(control->change_phase_handle);
    }
    if (control->xSemaphore) {
        vSemaphoreDelete(control->xSemaphore);
    }
    if (control->gptimer) {
        bldc_gptimer_deinit(control->gptimer);
    }
    if (control) {
        free(control);
    }
    return ret;
}

esp_err_t bldc_control_deinit(bldc_control_handle_t *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", ESP_ERR_INVALID_ARG);
    esp_err_t err = ESP_OK;

    switch (control->control_param.alignment_mode) {
    case ALIGNMENT_COMPARER:
        err = bldc_zero_cross_comparer_deinit(control->zero_cross_handle);
        BLDC_CHECK(err == ESP_OK, "bldc_zero_cross_comparer_deinit failed", ESP_FAIL);
        break;
    default:
        break;
    }

    switch (control->control_mode) {
    case BLDC_SIX_STEP:
        err = bldc_six_step_deinit(control->change_phase_handle);
        BLDC_CHECK(err == ESP_OK, "bldc_six_step_deinit failed", ESP_FAIL);
        break;
    default:
        break;
    }

    if (control->cbs) {
        free(control->cbs);
        control->cbs = NULL;
    }
    if (control->xSemaphore) {
        vSemaphoreDelete(control->xSemaphore);
    }
    if (control->gptimer) {
        bldc_gptimer_deinit(control->gptimer);
    }

    free(control);
    return ESP_OK;
}

esp_err_t bldc_control_start(bldc_control_handle_t *handle, uint32_t expect_Speed_rpm)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    if (control->is_running) {
        return bldc_control_set_speed_rpm(handle, (int)expect_Speed_rpm);
    }
    switch (control->control_mode) {
    case BLDC_SIX_STEP:
        bldc_six_step_start(control->change_phase_handle);
        break;
    default:
        break;
    }

    switch (control->control_param.alignment_mode) {
    case ALIGNMENT_COMPARER:
        bldc_zero_cross_comparer_start(control->zero_cross_handle);
        break;
#ifdef CONFIG_SOC_MCPWM_SUPPORTED
    case ALIGNMENT_ADC:
        bldc_zero_cross_adc_start(control->zero_cross_handle);
        break;
#endif
    default:
        break;
    }

    control->control_param.expect_speed_rpm = expect_Speed_rpm;
    control->control_param.charge_time = 0;
    control->control_param.inject_adc_read = 0;
    control->control_param.inject_count = 0;
    control->control_param.adc_bemf_phase = 0;

    if (control->speed_mode == SPEED_CLOSED_LOOP) {
        if (control->pid == NULL) {
            ESP_LOGE(TAG, "PID not initialized in CLOSED_LOOP mode");
            return ESP_ERR_INVALID_STATE;
        }
        pid_reset_ctrl_block(control->pid);
    }

    // Start timer then dispatch START event if success
#if INJECT_ENABLE
    control->control_param.status = INJECT;
#else
    control->control_param.status = ALIGNMENT;
#endif // INJECT_ENABLE
    esp_err_t err = bldc_gptimer_start(control->gptimer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bldc_gptimer_start failed: %d", err);
        return err;
    }
    control->is_running = true;
    bldc_control_dispatch_event(BLDC_CONTROL_START, NULL, 0);

    return ESP_OK;
}

esp_err_t bldc_control_stop(bldc_control_handle_t *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    esp_err_t err = gptimer_stop(control->gptimer);
    BLDC_CHECK(err == ESP_OK, "gptimer_stop failed", ESP_FAIL);

    switch (control->control_mode) {
    case BLDC_SIX_STEP:
        err = bldc_six_step_stop(control->change_phase_handle);
        BLDC_CHECK(err == ESP_OK, "bldc_six_step_stop failed", ESP_FAIL);
        break;
    default:
        break;
    }

    switch (control->control_param.alignment_mode) {
    case ALIGNMENT_COMPARER:
        break;
#if CONFIG_SOC_MCPWM_SUPPORTED
    case ALIGNMENT_ADC:
        break;
#endif
    default:
        break;
    }

    control->is_running = false;
    bldc_control_dispatch_event(BLDC_CONTROL_STOP, NULL, 0);
    return ESP_OK;
}

int bldc_control_get_speed_rpm(bldc_control_handle_t *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", 1);
    BLDC_CONTROL_ENTER_CRITICAL();
    int speed_rpm = control->control_param.speed_rpm;
    BLDC_CONTROL_EXIT_CRITICAL();
    return speed_rpm;
}

esp_err_t bldc_control_set_speed_rpm(bldc_control_handle_t *handle, int speed_rpm)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", ESP_ERR_INVALID_STATE);
    BLDC_CONTROL_ENTER_CRITICAL();
    control->control_param.expect_speed_rpm = speed_rpm;
    BLDC_CONTROL_EXIT_CRITICAL();
    return ESP_OK;
}

dir_enum_t bldc_control_get_dir(bldc_control_handle_t *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", -1);
    BLDC_CONTROL_ENTER_CRITICAL();
    dir_enum_t dir = control->control_param.dir;
    BLDC_CONTROL_EXIT_CRITICAL();
    return dir;
}

esp_err_t bldc_control_set_dir(bldc_control_handle_t *handle, dir_enum_t dir)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", ESP_ERR_INVALID_STATE);
    BLDC_CONTROL_ENTER_CRITICAL();
    control->control_param.dir = dir;
    BLDC_CONTROL_EXIT_CRITICAL();
    return ESP_OK;
}

int bldc_control_get_duty(bldc_control_handle_t *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", -1);
    BLDC_CONTROL_ENTER_CRITICAL();
    uint16_t duty = control->control_param.duty;
    BLDC_CONTROL_EXIT_CRITICAL();
    return duty;
}

esp_err_t bldc_control_set_duty(bldc_control_handle_t *handle, uint16_t duty)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", -1);
    BLDC_CONTROL_ENTER_CRITICAL();
    control->control_param.duty = duty;
    BLDC_CONTROL_EXIT_CRITICAL();
    return ESP_OK;
}

esp_err_t bldc_control_update_pid(bldc_control_handle_t *handle, const pid_ctrl_parameter_t *params)
{
    bldc_control_t *control = (bldc_control_t *)handle;
    BLDC_CHECK(control != NULL, "control is NULL", -1);
    esp_err_t ret = ESP_OK;
    BLDC_CONTROL_ENTER_CRITICAL();
    ret = pid_update_parameters(control->pid, params);
    BLDC_CONTROL_EXIT_CRITICAL();
    BLDC_CHECK(ret == ESP_OK, "pid_update_parameters failed", ESP_FAIL);
    return pid_reset_ctrl_block(control->pid);
}

static esp_err_t bldc_control_operation(void *handle)
{
    bldc_control_t *control = (bldc_control_t *)handle;

    switch (control->control_param.status) {
    case INJECT:
        if (--control->delayCnt <= 0) {
            if (control->change_phase(control->change_phase_handle) == 1) {
                control->control_param.status = ALIGNMENT;
            }
            control->delayCnt = control->control_param.charge_time;
        }
        break;
    case ALIGNMENT:
        bldc_control_dispatch_event(BLDC_CONTROL_ALIGNMENT, NULL, 0);
        control->change_phase(control->change_phase_handle);
        control->control_param.status = DRAG;
        bldc_control_dispatch_event(BLDC_CONTROL_DRAG, NULL, 0);
        /*!< To fix strong dragging to a certain phase, a delay of ALIGNMENTNMS is needed. */
        control->delayCnt = ALIGNMENTNMS;
        break;
    case DRAG:
        if (--control->delayCnt <= 0) {
            control->change_phase(control->change_phase_handle);
            control->delayCnt = control->control_param.drag_time;
        }

        /*!< Determining whether to stabilize the zero crossing point */
        if (control->zero_cross(control->zero_cross_handle) == 1) {
            ESP_LOGI(TAG, "CLOSED_LOOP");
            control->control_param.status = CLOSED_LOOP;
            bldc_control_dispatch_event(BLDC_CONTROL_CLOSED_LOOP, NULL, 0);
            control->delayCnt = control->control_param.filter_delay;
        }

        if (control->control_param.filter_failed_count > 15000) {
            control->control_param.filter_failed_count = 0;
            control->control_param.speed_rpm = 0;
            control->control_param.duty = 0;
            control->control_param.status = BLOCKED;
            bldc_control_dispatch_event(BLDC_CONTROL_BLOCKED, NULL, 0);
        }
        break;
    case CLOSED_LOOP:
        if (control->control_param.phase_cnt != control->control_param.phase_cnt_prev) {
            control->delayCnt--;
        }

        if (control->zero_cross(control->zero_cross_handle) == 1) {
            control->delayCnt = control->control_param.filter_delay;
        }

        if (control->control_param.filter_failed_count > 15000) {
            control->control_param.filter_failed_count = 0;
            control->control_param.speed_rpm = 0;
            control->control_param.duty = 0;

            control->control_param.status = BLOCKED;
            bldc_control_dispatch_event(BLDC_CONTROL_BLOCKED, NULL, 0);
        } else if (control->delayCnt <= 0) {
            control->change_phase(control->change_phase_handle);
        }

        if (control->speed_mode == SPEED_CLOSED_LOOP) {
            float duty = 0;
            int32_t a = control->control_param.expect_speed_rpm - control->control_param.speed_rpm;
            pid_compute(control->pid, (float)(a), &duty);
            control->control_param.duty = abs((int)duty);
        }

        break;
    case BLOCKED:
        ESP_LOGE(TAG, "BLOCKED");
        control->change_phase(control->change_phase_handle);
        control->control_param.status = FAULT;
        break;
    case STOP:
        bldc_control_dispatch_event(BLDC_CONTROL_STOP, NULL, 0);
        break;
    case FAULT:
        break;
    }
    return ESP_OK;
}
