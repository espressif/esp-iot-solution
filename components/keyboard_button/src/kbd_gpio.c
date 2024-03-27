/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/gpio.h"
#include "kbd_gpio.h"
#include "esp_check.h"
#include "esp_sleep.h"

static const char *TAG = "kbd_gpio";

esp_err_t kbd_gpio_init(const kbd_gpio_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Pointer of config is invalid");
    ESP_RETURN_ON_FALSE(config->gpio_num < GPIO_NUM_MAX && config->gpio_num > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid GPIO number");

    gpio_config_t gpio_conf = {0};
    esp_err_t ret = ESP_OK;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    if (config->gpio_mode == KBD_GPIO_MODE_INPUT) {
        gpio_conf.mode = GPIO_MODE_INPUT;
        if (config->active_level) {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        } else {
            gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
    } else {
        gpio_conf.mode = GPIO_MODE_OUTPUT;
    }

    for (int i = 0; i < config->gpio_num; i++) {
        ESP_RETURN_ON_FALSE(config->gpios[i] < GPIO_NUM_MAX && config->gpios[i] > -1, ESP_ERR_INVALID_ARG, TAG, "Invalid GPIO number");
        gpio_conf.pin_bit_mask = 1ULL << config->gpios[i];
        gpio_config(&gpio_conf);
    }

    if (config->enable_power_save) {
        if (!(config->gpio_mode == KBD_GPIO_MODE_INPUT)) {
            return ESP_OK;
        }

        for (int i = 0; i < config->gpio_num; i++) {
            ret = gpio_wakeup_enable(config->gpios[i], config->active_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
            ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Enable gpio wakeup failed");
        }
        ret = esp_sleep_enable_gpio_wakeup();
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "Enable gpio wakeup failed");
    }

    // TODO: Use dedicated_gpio

    return ESP_OK;
}

void kbd_gpio_deinit(const int *gpios, uint32_t gpio_num)
{
    for (int i = 0; i < gpio_num; i++) {
        gpio_reset_pin(gpios[i]);
    }
}

uint32_t kbd_gpios_read_level(const int *gpios, uint32_t gpio_num)
{
    uint32_t level = 0;
    for (int i = 0; i < gpio_num; i++) {
        level |= (gpio_get_level(gpios[i]) << i);
    }
    return level;
}

uint32_t kbd_gpio_read_level(int gpio)
{
    return (uint32_t)gpio_get_level(gpio);
}

void kbd_gpios_set_level(const int *gpios, uint32_t gpio_num, uint32_t level)
{
    for (int i = 0; i < gpio_num; i++) {
        gpio_set_level(gpios[i], (level >> i) & 0x01);
    }
}

void kbd_gpio_set_level(int gpio, uint32_t level)
{
    gpio_set_level(gpio, level);
}

void kbd_gpios_set_hold_en(const int *gpios, uint32_t gpio_num)
{
    for (int i = 0; i < gpio_num; i++) {
        gpio_hold_en(gpios[i]);
    }
}

void kbd_gpios_set_hold_dis(const int *gpios, uint32_t gpio_num)
{
    for (int i = 0; i < gpio_num; i++) {
        gpio_hold_dis(gpios[i]);
    }
}

esp_err_t kbd_gpios_set_intr(const int *gpios, uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args)
{
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        isr_service_installed = true;
    }
    for (int i = 0; i < gpio_num; i++) {
        gpio_set_intr_type(gpios[i], intr_type);
        gpio_isr_handler_add(gpios[i], isr_handler, args);
    }
    return ESP_OK;
}

esp_err_t kbd_gpios_intr_control(const int *gpios, uint32_t gpio_num, bool enable)
{
    for (int i = 0; i < gpio_num; i++) {
        if (enable) {
            gpio_intr_enable(gpios[i]);
        } else {
            gpio_intr_disable(gpios[i]);
        }
    }
    return ESP_OK;
}

// TODO:  uint32_t kbd_dedicated_gpio_read_level(void *bundle)
