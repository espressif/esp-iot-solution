/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "app_variable.h"
#include "app_rainmaker.h"
#include "hal_stepper_motor.h"
#include "hal_fan_button.h"
#include "hal_bldc.h"
#include "bldc_fan_config.h"

void app_main(void)
{
    app_variable_init();
    app_rainmaker_init();
    hal_bldc_init(CW);
    hal_stepper_motor_init(STEPPER_A_PIN, STEPPER_B_PIN, STEPPER_C_PIN, STEPPER_D_PIN);
    hal_fan_button_init(SETTING_START_PIN, SETTING_MODE_PIN, SETTING_SHACKING_HEAD_PIN, SETTING_TIMER_PIN);
}
