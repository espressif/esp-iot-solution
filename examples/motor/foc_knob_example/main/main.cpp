/*
 * SPDX-FileCopyrightText: 2016-2024 Espressif Systems (Shanghai) CO LTD
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
#define PHASE_U_GPIO 15
#define PHASE_V_GPIO 16
#define PHASE_W_GPIO 17
#define MOTOR_PP     7
#define MT6701_SPI_HOST SPI2_HOST
#define MT6701_SPI_SCLK_GPIO 12
#define MT6701_SPI_MISO_GPIO 13
#define MT6701_SPI_MOSI_GPIO -1
#define MT6701_SPI_CS_GPIO 11

#define USING_MCPWM 0

#if !USING_MCPWM
#define LEDC_CHAN_0 0
#define LEDC_CHAN_1 1
#define LEDC_CHAN_2 2
#endif

#define TAG "FOC_Knob_Example"
static foc_knob_handle_t foc_knob_handle = NULL;
static int mode = MOTOR_UNBOUND_NO_DETENTS;
static bool motor_shake = false;

/*update motor parameters based on hardware design*/
BLDCDriver3PWM driver = BLDCDriver3PWM(PHASE_U_GPIO, PHASE_V_GPIO, PHASE_W_GPIO);
BLDCMotor motor = BLDCMotor(MOTOR_PP);
MT6701 mt6701 = MT6701(MT6701_SPI_HOST, (gpio_num_t)MT6701_SPI_SCLK_GPIO, (gpio_num_t)MT6701_SPI_MISO_GPIO, (gpio_num_t)MT6701_SPI_MOSI_GPIO, (gpio_num_t)MT6701_SPI_CS_GPIO);

/*Motor initialization*/
void motor_init(void)
{
    SimpleFOCDebug::enable();
    Serial.begin(115200);
    mt6701.init();
    motor.linkSensor(&mt6701);
    driver.voltage_power_supply = 5;
    driver.voltage_limit = 5;
    motor.velocity_limit = 10000;

#if USING_MCPWM
    driver.init(0);
#else
    driver.init({LEDC_CHAN_0, LEDC_CHAN_1, LEDC_CHAN_2});
#endif

    motor.linkDriver(&driver);
    motor.foc_modulation = SpaceVectorPWM;
    motor.controller = MotionControlType::torque;

    motor.PID_velocity.P = 4.0;
    motor.PID_velocity.I = 0.0;
    motor.PID_velocity.D = 0.04;
    motor.PID_velocity.output_ramp = 10000;
    motor.PID_velocity.limit = 10;

    motor.useMonitoring(Serial);
    motor.init();                                        // initialize motor
    motor.initFOC();                                     // align sensor and start FOC
    ESP_LOGI(TAG, "Motor Initialize Successfully");
}

/*Button press callback*/
static void button_press_cb(void *arg, void *data)
{
    mode++;
    if (mode >= MOTOR_MAX_MODES) {
        mode = MOTOR_UNBOUND_NO_DETENTS;
    }
    foc_knob_change_mode(foc_knob_handle, mode);
    ESP_LOGI(TAG, "mode: %d", mode);
    motor_shake = true;
}

static void foc_knob_inc_cb(void *arg, void *data)
{
    /*!< Do not printf here */
    foc_knob_state_t state;
    foc_knob_get_state(arg, &state);
}

static void foc_knob_dec_cb(void *arg, void *data)
{
    /*!< Do not printf here */
    foc_knob_state_t state;
    foc_knob_get_state(arg, &state);
}

static void foc_knob_h_lim_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "foc_knob_h_lim_cb");
}

static void foc_knob_l_lim_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "foc_knob_l_lim_cb");
}

float motor_shake_func(float strength, int delay_cnt)
{
    static int time_cnt = 0;
    if (time_cnt < delay_cnt) {
        time_cnt++;
        return strength;
    } else if (time_cnt < 2 * delay_cnt) {
        time_cnt++;
        return -strength;
    } else {
        time_cnt = 0;
        motor_shake = false;
        return 0;
    }
}

static float motor_pid_cb(float P, float D, float limit, float error)
{
    motor.PID_velocity.limit = limit;
    motor.PID_velocity.P = P;
    motor.PID_velocity.D = D;
    return motor.PID_velocity(error);
}

static void motor_task(void *arg)
{
    static float torque = 0;
    while (1) {
        motor.loopFOC();
        if (motor_shake) {
            torque = motor_shake_func(2, 4);
        } else {
            torque = foc_knob_run(foc_knob_handle, motor.shaft_velocity, motor.shaft_angle);
        }
        motor.move(torque);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
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

    foc_knob_config_t cfg = {
        .param_lists = default_foc_knob_param_lst,
        .param_list_num = MOTOR_MAX_MODES,
        .max_torque_out_limit = 5,
        .max_torque = 5,
        .pid_cb = motor_pid_cb,
    };

    foc_knob_handle = foc_knob_create(&cfg);

    foc_knob_register_cb(foc_knob_handle, FOC_KNOB_INC, foc_knob_inc_cb, NULL);
    foc_knob_register_cb(foc_knob_handle, FOC_KNOB_DEC, foc_knob_dec_cb, NULL);
    foc_knob_register_cb(foc_knob_handle, FOC_KNOB_H_LIM, foc_knob_h_lim_cb, NULL);
    foc_knob_register_cb(foc_knob_handle, FOC_KNOB_L_LIM, foc_knob_l_lim_cb, NULL);

    xTaskCreate(motor_task, "motor_task", 4096, NULL, 5, NULL);
}
