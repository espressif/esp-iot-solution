/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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

#define USING_MCPWM

BLDCMotor motor = BLDCMotor(14);
BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);

extern "C" void app_main(void)
{
    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;

#ifdef USING_MCPWM
    driver.init(0);
#else
    driver.init({1, 2, 3});
#endif
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor.voltage_limit = 12.0;   // 12 Volt
    motor.controller = MotionControlType::velocity_openloop;

    motor.init();
    while (1) {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
