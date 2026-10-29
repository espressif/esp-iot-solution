/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_driver.h"
#include "esp_attr.h"
#include "bldc_control_param.h"

typedef struct {
    bldc_gpio_config_t comparer_gpio[PHASE_MAX]; /*!< Read IO for hardware comparators */
} bldc_zero_cross_comparer_config_t;

/**
 * @brief zero cross comparer handle
 *
 */
typedef void *bldc_zero_cross_comparer_handle_t;

/**
 * @brief Used to detect the value of gpio when the top tube and bottom tube are open
 *
 * @param timer MCPWM timer handle
 * @param edata MCPWM timer event data, fed by driver
 * @param user_data User data, set in `mcpwm_timer_register_event_callbacks()`
 * @return  Whether a high priority task has been waken up by this function
 */
bool read_comparer_on_full(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_data);

/**
 * @brief Initialize zero cross comparer
 *
 * @param handle pointer to zero cross comparer handle
 * @param config pointer to zero cross comparer config
 * @param control_param Pointer to the control parameter
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG if the parameter is invalid
 *      ESP_ERR_NO_MEM if memory allocation failed
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_zero_cross_comparer_init(bldc_zero_cross_comparer_handle_t *handle, bldc_zero_cross_comparer_config_t *config, control_param_t *control_param);

/**
 * @brief deinitialize zero cross comparer
 *
 * @param handle pointer to zero cross comparer handle
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG if the parameter is invalid
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_zero_cross_comparer_deinit(bldc_zero_cross_comparer_handle_t handle);

/**
 * @brief Comparator detects over-zero key functions without the user having to call them themselves
 *
 * @param handle pointer to zero cross comparer handle
 * @return
 *      0 normal
 *      1 Access to sensorless closed loop control
 */
uint8_t bldc_zero_cross_comparer_operation(void *handle);

/**
 * @brief Called before the motor starts to rotate, used to initialize the parameters, the user does not need to call their own
 *
 * @param handle pointer to zero cross comparer handle
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bldc_zero_cross_comparer_start(bldc_zero_cross_comparer_handle_t handle);

#ifdef __cplusplus
}
#endif
