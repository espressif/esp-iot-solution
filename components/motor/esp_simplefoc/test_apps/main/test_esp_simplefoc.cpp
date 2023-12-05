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
#include "unity.h"

const char *TAG = "TEST";

TEST_CASE("test as5600", "[sensor][as5600]")
{
    AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_8, GPIO_NUM_2);
    as5600.init();
    for (int i = 0; i < 10; ++i)
    {
        ESP_LOGI(TAG, "angle:%.2f", as5600.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    as5600.deinit();
}

TEST_CASE("test mt6701", "[sensor][mt6701]")
{
#if CONFIG_IDF_TARGET_ESP32C3
    MT6701 mt6701 = MT6701(SPI2_HOST, GPIO_NUM_2, GPIO_NUM_1, (gpio_num_t)-1, GPIO_NUM_3);
#else
    MT6701 mt6701 = MT6701(SPI2_HOST, GPIO_NUM_2, GPIO_NUM_1, (gpio_num_t)-1, GPIO_NUM_42);
#endif
    mt6701.init();
    for (int i = 0; i < 10; ++i)
    {
        ESP_LOGI(TAG, "angle:%.2f", mt6701.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    mt6701.deinit();
}

TEST_CASE("test esp_simplefoc openloop control", "[single motor][openloop][14pp][ledc][drv8313][c3]")
{
    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6, 10);

    SimpleFOCDebug::enable();                                   /*!< Enable debug */
    Serial.begin(115200);

    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init({1, 2, 3});                                     /*!< Enable ledc channel 1,2,3 */
    motor.linkDriver(&driver);

    motor.velocity_limit = 200.0;
    motor.voltage_limit = 12.0;
    motor.controller = MotionControlType::velocity_openloop;

    motor.init();

    for (int i = 0; i < 5000; ++i)
    {
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    driver.deinit();
}

TEST_CASE("test esp_simplefoc velocity control", "[single motor][velocity][14pp][ledc][drv8313][c3]")
{
    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6, 10);
    AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_8, GPIO_NUM_2);

    SimpleFOCDebug::enable();                                   /*!< Enable debug */
    Serial.begin(115200);
    as5600.init();                                              /*!< Init as5600 */
    motor.linkSensor(&as5600);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init({1, 2, 3});                                     /*!< Enable ledc channel 1,2,3 */
    motor.linkDriver(&driver);
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;   /*!< Set svpwm mode */
    motor.controller = MotionControlType::velocity;             /*!< Set velocity control mode */

    /*!< Set velocity pid */
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;
    motor.LPF_velocity.Tf = 0.05;

    Serial.begin(115200);
    motor.useMonitoring(Serial);
    motor.init();                                              /*!< Initialize motor */
    motor.initFOC();                                           /*!< Align sensor and start fog */

    for (int i = 0; i < 3000; ++i)
    {
        motor.loopFOC();
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    as5600.deinit();
    driver.deinit();
}

TEST_CASE("test esp_simplefoc position control", "[single motor][position][14pp][ledc][drv8313][c3]")
{
    BLDCMotor motor = BLDCMotor(14);
    BLDCDriver3PWM driver = BLDCDriver3PWM(4, 5, 6, 10);
    AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_8, GPIO_NUM_2);
    SimpleFOCDebug::enable();                                   /*!< Enable debug */
    Serial.begin(115200);

    as5600.init();                                              /*!< Init as5600 */
    motor.linkSensor(&as5600);
    driver.voltage_power_supply = 12;
    driver.voltage_limit = 11;
    driver.init({1, 2, 3});                                     /*!< Enable ledc channel 1,2,3 */
    motor.linkDriver(&driver);
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;   /*!< Set svpwm mode */
    motor.controller = MotionControlType::angle;                /*!< Set position control mode */

    /*!< Set velocity pid */
    motor.PID_velocity.P = 0.9f;
    motor.PID_velocity.I = 2.2f;
    motor.voltage_limit = 11;
    motor.voltage_sensor_align = 2;

    /*!< Set angle pid */
    motor.LPF_velocity.Tf = 0.05;
    motor.P_angle.P = 15.5;
    motor.P_angle.D = 0.05;
    motor.velocity_limit = 200;

    Serial.begin(115200);
    motor.useMonitoring(Serial);
    motor.init();                                               /*!< Initialize motor */
    motor.initFOC();                                            /*!< Align sensor and start fog */

    for (int i = 0; i < 3000; ++i)
    {
        motor.loopFOC();
        motor.move(1.2f);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    as5600.deinit();
    driver.deinit();
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
