/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_digit.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "nvs_flash.h"

#include "digital_tube.h"

static void button_event_cb(void *arg, void *data)
{
    bool state = get_touch_dight_normalize_state();
    state = !state;
    if (state) {
        touch_dight_begin_normalize();
    } else {
        touch_dight_end_normalize();
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret;

    /* Initialize NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize button */
    const button_config_t btn_cfg = {};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = 0,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };

    /* Initialize button for normalization calibration */
    button_handle_t btn = NULL;
    ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    // Register callback for button press down to start normalization calibration
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);

    /* Initialize touch digit */
    touch_digit_init();

    /* Initialize digital tube */
    digital_tube_driver_install(I2C_NUM_0, GPIO_NUM_37, GPIO_NUM_38);
    digital_tube_enable();
}
