/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "led_gpio.h"

esp_err_t led_indicator_gpio_init(void *io_num)
{
    gpio_config_t io_conf = {0};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = 1ULL << (uint32_t)io_num;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    esp_err_t ret = gpio_config(&io_conf);

    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t led_indicator_gpio_deinit(void *io_num)
{
    esp_err_t ret = gpio_reset_pin((uint32_t)io_num);

    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t led_indicator_gpio_set_on_off(void *io_num, bool on_off)
{
    return gpio_set_level((uint32_t)io_num, on_off);
}