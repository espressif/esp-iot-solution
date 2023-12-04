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

typedef enum {
    CONTROL_TYPE_LEDC = 0,   /*!< LEDC control */
#if CONFIG_SOC_MCPWM_SUPPORTED
    CONTROL_TYPE_MCPWM,      /*!< MCPWM control */
#endif
    CONTROL_TYPE_GPIO,       /*!< GPIO control */
} control_type_t;

typedef struct {
    control_type_t control_type;                 /*!< Control type */
    union {
        bldc_ledc_config_t bldc_ledc;            /*!< LEDC control config */
#if CONFIG_SOC_MCPWM_SUPPORTED
        bldc_mcpwm_config_t bldc_mcpwm;          /*!< MCPWM control config */
#endif
        bldc_gpio_config_t bldc_gpio[PHASE_MAX]; /*!< GPIO control config */
    };
} switch_config_t_t;

typedef struct {
    bool en_level;                          /*!< Enable level */
    bool has_enable;                        /*!< Enable pin or not */
    bldc_gpio_config_t en_gpio[PHASE_MAX];  /*!< If three enable pins share a gpio, all three need to be set to the same */
} mos_en_config_t;

typedef struct {
    bool lower_switch_active_level;        /*!< Lower mos tube active level */
    switch_config_t_t upper_switch_config; /*!< Upper mos tube configuration */
    switch_config_t_t lower_switch_config; /*!< Lower mos tube configuration */
    mos_en_config_t mos_en_config;         /*!< mos tube enable configuration */
} bldc_six_step_config_t;

/**
 * @brief Six step handle
 *
 */
typedef void *bldc_six_step_handle_t;

/**
 * @brief Initialize the six-step phase change
 *
 * @param handle Pointer to the handle
 * @param config Pointer to the configuration
 * @param control_param Pointer to the control parameter
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG if the parameter is invalid
 *      ESP_ERR_NO_MEM if memory allocation failed
 *      ESP_ERR_INVALID_STATE Unsupported control types
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_six_step_init(bldc_six_step_handle_t *handle, bldc_six_step_config_t *config, control_param_t *control_param);

/**
 * @brief Deinitialize the six-step phase change
 *
 * @param handle Pointer to the handle
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG if the parameter is invalid
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_six_step_deinit(bldc_six_step_handle_t handle);

/**
 * @brief Six-step phase change key functions that the user doesn't have to call themselves
 *
 * @param handle Pointer to the handle
 * @return
 *      0 normal
 *      1 inject success
 */
uint8_t bldc_six_step_operation(void *handle);

/**
 * @brief Reinitialize the relevant parameters, which need to be called every time the motor is started, and the user does not need to care about
 *
 * @param handle Pointer to the handle
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bldc_six_step_start(bldc_six_step_handle_t handle);

/**
 * @brief To stop the motor, the user does not need to call
 *
 * @param handle Pointer to the handle
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bldc_six_step_stop(bldc_six_step_handle_t handle);

#ifdef __cplusplus
}
#endif
