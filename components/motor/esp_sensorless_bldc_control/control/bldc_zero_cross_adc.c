/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_zero_cross_adc.h"
#include "bldc_snls_lib.h"
#include "bldc_config.h"

static const char *TAG = "bldc_zero_cross_adc";

typedef struct {
    control_param_t *control_param;
    adc_oneshot_unit_handle_t adc_handle;
    adc_channel_t adc_channel[5];
    uint16_t avoid_continue_current;     /*!< Avoid continuation current counting */
    uint32_t speed_cnt;
} bldc_zero_cross_adc_t;

bool IRAM_ATTR read_adc_on_full(mcpwm_timer_handle_t timer, const mcpwm_timer_event_data_t *edata, void *user_data)
{
    bldc_zero_cross_adc_t *zero_cross = (bldc_zero_cross_adc_t *)user_data;
    if (zero_cross->control_param->status == INJECT) {
        /*!< Read the injected ADC value */
        if (zero_cross->control_param->inject_adc_read) {
            zero_cross->control_param->inject_adc_value[zero_cross->control_param->inject_count - 1] = bldc_adc_read_isr(zero_cross->adc_handle, zero_cross->adc_channel[BUS_CHANNEL]);
            /*!< ADC read complete. */
        }
    } else {
        /*!< Obtain the current phase voltage and power supply voltage. */
        zero_cross->control_param->adc_value[1] = bldc_adc_read_isr(zero_cross->adc_handle, zero_cross->adc_channel[POW_CHANNEL]);
        zero_cross->control_param->adc_value[0] = bldc_adc_read_isr(zero_cross->adc_handle, zero_cross->adc_channel[zero_cross->control_param->adc_bemf_phase]);
    }

    return false;
}

esp_err_t bldc_zero_cross_adc_init(bldc_zero_cross_adc_handle_t *handle, bldc_zero_cross_adc_config_t *config, control_param_t *control_param)
{
    BLDC_CHECK(config != NULL, "bldc zero cross adc config is NULL", ESP_ERR_INVALID_ARG);
    bldc_zero_cross_adc_t *zero_cross = (bldc_zero_cross_adc_t *)calloc(1, sizeof(bldc_zero_cross_adc_t));
    BLDC_CHECK(zero_cross != NULL, "calloc error", ESP_ERR_NO_MEM);

    zero_cross->control_param = control_param;
    esp_err_t err = ESP_OK;

    bldc_adc_config_t adc_cfg = {
        .adc_handle = config->adc_handle,
        .adc_unit = config->adc_unit,
        .chan_cfg = config->chan_cfg,
        .adc_channel = config->adc_channel,
        .adc_channel_num = 5,
    };

    /*!< Get adc handle */
    err = bldc_adc_init(&adc_cfg, &zero_cross->adc_handle);
    BLDC_CHECK_GOTO(err == ESP_OK, "adc init error", deinit);

    for (int i = 0; i < 5; i++) {
        zero_cross->adc_channel[i] = config->adc_channel[i];
    }

    *handle = (bldc_zero_cross_adc_handle_t)zero_cross;
    return ESP_OK;

deinit:
    if (zero_cross != NULL) {
        free(zero_cross);
    }
    return ESP_FAIL;
}

esp_err_t bldc_zero_cross_adc_deinit(bldc_zero_cross_adc_handle_t handle)
{
    BLDC_CHECK(handle != NULL, "bldc zero cross adc handle is NULL", ESP_ERR_INVALID_STATE);
    bldc_zero_cross_adc_t *zero_cross = (bldc_zero_cross_adc_t *)handle;
    esp_err_t err = ESP_OK;
    err = bldc_adc_deinit(zero_cross->adc_handle);
    BLDC_CHECK(err == ESP_OK, "adc deinit error", ESP_FAIL);
    return ESP_OK;
}

uint8_t bldc_zero_cross_adc_operation(void *handle)
{
    BLDC_CHECK(handle != NULL, "bldc zero cross adc handle is NULL", 0);
    bldc_zero_cross_adc_t *zero_cross = (bldc_zero_cross_adc_t *)handle;
    /*!< ADC injection phase */
    zero_cross->control_param->speed_count++;
    zero_cross->speed_cnt++;
    static bool speed_clac_flag = false;
    /*!< Motor changed six phases */
    if (zero_cross->control_param->phase_cnt == 1 && !speed_clac_flag) {
        speed_clac_flag = true;
        if (zero_cross->speed_cnt < SKIP_INVALID_SPEED_CALCULATION) {
        } else {
            uint32_t speed_rpm = 0;
            if (zero_cross->control_param->dir == CCW) {
                /*!< ft/(n*c) * 60; where ft is the counting frequency, that is, the frequency of the cycle interrupt, n is the polar logarithm, c is the number of times to remember */
                speed_rpm = (uint32_t)(ADC_RPM_CALCULATION_COEFFICIENT / zero_cross->speed_cnt);
            } else {
                speed_rpm = (uint32_t)(ADC_RPM_CALCULATION_COEFFICIENT / zero_cross->speed_cnt);
            }
            BLDC_LPF(zero_cross->control_param->speed_rpm, speed_rpm, 0.2);
            ESP_LOGD(TAG, "speed_rpm: %"PRIu32", speed_count: %"PRIu32"\n", zero_cross->control_param->speed_rpm, zero_cross->speed_cnt);
        }
        zero_cross->speed_cnt = 0;
    } else if (zero_cross->control_param->phase_cnt != 1) {
        speed_clac_flag = false;
    }

    if (zero_cross->control_param->status == DRAG) {
        if (zero_cross->control_param->speed_count >= 10000) {
            zero_cross->control_param->speed_count = 0;
            return 1;
        }
    } else {
        ESP_LOGD(TAG, "%"PRIu32",%"PRIu32",%d\n", zero_cross->control_param->adc_value[0], zero_cross->control_param->adc_value[1] >> 1, zero_cross->control_param->phase_cnt);
        uint8_t ret = 0;
        if (zero_cross->control_param->phase_change_done == true) {

            if (++zero_cross->avoid_continue_current >= AVOID_CONTINUE_CURRENT_TIME) {
                zero_cross->control_param->phase_cnt = zero_cross_adc_get_phase(zero_cross->control_param->phase_cnt, ZERO_REPEAT_TIME, zero_cross->control_param->adc_value);

                if (zero_cross->control_param->phase_cnt == zero_cross->control_param->phase_cnt_prev) {
                    zero_cross->control_param->filter_failed_count++;
                } else {
                    zero_cross->control_param->filter_delay = zero_cross->control_param->speed_count * 3.0 / ZERO_CROSS_ADVANCE; /*!< Thirty-degree time delay. At some point compensation is required */
                    zero_cross->control_param->phase_change_done = false;
                    zero_cross->control_param->speed_count = 0;
                    zero_cross->control_param->filter_failed_count = 0;
                    zero_cross->avoid_continue_current = 0;
                    ret = 1;
                }
            }

            return  ret;
        }
    }
    return 0;
}

esp_err_t bldc_zero_cross_adc_start(bldc_zero_cross_adc_handle_t *handle)
{
    BLDC_CHECK(handle != NULL, "bldc zero cross adc handle is NULL", ESP_ERR_INVALID_STATE);
    bldc_zero_cross_adc_t *zero_cross = (bldc_zero_cross_adc_t *)handle;
    zero_cross->avoid_continue_current = 0;
    zero_cross->speed_cnt = 0;
    return ESP_OK;
}
