/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bldc_gpio.h"

static const char *TAG = "bldc_gpio";

esp_err_t bldc_gpio_init(const bldc_gpio_config_t *config)
{
    BLDC_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    BLDC_CHECK(GPIO_IS_VALID_GPIO(config->gpio_num), "GPIO number error", ESP_ERR_INVALID_ARG);
    gpio_config_t gpio_conf = GPIO_CONFIG_DEFAULT(config->gpio_mode, config->gpio_num);
    if (config->gpio_mode == GPIO_MODE_INPUT) {
        if (config->active_level) {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        } else {
            gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
    }
    gpio_config(&gpio_conf);

    return ESP_OK;
}

esp_err_t bldc_gpio_deinit(int gpio_num)
{
    gpio_reset_pin(gpio_num);
    return ESP_OK;
}

esp_err_t bldc_gpio_set_level(void *gpio_num, uint32_t level)
{
    return gpio_set_level((uint32_t)gpio_num, level);
}

int bldc_gpio_get_level(uint32_t gpio_num)
{
    return gpio_get_level(gpio_num);
}
