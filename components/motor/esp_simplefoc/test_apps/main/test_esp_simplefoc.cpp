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
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_simplefoc.h"
#include "unity.h"

const char *TAG = "TEST";

void angle_sensor_scl_1_sda_2_init()
{
    sensor_as5600_init(0, GPIO_NUM_1, GPIO_NUM_2);
}

float angle_sensor_scl_1_sda_2_get()
{
    return sensor_as5600_getAngle(0);
}

void angle_sensor_scl_40_sda_39_init()
{
    sensor_as5600_init(0, GPIO_NUM_40, GPIO_NUM_39);
}

float angle_sensor_scl_40_sda_39_get()
{
    return sensor_as5600_getAngle(0);
}

void angle_sensor_scl_42_sda_41_init()
{
    sensor_as5600_init(1, GPIO_NUM_42, GPIO_NUM_41);
}

float angle_sensor_scl_42_sda_41_get()
{
    return sensor_as5600_getAngle(1);
}

// LowsideCurrentSense cs = LowsideCurrentSense(0.005f, 10, 4, 5, 6);

// float target_value = 0.0f;
// Commander command = Commander(Serial);
// void doTarget(char *cmd) { command.scalar(&target_value, cmd); }
// void onMotor(char *cmd) { command.motor(&motor, cmd); }

TEST_CASE("test esp_simplefoc openloop control", "[single motor][openloop][14pp][mcpwm]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);

    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init(0); // enable 3pwm driver, use mcpwm group_0
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor.voltage_limit = 12.0;   // 12 Volt
    motor.controller = MotionControlType::velocity_openloop;

    motor.init();
    while (1)
    {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc openloop control", "[single motor][openloop][14pp][ledc]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);

    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init({1, 2, 3}); // enable 3pwm driver, use ledc channel {1,2,3}
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor.voltage_limit = 12.0;   // 12 Volt
    motor.controller = MotionControlType::velocity_openloop;

    motor.init();
    while (1)
    {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc openloop control", "[single motor][openloop][14pp][auto]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);

    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init(); // enable 3pwm driver, system automatically selects the idle driver
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor.voltage_limit = 12.0;   // 12 Volt
    motor.controller = MotionControlType::velocity_openloop;

    motor.init();
    while (1)
    {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc position control", "[single motor][position][14pp][auto]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);
    GenericSensor sensor = GenericSensor(angle_sensor_scl_1_sda_2_get, angle_sensor_scl_1_sda_2_init);
    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    sensor.init(); // enable as5600 angle sensor
    motor.linkSensor(&sensor);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init(); // enable 3pwm driver, system automatically selects the idle driver.
    motor.linkDriver(&driver);
    motor.controller = MotionControlType::angle; // set position control mode

    // set velocity pid
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;

    // set angle pid
    motor.LPF_velocity.Tf = 0.05;
    motor.P_angle.P = 15.5;
    motor.P_angle.D = 0.05;
    motor.velocity_limit = 200;

    Serial.begin(115200);

    motor.useMonitoring(Serial);
    motor.init();    // initialize motor
    motor.initFOC(); // align sensor and start FOC

    while (1)
    {
        motor.loopFOC();
        motor.move(2.5f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc position control", "[single motor][position][14pp][mcpwm]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);
    GenericSensor sensor = GenericSensor(angle_sensor_scl_1_sda_2_get, angle_sensor_scl_1_sda_2_init);
    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    sensor.init(); // enable as5600 angle sensor
    motor.linkSensor(&sensor);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init(0); // enable 3pwm driver, use mcpwm driver, group 0.
    motor.linkDriver(&driver);
    motor.controller = MotionControlType::angle; // set position control mode

    // set velocity pid
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;

    // set angle pid
    motor.LPF_velocity.Tf = 0.05;
    motor.P_angle.P = 15.5;
    motor.P_angle.D = 0.05;
    motor.velocity_limit = 200;

    Serial.begin(115200);

    motor.useMonitoring(Serial);
    motor.init();    // initialize motor
    motor.initFOC(); // align sensor and start FOC

    while (1)
    {
        motor.loopFOC();
        motor.move(2.5f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc position control", "[single motor][position][14pp][ledc]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);
    GenericSensor sensor = GenericSensor(angle_sensor_scl_1_sda_2_get, angle_sensor_scl_1_sda_2_init);
    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    sensor.init(); // enable as5600 angle sensor
    motor.linkSensor(&sensor);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init({4, 5, 6}); // enable 3pwm driver, use ledc driver {4,5,6}
    motor.linkDriver(&driver);
    motor.controller = MotionControlType::angle; // set position control mode

    // set velocity pid
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;

    // set angle pid
    motor.LPF_velocity.Tf = 0.05;
    motor.P_angle.P = 15.5;
    motor.P_angle.D = 0.05;
    motor.velocity_limit = 200;

    Serial.begin(115200);

    motor.useMonitoring(Serial);
    motor.init();    // initialize motor
    motor.initFOC(); // align sensor and start FOC

    while (1)
    {
        motor.loopFOC();
        motor.move(2.5f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc velocity control", "[single motor][velocity][14pp][auto]")
{

    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(17, 16, 15);
    GenericSensor sensor = GenericSensor(angle_sensor_scl_1_sda_2_get, angle_sensor_scl_1_sda_2_init);
    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    sensor.init(); // enable as5600 angle sensor
    motor.linkSensor(&sensor);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init(); // enable 3pwm driver, auto select.
    motor.linkDriver(&driver);
    motor.controller = MotionControlType::velocity; // set position control mode

    // set velocity pid
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;

    // set angle pid
    motor.LPF_velocity.Tf = 0.05;
    motor.P_angle.P = 15.5;
    motor.P_angle.D = 0.05;
    motor.velocity_limit = 200;

    Serial.begin(115200);

    motor.useMonitoring(Serial);
    motor.init();    // initialize motor
    motor.initFOC(); // align sensor and start FOC

    while (1)
    {
        motor.loopFOC();
        motor.move(2.5f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc openloop control", "[two motors][openloop][7pp][auto]")
{
    BLDCMotor motor1 = BLDCMotor(7);
    BLDCDriver3PWM driver1 = BLDCDriver3PWM(13, 14, 21);

    BLDCMotor motor2 = BLDCMotor(7);
    BLDCDriver3PWM driver2 = BLDCDriver3PWM(47, 48, 45);

    pinMode(GPIO_NUM_38, OUTPUT);
    digitalWrite(GPIO_NUM_38, HIGH); // enable drv8313

    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    // M1
    driver1.voltage_power_supply = 12;
    driver1.voltage_limit = 11;
    driver1.init(); // enable 3pwm driver, auto select
    motor1.linkDriver(&driver1);

    motor1.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor1.voltage_limit = 12.0;   // 12 Volt
    motor1.controller = MotionControlType::velocity_openloop;
    motor1.init();

    // M2
    driver2.voltage_power_supply = 12;
    driver2.voltage_limit = 11;
    driver2.init(); // enable 3pwm driver, auto select
    motor2.linkDriver(&driver2);
    motor2.velocity_limit = 200.0; // 200 rad/s velocity limit
    motor2.voltage_limit = 12.0;   // 12 Volt
    motor2.controller = MotionControlType::velocity_openloop;
    motor2.init();

    while (1)
    {
        motor1.move(1.2f);
        motor2.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("test esp_simplefoc position control", "[two motors][position][7pp][auto]")
{
    BLDCMotor motor1 = BLDCMotor(7);
    BLDCDriver3PWM driver1 = BLDCDriver3PWM(13, 14, 21);
    GenericSensor sensor1 = GenericSensor(angle_sensor_scl_40_sda_39_get, angle_sensor_scl_40_sda_39_init);

    BLDCMotor motor2 = BLDCMotor(7);
    BLDCDriver3PWM driver2 = BLDCDriver3PWM(47, 48, 45);
    GenericSensor sensor2 = GenericSensor(angle_sensor_scl_42_sda_41_get, angle_sensor_scl_42_sda_41_init);

    pinMode(GPIO_NUM_38, OUTPUT);
    digitalWrite(GPIO_NUM_38, HIGH); // enable drv8313

    SimpleFOCDebug::enable(); // enbale debug
    Serial.begin(115200);

    // M1
    sensor1.init(); // enable as5600 angle sensor
    motor1.linkSensor(&sensor1);
    driver1.voltage_power_supply = 12;
    driver1.voltage_limit = 11;
    driver1.init(); // enable 3pwm driver, auto select
    motor1.linkDriver(&driver1);
    motor1.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor1.controller = MotionControlType::angle;

    motor1.PID_velocity.P = 0.2f;
    motor1.PID_velocity.I = 1.3f;
    motor1.voltage_limit = 11;
    motor1.voltage_sensor_align = 2;
    motor1.LPF_velocity.Tf = 0.1;
    motor1.P_angle.P = 8.5;
    motor1.P_angle.D = 0.2;
    motor1.velocity_limit = 200;

    motor1.useMonitoring(Serial);
    motor1.init();    // initialize motor
    motor1.initFOC(); // align sensor and start FOC

    // M2
    sensor2.init(); // enable as5600 angle sensor
    motor2.linkSensor(&sensor2);
    driver2.voltage_power_supply = 12;
    driver2.voltage_limit = 11;
    driver2.init(); // enable 3pwm driver, auto select
    motor2.linkDriver(&driver2);
    motor2.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor2.controller = MotionControlType::angle;

    motor2.PID_velocity.P = 0.2f;
    motor2.PID_velocity.I = 1.3f;
    motor2.voltage_limit = 11;
    motor2.voltage_sensor_align = 2;
    motor2.LPF_velocity.Tf = 0.1;
    motor2.P_angle.P = 1.5;
    motor2.P_angle.D = 0.2;
    motor2.velocity_limit = 200;

    motor2.useMonitoring(Serial);
    motor2.init();    // initialize motor
    motor2.initFOC(); // align sensor and start FOC

    while (1)
    {
        motor1.move(1.2f);
        motor2.move(1.2f);
        motor1.loopFOC();
        motor2.loopFOC();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(void)
{
    printf("  _____ ____  ____      ____ ___ __  __ ____  _     _____ _____ ___   ____ \n");
    printf(" | ____/ ___||  _ \\    / ___|_ _|  \\/  |  _ \\| |   | ____|  ___/ _ \\ / ___|\n");
    printf(" |  _| \\___ \\| |_) |___\\___ \\| || |\\/| | |_) | |   |  _| | |_ | | | | |    \n");
    printf(" | |___ ___) |  __/_____|__) | || |  | |  __/| |___| |___|  _|| |_| | |___ \n");
    printf(" |_____|____/|_|       |____/___|_|  |_|_|   |_____|_____|_|   \\___/ \\____|\n");
    unity_run_menu();
}