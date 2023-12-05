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
BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6);
AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_1, GPIO_NUM_2);

float target_value = 0.0f;
Commander command = Commander(Serial);
void doTarget(char *cmd)
{
    command.scalar(&target_value, cmd);
}

extern "C" void app_main(void)
{
    SimpleFOCDebug::enable();                                        /*!< Enable debug */
    Serial.begin(115200);

    as5600.init();                                                   /*!< Enable as5600 */
    motor.linkSensor(&as5600);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
#ifdef USING_MCPWM
    driver.init(0);
#else
    driver.init({1, 2, 3});
#endif

    motor.linkDriver(&driver);
    motor.controller = MotionControlType::velocity;                  /*!< Set position control mode */
    /*!< Set velocity pid */
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;
    motor.LPF_velocity.Tf = 0.05;
    motor.velocity_limit = 200;

    motor.useMonitoring(Serial);
    motor.init();                                                    /*!< Initialize motor */
    motor.initFOC();                                                 /*!<  Align sensor and start FOC */
    command.add('T', doTarget, const_cast<char *>("target angle"));  /*!< Add serial command */

    while (1) {
        motor.loopFOC();
        motor.move(target_value);
        command.run();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
