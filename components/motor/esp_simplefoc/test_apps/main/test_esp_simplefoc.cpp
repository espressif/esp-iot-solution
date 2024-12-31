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
#include "unity.h"

const char *TAG = "TEST";

TEST_CASE("test as5600", "[sensor][as5600][i2c]")
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
    AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_2, GPIO_NUM_1);
#else
    AS5600 as5600 = AS5600(I2C_NUM_0, GPIO_NUM_12, GPIO_NUM_13);
#endif
    as5600.init();
    for (int i = 0; i < 10; ++i) {
        ESP_LOGI(TAG, "angle:%.2f", as5600.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    as5600.deinit();
}

TEST_CASE("test mt6701", "[sensor][mt6701][spi]")
{
    MT6701 mt6701 = MT6701(SPI2_HOST, GPIO_NUM_2, GPIO_NUM_1, (gpio_num_t) -1, GPIO_NUM_3);
    mt6701.init();
    for (int i = 0; i < 10; ++i) {
        ESP_LOGI(TAG, "angle:%.2f", mt6701.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    mt6701.deinit();
}

TEST_CASE("test mt6701", "[sensor][mt6701][i2c]")
{
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
    MT6701 mt6701 = MT6701(I2C_NUM_0, GPIO_NUM_2, GPIO_NUM_1);
#else
    MT6701 mt6701 = MT6701(I2C_NUM_0, GPIO_NUM_12, GPIO_NUM_13);
#endif
    mt6701.init();

    for (int i = 0; i < 10; ++i) {
        ESP_LOGI(TAG, "angle:%.2f", mt6701.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    mt6701.deinit();
}

TEST_CASE("test as5048a", "[sensor][as5048a][spi]")
{
    AS5048a as5048a = AS5048a(SPI2_HOST, GPIO_NUM_2, GPIO_NUM_1, (gpio_num_t) -1, GPIO_NUM_3);
    as5048a.init();
    for (int i = 0; i < 10; ++i) {
        ESP_LOGI(TAG, "angle:%.2f", as5048a.getSensorAngle());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    as5048a.deinit();
}

#if CONFIG_SOC_MCPWM_SUPPORTED
TEST_CASE("test 6pwm driver", "[driver][6pwm]")
{
    BLDCDriver6PWM driver = BLDCDriver6PWM(1, 2, 3, 4, 5, 6);
    TEST_ASSERT_EQUAL(driver.init(), 1);
    driver.dc_a = 0.2;
    driver.dc_b = 0.4;
    driver.dc_c = 0.8;

    driver.halPwmWrite();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    driver.deinit();
}
#endif

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

    for (int i = 0; i < 5000; ++i) {
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

    for (int i = 0; i < 3000; ++i) {
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

    for (int i = 0; i < 3000; ++i) {
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
