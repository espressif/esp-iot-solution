/*
 * SPDX-FileCopyrightText: 2016-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "foc_knob.h"
#include "foc_knob_default.h"
#include "esp_simplefoc.h"
#include "iot_button.h"

#define SWITCH_BUTTON 0
#define USING_MCPWM 0
#define TAG "FOC_Knob_Example"
static knob_handle_t knob_handle_0 = NULL;
static int mode = MOTOR_UNBOUND_FINE_DETENTS;

/*update motor parameters based on hardware design*/
BLDCDriver3PWM driver = BLDCDriver3PWM(15, 16, 17);
BLDCMotor motor = BLDCMotor(7);
MT6701 mt6701 = MT6701(SPI2_HOST, GPIO_NUM_12, GPIO_NUM_13, (gpio_num_t) -1, GPIO_NUM_11);

/*Motor initialization*/
void motor_init(void)
{
    SimpleFOCDebug::enable();
    Serial.begin(115200);
    mt6701.init();
    motor.linkSensor(&mt6701);
    driver.voltage_power_supply = 5;
    driver.voltage_limit = 5;

#if USING_MCPWM
    driver.init(0);
#else
    driver.init({0, 1, 2});
#endif

    motor.linkDriver(&driver);
    motor.foc_modulation = SpaceVectorPWM;
    motor.controller = MotionControlType::torque;

    motor.PID_velocity.P = 1.0f;
    motor.PID_velocity.I = 0;
    motor.PID_velocity.D = 0.01f;
    motor.voltage_limit = 4;
    motor.LPF_velocity.Tf = 0.01;
    motor.velocity_limit = 4;

    motor.init();                                                   // initialize motor
    motor.initFOC();                                                // align sensor and start FOC
    ESP_LOGI(TAG, "Motor Initialize Successfully");
}

/*Button press callback*/
static void button_press_cb(void *arg, void *data)
{
    mode++;
    if (mode >= MOTOR_MAX_MODES) {
        mode = MOTOR_UNBOUND_FINE_DETENTS;
    }
    ESP_LOGI(TAG, "mode: %d", mode);
}

extern "C" void app_main(void)
{
    button_config_t btn_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 0,
        .short_press_time = 0,
        .gpio_button_config = {
            .gpio_num = SWITCH_BUTTON,
            .active_level = 0,
        }
    };

    button_handle_t btn =  iot_button_create(&btn_config);
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_press_cb, NULL);
    motor_init();

    knob_config_t cfg = {
        .param_lists = default_knob_param_lst,
        .param_list_num = MOTOR_MAX_MODES,
    };

    knob_handle_0 = knob_create(&cfg);

    while (1) {
        motor.loopFOC();
        float torque = knob_start(knob_handle_0, mode, motor.shaft_velocity, motor.shaft_angle);
        ESP_LOGD(TAG, "Angle: %f, Velocity: %f, torque: %f", motor.shaft_angle, motor.shaft_velocity, torque);
        motor.move(torque);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}
