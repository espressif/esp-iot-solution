/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "usb_host_msc.h"
#include "usb_device_cdc.h"
#include "iot_button.h"
#include "button_gpio.h"
#include "driver/gpio.h"

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2
#define BOOT_GPIO GPIO_NUM_0
#elif CONFIG_IDF_TARGET_ESP32P4
#define BOOT_GPIO GPIO_NUM_35
#endif

static const char *TAG = "manual_switch";

typedef enum {
    OTG_HOST = 0,
    OTG_DEVICE,
} usb_otg_mode_t;

static usb_otg_mode_t s_usb_otg_mode = OTG_HOST;

static void usb_otg_mode_switch_cb(void *arg, void *data)
{
    if (s_usb_otg_mode == OTG_HOST) {
        ESP_LOGI(TAG, "Switch to USB Device Mode\n");
        host_msc_deinit();
        device_cdc_init();
        s_usb_otg_mode = OTG_DEVICE;
    } else {
        ESP_LOGI(TAG, "Switch to USB Device Mode\n");
        device_cdc_deinit();
        host_msc_init();
        s_usb_otg_mode = OTG_HOST;
    }
}

static void usb_otg_mode_switch_init(void)
{
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BOOT_GPIO,
        .active_level = 0,
    };
    button_handle_t btn = NULL;
    iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &btn);
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, usb_otg_mode_switch_cb, NULL);
}

void app_main(void)
{
    esp_event_loop_create_default();
    usb_otg_mode_switch_init();
    s_usb_otg_mode = OTG_HOST;
    host_msc_init();
}
