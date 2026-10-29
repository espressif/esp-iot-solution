/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Default steady-on LED preset (used when test mode 1 multi-demo is off). */

#include "esl_ap_led_preset.h"

#include "sdkconfig.h"
#include <string.h>

void esl_ap_led_fill_default_settings(esl_ap_led_settings_t *led)
{
    if (led == NULL) {
        return;
    }

    memset(led, 0, sizeof(*led));
    led->color_red = CONFIG_ESL_AP_LED_COLOR_RED;
    led->color_green = CONFIG_ESL_AP_LED_COLOR_GREEN;
    led->color_blue = CONFIG_ESL_AP_LED_COLOR_BLUE;
    led->brightness = CONFIG_ESL_AP_LED_BRIGHTNESS;
    /* repeat_type=1 + repeats_duration=0 => steady on (ESL Profile). */
    led->repeat_type = 1;
    led->repeats_duration = 0;
}
