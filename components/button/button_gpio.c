/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/gpio.h"
#include "button_gpio.h"

static const char *TAG = "gpio button";

#define GPIO_BTN_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

esp_err_t button_gpio_init(const button_gpio_config_t *config)
{
    GPIO_BTN_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    GPIO_BTN_CHECK(GPIO_IS_VALID_GPIO(config->gpio_num), "GPIO number error", ESP_ERR_INVALID_ARG);

    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << config->gpio_num);
    if (config->active_level) {
        gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    } else {
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    gpio_config(&gpio_conf);

    return ESP_OK;
}

esp_err_t button_gpio_deinit(int gpio_num)
{
    return gpio_reset_pin(gpio_num);;
}

uint8_t button_gpio_get_key_level(void *gpio_num)
{
    return (uint8_t)gpio_get_level((uint32_t)gpio_num);
}
