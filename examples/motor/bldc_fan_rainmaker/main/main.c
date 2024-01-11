/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
#include "hal_bldc.h"

void app_main(void)
{
    app_variable_init();
    app_rainmaker_init();
    hal_bldc_init(CW);
    hal_stepper_motor_init(GPIO_NUM_14, GPIO_NUM_21, GPIO_NUM_47, GPIO_NUM_48);
}
