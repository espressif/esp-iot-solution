/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#include "airmouse_config.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AIRMOUSE_CONFIG_CHANGE_NONE = 0,
    AIRMOUSE_CONFIG_CHANGE_POSE = BIT0,
    AIRMOUSE_CONFIG_CHANGE_KNOB = BIT1,
    AIRMOUSE_CONFIG_CHANGE_SPACE_SWITCH = BIT2,
    AIRMOUSE_CONFIG_CHANGE_INFERENCE = BIT3,
    AIRMOUSE_CONFIG_CHANGE_HARDWARE = BIT4,
} airmouse_config_change_flags_t;

uint32_t airmouse_runtime_config_get_change_flags(
    const airmouse_config_t *old_config,
    const airmouse_config_t *new_config);

#ifdef __cplusplus
}
#endif
