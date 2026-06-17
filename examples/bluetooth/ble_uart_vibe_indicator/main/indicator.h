/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Firmware maps light_id 0/1/2 to red / yellow / green GPIO per indicator group. */
typedef enum {
    INDICATOR_LIGHT_ACTION_OFF = 0,
    INDICATOR_LIGHT_ACTION_ON = 1,
    INDICATOR_LIGHT_ACTION_SLOW_BLINK = 2,
    INDICATOR_LIGHT_ACTION_FAST_BLINK = 3,
} indicator_light_action_t;

/** Configure GPIO outputs and start the blink timer. */
esp_err_t indicator_init(void);

int indicator_get_channel_count(void);

/** Validate parameters without applying. */
const char *indicator_validate_light(int indicator_id, int light_id, int light_action);

/** Apply one lamp update within an indicator group. */
const char *indicator_set_light(int indicator_id, int light_id, int light_action);

#ifdef __cplusplus
}
#endif
