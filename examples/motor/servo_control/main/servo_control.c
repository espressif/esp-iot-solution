/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "iot_servo.h"

#define SERVO_GPIO          (2)  // Servo GPIO

static const char *TAG = "Servo Control";

static uint16_t calibration_value_0 = 30;    // Real 0 degree angle
static uint16_t calibration_value_180 = 195; // Real 0 degree angle

// Task to test the servo
static void servo_test_task(void *arg)
{
    ESP_LOGI(TAG, "Servo Test Task");
    while (1) {
        // Set the angle of the servo
        for (int i = calibration_value_0; i <= calibration_value_180; i += 1) {
            iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, i);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        // Return to the initial position
        iot_servo_write_angle(LEDC_LOW_SPEED_MODE, 0, calibration_value_0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void servo_init(void)
{
    ESP_LOGI(TAG, "Servo Control");

    // Configure the servo
    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_GPIO,
            },
            .ch = {
                LEDC_CHANNEL_0,
            },
        },
        .channel_number = 1,
    };

    // Initialize the servo
    iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Servo Control");
    // Initialize the servo
    servo_init();
    // Create the servo test task
    xTaskCreate(servo_test_task, "servo_test_task", 2048, NULL, 5, NULL);
}
