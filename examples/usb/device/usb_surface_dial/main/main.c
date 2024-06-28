/*
 * SPDX-FileCopyrightText: 2016-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_private/usb_phy.h"
#include "sdkconfig.h"
#include "tusb.h"
#include "tusb_config.h"
#include "iot_button.h"
#include "iot_knob.h"
#ifdef CONFIG_ESP32_S3_USB_OTG
#include "bsp/esp-bsp.h"
#endif

#define TAG "DIAL"

#define GPIO_BUTTON 42
#define GPIO_KNOB_A 1
#define GPIO_KNOB_B 2

#define REPORT_ID    1
#define DIAL_R       0xC8
#define DIAL_L       0x38
#define DIAL_PRESS   0x01
#define DIAL_RELEASE 0x00
#define DIAL_R_F     0x14
#define DIAL_L_F     0xEC

static button_handle_t s_btn = 0;
static knob_handle_t s_knob = 0;
static usb_phy_handle_t s_phy_hdl;

static void usb_phy_init(void)
{
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .target = USB_PHY_TARGET_INT,
    };
    usb_new_phy(&phy_conf, &s_phy_hdl);
}

static void surface_dial_report(uint8_t key)
{
    uint8_t _dial_report[2];
    _dial_report[0] = key;
    _dial_report[1] = 0;
    if (key == DIAL_L || key == DIAL_L_F) {
        _dial_report[1] = 0xff;
    }
    tud_hid_report(REPORT_ID, _dial_report, 2);
}

static void _button_press_down_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "BTN: BUTTON_PRESS_DOWN");
    surface_dial_report(DIAL_PRESS);
}

static void _button_press_up_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "BTN: BUTTON_PRESS_UP[%d]", iot_button_get_ticks_time((button_handle_t)arg));
    surface_dial_report(DIAL_RELEASE);
}

static void _knob_right_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "KONB: KONB_RIGHT,count_value:%"PRId32"", iot_knob_get_count_value((button_handle_t)arg));
    surface_dial_report(DIAL_L);
}

static void _knob_left_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "KONB: KONB_LEFT,count_value:%"PRId32"", iot_knob_get_count_value((button_handle_t)arg));
    surface_dial_report(DIAL_R);
}

static void _button_init(void)
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 1000,
        .short_press_time = 200,
        .gpio_button_config = {
            .gpio_num  = GPIO_BUTTON,
            .active_level = 0,
        },
    };
    s_btn = iot_button_create(&cfg);
    iot_button_register_cb(s_btn, BUTTON_PRESS_DOWN, _button_press_down_cb, NULL);
    iot_button_register_cb(s_btn, BUTTON_PRESS_UP, _button_press_up_cb, NULL);
}

static void _knob_init(void)
{
    knob_config_t cfg = {
        .default_direction = 0,
        .gpio_encoder_a = GPIO_KNOB_A,
        .gpio_encoder_b = GPIO_KNOB_B,
    };
    s_knob = iot_knob_create(&cfg);
    if (NULL == s_knob) {
        ESP_LOGE(TAG, "knob create failed");
    }

    iot_knob_register_cb(s_knob, KNOB_LEFT, _knob_left_cb, NULL);
    iot_knob_register_cb(s_knob, KNOB_RIGHT, _knob_right_cb, NULL);
}

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    bsp_usb_mode_select_device();
#endif
    usb_phy_init();
    _button_init();
    _knob_init();

    tusb_init();

    size_t timeout_tick = pdMS_TO_TICKS(10);
    while (1) {
        tud_task();
        vTaskDelay(timeout_tick);
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}
