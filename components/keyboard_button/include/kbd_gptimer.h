/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gptimer.h"

// 1MHz, 1 tick = 1us
#define GPTIMER_CONFIG_DEFAULT()        \
{                                       \
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, \
    .direction = GPTIMER_COUNT_UP,      \
    .resolution_hz = 1 * 1000 * 1000,   \
}

#define GPTIMER_ALARM_CONFIG_DEFAULT(count) \
{                                       \
    .reload_count = 0,                  \
    .flags.auto_reload_on_alarm = true, \
    .alarm_count = count,               \
}

typedef struct {
    gptimer_handle_t *gptimer;      /*!< Pointer to gptimer handle */
    gptimer_event_callbacks_t cbs;  /*!< gptimer event callbacks */
    void *user_data;                /*!< User data */
    uint32_t alarm_count_us;        /*!< Timer interrupt period */
} kbd_gptimer_config_t;

/**
 * @brief  Initialize gptime
 *
 * @param config Configuration for the gptimer
 * @return
 *     ESP_OK on success
 *     ESP_ERR_INVALID_ARG if the parameter is invalid
 */
esp_err_t kbd_gptimer_init(kbd_gptimer_config_t *config);

/**
 * @brief deinitialize gptimer
 *
 * @param gptimer gptimer handle
 * @return
 *      ESP_ERR_INVALID_ARG if parameter is invalid
 *      ESP_OK if success
 */
esp_err_t kbd_gptimer_deinit(gptimer_handle_t gptimer);

/**
 * @brief Stop the gptimer.
 *
 * @param gptimer gptimer handle
 * @return ESP_OK if success
 */
esp_err_t kbd_gptimer_stop(gptimer_handle_t gptimer);

/**
 * @brief Start the gptimer.
 *
 * @param gptimer gptimer handle
 * @return ESP_OK if success
 */
esp_err_t kbd_gptimer_start(gptimer_handle_t gptimer);

#ifdef __cplusplus
}
#endif
