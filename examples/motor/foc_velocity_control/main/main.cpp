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

void angle_sensor_init()
{
    sensor_as5600_init(0, GPIO_NUM_1, GPIO_NUM_2);
}

float angle_sensor_get()
{
    return sensor_as5600_getAngle(0);
}

BLDCMotor motor = BLDCMotor(14);
BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);
GenericSensor sensor = GenericSensor(angle_sensor_get, angle_sensor_init);

float target_value = 0.0f;
Commander command = Commander(Serial);
void doTarget(char *cmd)
{
    command.scalar(&target_value, cmd);
}

extern "C" void app_main(void)
{
    SimpleFOCDebug::enable(); // enable debug
    Serial.begin(115200);

    sensor.init(); // enable as5600 angle sensor
    motor.linkSensor(&sensor);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
#ifdef USING_MCPWM
    driver.init(0);
#else
    driver.init({1, 2, 3});
#endif

    motor.linkDriver(&driver);
    motor.controller = MotionControlType::velocity; // set position control mode

    // set velocity pid
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;
    motor.LPF_velocity.Tf = 0.05;
    motor.velocity_limit = 200;

    motor.useMonitoring(Serial);
    motor.init();                                                   // initialize motor
    motor.initFOC();                                                // align sensor and start FOC
    command.add('T', doTarget, const_cast<char *>("target angle")); // add serial command

    while (1) {
        motor.loopFOC();
        motor.move(target_value);
        command.run();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
