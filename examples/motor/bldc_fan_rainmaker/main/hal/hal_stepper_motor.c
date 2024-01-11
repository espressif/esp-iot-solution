/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_stepper_motor.h"
#include "string.h"

stepper_motor_t stepper_motor;
void hal_stepper_motor_ctrl(stepper_dir_t dir);

static void hal_stepper_motor_timer_cb(void *args)
{
    static uint16_t step_count = 0;
    if (stepper_motor.is_start) {
        if (step_count++ == 30 * 100) {
            stepper_motor.dir = !stepper_motor.dir;
            step_count = 0;
        }
        hal_stepper_motor_ctrl(stepper_motor.dir);
    }
}

esp_err_t hal_stepper_motor_init(gpio_num_t pin_1, gpio_num_t pin_2, gpio_num_t pin_3, gpio_num_t pin_4)
{
    gpio_num_t pin[4] = {pin_1, pin_2, pin_3, pin_4};
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << pin_1) | (1ULL << pin_2) | (1ULL << pin_3) | (1ULL << pin_4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    memcpy(stepper_motor.pin, pin, sizeof(pin));

    if (gpio_config(&io_config) != ESP_OK) {
        return ESP_FAIL;
    }

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &hal_stepper_motor_timer_cb,
        .name = "periodic"
    };

    if (esp_timer_create(&periodic_timer_args, &stepper_motor.stepper_timer) != ESP_OK) {
        return ESP_FAIL;
    }

    if (esp_timer_start_periodic(stepper_motor.stepper_timer, 10 * 1000) != ESP_OK) {
        return ESP_FAIL;
    }

    stepper_motor.dir = STEP_CCW;
    return ESP_OK;
}

void hal_stepper_motor_ctrl(stepper_dir_t dir)
{
    static uint8_t step = 0;
    if (dir == STEP_CCW) {
        if (++step == 8) {
            step = 0;
        }
    } else if (dir == STEP_CW) {
        if (step == 0) {
            step = 8;
        }
        --step;
    }

    switch (step) {
    case 0:
        gpio_set_level(stepper_motor.pin[0], 1);
        gpio_set_level(stepper_motor.pin[1], 0);
        gpio_set_level(stepper_motor.pin[2], 0);
        gpio_set_level(stepper_motor.pin[3], 1);
        break;
    case 1:
        gpio_set_level(stepper_motor.pin[0], 0);
        gpio_set_level(stepper_motor.pin[1], 0);
        gpio_set_level(stepper_motor.pin[2], 0);
        gpio_set_level(stepper_motor.pin[3], 1);
        break;
    case 2:
        gpio_set_level(stepper_motor.pin[0], 0);
        gpio_set_level(stepper_motor.pin[1], 0);
        gpio_set_level(stepper_motor.pin[2], 1);
        gpio_set_level(stepper_motor.pin[3], 1);
        break;
    case 3:
        gpio_set_level(stepper_motor.pin[0], 0);
        gpio_set_level(stepper_motor.pin[1], 0);
        gpio_set_level(stepper_motor.pin[2], 1);
        gpio_set_level(stepper_motor.pin[3], 0);
        break;
    case 4:
        gpio_set_level(stepper_motor.pin[0], 0);
        gpio_set_level(stepper_motor.pin[1], 1);
        gpio_set_level(stepper_motor.pin[2], 1);
        gpio_set_level(stepper_motor.pin[3], 0);
        break;
    case 5:
        gpio_set_level(stepper_motor.pin[0], 0);
        gpio_set_level(stepper_motor.pin[1], 1);
        gpio_set_level(stepper_motor.pin[2], 0);
        gpio_set_level(stepper_motor.pin[3], 0);
        break;
    case 6:
        gpio_set_level(stepper_motor.pin[0], 1);
        gpio_set_level(stepper_motor.pin[1], 1);
        gpio_set_level(stepper_motor.pin[2], 0);
        gpio_set_level(stepper_motor.pin[3], 0);
        break;
    case 7:
        gpio_set_level(stepper_motor.pin[0], 1);
        gpio_set_level(stepper_motor.pin[1], 0);
        gpio_set_level(stepper_motor.pin[2], 0);
        gpio_set_level(stepper_motor.pin[3], 0);
        break;
    default:
        break;
    }
}
