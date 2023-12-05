/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_simplefoc.h"

#if CONFIG_SOC_MCPWM_SUPPORTED
#define USING_MCPWM
#endif

BLDCMotor motor = BLDCMotor(14);
BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);

extern "C" void app_main(void)
{
    SimpleFOCDebug::enable();                               /*!< Enable Debug */
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;

#ifdef USING_MCPWM
    driver.init(0);
#else
    driver.init({1, 2, 3});
#endif
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0;
    motor.voltage_limit = 12.0;
    motor.controller = MotionControlType::velocity_openloop;/*!< Set openloop control mode */

    motor.init();
    while (1) {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
