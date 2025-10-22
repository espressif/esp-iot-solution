/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_control_param.h"
#include "bldc_driver.h"
#include "bldc_six_step.h"
#include "bldc_zero_cross_comparer.h"
#if CONFIG_SOC_MCPWM_SUPPORTED
#include "bldc_zero_cross_adc.h"
#endif
#include "esp_event.h"

/**
 * @brief using esp_event_handler_register() to register BLDC_CONTROL_EVENT
 *
 */
ESP_EVENT_DECLARE_BASE(BLDC_CONTROL_EVENT); /*!< esp event name */

typedef enum {
    BLDC_CONTROL_START = 0,      /*!< BLDC control start event */
    BLDC_CONTROL_ALIGNMENT,      /*!< BLDC control alignment event */
    BLDC_CONTROL_DRAG,           /*!< BLDC control drag event */
    BLDC_CONTROL_STOP,           /*!< BLDC control stop event */
    BLDC_CONTROL_CLOSED_LOOP,    /*!< BLDC control closed loop event */
    BLDC_CONTROL_BLOCKED,        /*!< BLDC control blocked event */
} bldc_control_event_t;

typedef enum {
    SPEED_OPEN_LOOP = 0,  /*!< Open-loop speed control, speed control by setting pwm duty, poor load carrying capacity */
    SPEED_CLOSED_LOOP,    /*!< Closed-loop speed control, rotational speed control via PID, high load carrying capacity */
} speed_mode_t;

typedef enum {
    BLDC_SIX_STEP = 0,   /*!< six-step phase change */
    BLDC_FOC,            /*!< foc phase change, not supported yet */
} control_mode_t;

/**
 * @brief Debug configuration, when activated, will periodically invoke the debug_operation.
 *
 */
typedef struct {
    uint8_t if_debug;                            /*!< set 1 to open debug mode */
    esp_err_t (*debug_operation)(void *handle);  /*!< debug operation */
} bldc_debug_config_t;

/**
 * @brief BLDC Control Configuration
 *
 */
typedef struct {
    speed_mode_t speed_mode;          /*!< Speed Mode */
    control_mode_t control_mode;      /*!< Control Mode */
    alignment_mode_t alignment_mode;  /*!< Alignment Mode */
    union {
        bldc_six_step_config_t six_step_config; /*!< six-step phase change config */
    };
    union {
        bldc_zero_cross_comparer_config_t zero_cross_comparer_config; /*!< Comparator detects zero crossing config */
#if CONFIG_SOC_MCPWM_SUPPORTED
        bldc_zero_cross_adc_config_t zero_cross_adc_config;           /*!< ADC detects zero crossing config */
#endif
    };
    bldc_debug_config_t debug_config; /*!< debug config */
} bldc_control_config_t;

/**
 * @brief bldc control handle
 *
 */
typedef void *bldc_control_handle_t;

/**
 * @brief init bldc control
 *
 * @param handle pointer to bldc control handle
 * @param config pointer to bldc control config
 * @return
 *      ESP_ERR_INVALID_ARG if handle or config is NULL
 *      ESP_ERR_NO_MEM if memory allocation failed
 *      ESP_OK on success
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_control_init(bldc_control_handle_t *handle, bldc_control_config_t *config);

/**
 * @brief deinit bldc control
 *
 * @param handle pointer to bldc control handle
 *      ESP_ERR_INVALID_ARG if handle or config is NULL
 *      ESP_OK on success
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_control_deinit(bldc_control_handle_t *handle);

/**
 * @brief motor start
 *
 * @param handle pointer to bldc control handle
 * @param expect_speed_rpm expect speed in rpm. This parameter does not work in case of open-loop control
 * @return ESP_OK on success
 */
esp_err_t bldc_control_start(bldc_control_handle_t *handle, uint32_t expect_speed_rpm);

/**
 * @brief motor stop
 *
 * @param handle pointer to bldc control handle
 * @return
 *      ESP_FAIL if motor stop failed
 *      ESP_OK on success
 */
esp_err_t bldc_control_stop(bldc_control_handle_t *handle);

/**
 * @brief get current motor direction
 *
 * @param handle pointer to bldc control handle
 * @return dir_enum_t current motor direction
 */
dir_enum_t bldc_control_get_dir(bldc_control_handle_t *handle);

/**
 * @brief set motor direction
 *
 * @param handle pointer to bldc control handle
 * @param dir motor direction
 * @return ESP_OK on success
 */
esp_err_t bldc_control_set_dir(bldc_control_handle_t *handle, dir_enum_t dir);

/**
 * @brief get current motor pwm duty
 *
 * @param handle pointer to bldc control handle
 * @return int current motor pwm duty
 */
int bldc_control_get_duty(bldc_control_handle_t *handle);

/**
 * @brief set motor pwm duty, Closed-loop speed control without calls
 *
 * @param handle pointer to bldc control handle
 * @param duty motor pwm duty
 * @return ESP_OK on success
 */
esp_err_t bldc_control_set_duty(bldc_control_handle_t *handle, uint16_t duty);

/**
 * @brief get current RPM
 *
 * @param handle pointer to bldc control handle
 * @return int current RPM
 */
int bldc_control_get_speed_rpm(bldc_control_handle_t *handle);

/**
 * @brief set motor RPM
 *
 * @param handle pointer to bldc control handle
 * @param speed_rpm motor RPM
 * @return ESP_OK on success
 */
esp_err_t bldc_control_set_speed_rpm(bldc_control_handle_t *handle, int speed_rpm);

#ifdef __cplusplus
}
#endif
