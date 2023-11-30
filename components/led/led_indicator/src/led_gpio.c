/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "led_gpio.h"

typedef struct {
    bool is_active_level_high;      /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    uint32_t io_num;
} led_gpio_t;

esp_err_t led_indicator_gpio_init(void *param, void **ret_handle)
{
    const led_indicator_gpio_config_t *cfg = (const led_indicator_gpio_config_t *)param;

    gpio_config_t io_conf = {0};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = 1ULL << (uint32_t)cfg->gpio_num;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        return ret;
    }

    led_gpio_t *handle = calloc(1, sizeof(led_gpio_t));
    handle->is_active_level_high = cfg->is_active_level_high;
    handle->io_num = cfg->gpio_num;
    *ret_handle = (void *)handle;
    return ESP_OK;
}

esp_err_t led_indicator_gpio_deinit(void *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    led_gpio_t *p_gpio = (led_gpio_t *)handle;
    esp_err_t ret = gpio_reset_pin(p_gpio->io_num);

    if (ret != ESP_OK) {
        return ret;
    }
    free(p_gpio);

    return ESP_OK;
}

esp_err_t led_indicator_gpio_set_on_off(void *handle, bool on_off)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    led_gpio_t *p_gpio = (led_gpio_t *)handle;
    if (!p_gpio->is_active_level_high) {
        on_off = !on_off;
    }
    return gpio_set_level(p_gpio->io_num, on_off);
}
