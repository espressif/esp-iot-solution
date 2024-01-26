/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

typedef enum {
    START = 0,
    STOP,
    PREEMPT_START,
    PREEMPT_STOP,
} cmd_type_t;

typedef void (*led_indicator_cmd_cb)(cmd_type_t cmd_type, uint32_t mode_index);

typedef struct {
    uint32_t mode_count;
    led_indicator_cmd_cb cmd_cb;
} cmd_led_indicator_t;

/**
 * @brief install led indicator cmd
 *
 * @param cmd_led_indicator led indicator cmd cfg
 * @return
 *      ESP_OK success
 *      ESP_FAIL failed
 */
esp_err_t cmd_led_indicator_init(cmd_led_indicator_t *cmd_led_indicator);

#ifdef __cplusplus
}
#endif
