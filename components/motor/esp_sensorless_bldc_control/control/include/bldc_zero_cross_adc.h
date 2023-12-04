/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_common.h"
#include "esp_attr.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "bldc_driver.h"
#include "bldc_control_param.h"

typedef enum {
    U_PHASE_CHANNEL = 0,   /*!< U phase adc channel */
    V_PHASE_CHANNEL,       /*!< V phase adc channel */
    W_PHASE_CHANNEL,       /*!< W phase adc channel */
    BUS_CHANNEL,           /*!< bus adc channel */
    POW_CHANNEL,           /*!< power adc channel */
    BLDC_ADC_CHANNEL_MAX,
} bldc_adc_channel_enum_t;

typedef struct {
    adc_oneshot_unit_handle_t *adc_handle;            /*!< ADC handle */
    adc_unit_t adc_unit;                              /*!< ADC unit */
    adc_oneshot_chan_cfg_t chan_cfg;                  /*!< ADC channel configuration */
    adc_channel_t adc_channel[BLDC_ADC_CHANNEL_MAX];  /*!< ADC channel */
} bldc_zero_cross_adc_config_t;

/**
 * @brief bldc zero cross adc handle
 *
 */
typedef void *bldc_zero_cross_adc_handle_t;

/**
 * @brief Used to detect the value of adc when the top tube and bottom tube are open
 *
 * @param[in] timer MCPWM timer handle
 * @param[in] edata MCPWM timer event data, fed by driver
 * @param[in] user_ctx User data, set in `mcpwm_timer_register_event_callbacks()`
 * @return Whether a high priority task has been waken up by this function
 */
bool read_adc_on_full(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_data);

/**
 * @brief bldc zero cross adc init
 *
 * @param handle Pointer to the bldc zero cross adc handle
 * @param config Pointer to the bldc zero cross adc config
 * @param control_param Pointer to the control parameter
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG if the parameter is invalid
 *      ESP_ERR_NO_MEM if memory allocation failed
 *      ESP_FAIL on other errors
 */
esp_err_t bldc_zero_cross_adc_init(bldc_zero_cross_adc_handle_t *handle, bldc_zero_cross_adc_config_t *config, control_param_t *control_param);

/**
 * @brief The adc detects the zero crossing key function without the user having to call it himself.
 *
 * @param handle Pointer to the bldc zero cross adc handle
 * @return
 *      1 Access to closed-loop sensorless control
 *      0 Nominal operation
 */
uint8_t bldc_zero_cross_adc_operation(void *handle);

/**
 * @brief Initialization of the parameters before motor rotation, the user does not need to call them himself
 *
 * @param handle Pointer to the bldc zero cross adc handle
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bldc_zero_cross_adc_start(bldc_zero_cross_adc_handle_t *handle);

/**
 * @brief Deinitialization of the parameters after motor rotation, the user does not need to call them himself
 *
 * @param handle Pointer to the bldc zero cross adc handle
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t bldc_zero_cross_adc_deinit(bldc_zero_cross_adc_handle_t handle);

#ifdef __cplusplus
}
#endif
