/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "led_common.h"
#include "led_indicator_gpio.h"
#include "led_indicator_blink_default.h"

#define TAG "led_gpio"

typedef struct {
    bool is_active_level_high;      /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    uint32_t io_num;
} led_gpio_t;

static esp_err_t led_indicator_gpio_init(void *param, void **ret_handle)
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
    gpio_set_level(cfg->gpio_num, cfg->is_active_level_high ? 0 : 1);  /*!< Off by default */
    led_gpio_t *handle = calloc(1, sizeof(led_gpio_t));
    handle->is_active_level_high = cfg->is_active_level_high;
    handle->io_num = cfg->gpio_num;
    *ret_handle = (void *)handle;
    return ESP_OK;
}

static esp_err_t led_indicator_gpio_deinit(void *handle)
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

static esp_err_t led_indicator_gpio_set_on_off(void *handle, bool on_off)
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

esp_err_t led_indicator_new_gpio_device(const led_indicator_config_t *led_config, const led_indicator_gpio_config_t *gpio_cfg, led_indicator_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;

    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(gpio_cfg != NULL, "invalid config pointer", return ESP_ERR_INVALID_ARG);
    LED_INDICATOR_CHECK(GPIO_IS_VALID_GPIO(gpio_cfg->gpio_num), "invalid GPIO number", return ESP_ERR_INVALID_ARG);

    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;

    void *hardware_data = NULL;
    ret = led_indicator_gpio_init((void *)gpio_cfg, &hardware_data);
    LED_INDICATOR_CHECK(ESP_OK == ret, "LED rgb init failed", return ESP_FAIL);
    com_cfg.hardware_data = hardware_data;
    com_cfg.hal_indicator_set_on_off = led_indicator_gpio_set_on_off;
    com_cfg.hal_indicator_deinit = led_indicator_gpio_deinit;
    com_cfg.hal_indicator_set_brightness =  NULL;
    com_cfg.duty_resolution = LED_DUTY_1_BIT;

    if (led_config->blink_lists == NULL) {
        ESP_LOGI(TAG, "blink_lists is null, use default blink list");
        com_cfg.blink_lists = default_led_indicator_blink_lists;
        com_cfg.blink_list_num = DEFAULT_BLINK_LIST_NUM;
        if_blink_default_list = true;
    } else {
        com_cfg.blink_lists = led_config->blink_lists;
        com_cfg.blink_list_num = led_config->blink_list_num;
    }

    p_led_indicator = _led_indicator_create_com(&com_cfg);

    LED_INDICATOR_CHECK(NULL != p_led_indicator, "LED indicator create failed", return ESP_FAIL);
    _led_indicator_add_node(p_led_indicator);
    ESP_LOGI(TAG, "Indicator create successfully. type:GPIO mode, hardware_data:%p, blink_lists:%s", p_led_indicator->hardware_data, if_blink_default_list ? "default" : "custom");
    *handle = (led_indicator_handle_t)p_led_indicator;
    return ESP_OK;
}
