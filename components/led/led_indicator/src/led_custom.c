/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "led_custom.h"
#include "led_indicator_blink_default.h"

#define TAG "led_custom"
led_indicator_handle_t iot_led_new_custom_device(const led_config_t *led_config, const led_indicator_custom_config_t *custom_cfg)
{
    esp_err_t ret = ESP_OK;
    bool if_blink_default_list = false;

    ESP_LOGI(TAG, "LED Indicator Version: %d.%d.%d", LED_INDICATOR_VER_MAJOR, LED_INDICATOR_VER_MINOR, LED_INDICATOR_VER_PATCH);
    LED_INDICATOR_CHECK(led_config != NULL, "invalid config pointer", return NULL);
    LED_INDICATOR_CHECK(custom_cfg != NULL, "invalid config pointer", return NULL);
    _led_indicator_com_config_t com_cfg = {0};
    _led_indicator_t *p_led_indicator = NULL;

    LED_INDICATOR_CHECK_WARNING(custom_cfg->hal_indicator_init != NULL, "LED indicator does not have hal_indicator_init ", goto without_init);
    ret = custom_cfg->hal_indicator_init(custom_cfg->hardware_data);
    LED_INDICATOR_CHECK(ret == ESP_OK, "LED indicator init failed", return NULL);
without_init:
    com_cfg.hardware_data = custom_cfg->hardware_data;
    com_cfg.hal_indicator_set_on_off = custom_cfg->hal_indicator_set_on_off;
    com_cfg.hal_indicator_deinit = custom_cfg->hal_indicator_deinit;
    com_cfg.hal_indicator_set_brightness = custom_cfg->hal_indicator_set_brightness;
    com_cfg.hal_indicator_set_rgb = custom_cfg->hal_indicator_set_rgb;
    com_cfg.hal_indicator_set_hsv = custom_cfg->hal_indicator_set_hsv;
    com_cfg.duty_resolution = custom_cfg->duty_resolution;

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

    LED_INDICATOR_CHECK(NULL != p_led_indicator, "LED indicator create failed", return NULL);
    p_led_indicator->mode = LED_GPIO_MODE;
    _led_indicator_add_node(p_led_indicator);
    ESP_LOGI(TAG, "Indicator create successfully. type:GPIO mode, hardware_data:%p, blink_lists:%s", p_led_indicator->hardware_data, if_blink_default_list ? "default" : "custom");
    return (led_indicator_handle_t)p_led_indicator;
}
