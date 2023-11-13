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
#include "driver/mcpwm_prelude.h"

// 10MHz, 1 tick = 0.1us

#define MCPWM_MAX_COMPARATOR (3)

#define MCPWM_TIMER_CONFIG_DEFAULT(mc_group_id)   \
{                                                 \
    .group_id = mc_group_id,                      \
    .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,       \
    .resolution_hz = MCPWM_CLK_SRC,               \
    .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN, \
    .period_ticks = (uint32_t)(MCPWM_PERIOD) \
};

typedef struct {
    /*!< TODO: Add deadtime config */
    int group_id;
    int gpio_num[3];
    mcpwm_timer_event_callbacks_t *cbs;
    void *timer_cb_user_data;
} bldc_mcpwm_config_t;

esp_err_t bldc_mcpwm_init(bldc_mcpwm_config_t *mcpwm_config, void **cmprs);

esp_err_t bldc_mcpwm_deinit(void *cmprs);

esp_err_t bldc_mcpwm_set_duty(void *cmprs, uint32_t duty);

#ifdef __cplusplus
}
#endif
