// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "board.h"
#include "tusb_hid.h"

#define TAG "HID Example"

#define EXAMPLE_MOUSE_OFFSET_X 8
#define EXAMPLE_MOUSE_OFFSET_Y 8

#define EXAMPLE_BUTTON_NUM 4
#ifdef CONFIG_BOARD_ESP32S3_USB_OTG_EV
#define EXAMPLE_BUTTON_UP BOARD_IO_BUTTON_UP
#define EXAMPLE_BUTTON_DOWN BOARD_IO_BUTTON_DW
#define EXAMPLE_BUTTON_LEFT BOARD_IO_BUTTON_OK
#define EXAMPLE_BUTTON_RIGHT BOARD_IO_BUTTON_MENU
#else
#define EXAMPLE_BUTTON_UP 10
#define EXAMPLE_BUTTON_DOWN 11
#define EXAMPLE_BUTTON_LEFT 0
#define EXAMPLE_BUTTON_RIGHT 14
#endif

static int s_button_gpio[EXAMPLE_BUTTON_NUM] = {EXAMPLE_BUTTON_UP, EXAMPLE_BUTTON_DOWN, EXAMPLE_BUTTON_LEFT, EXAMPLE_BUTTON_RIGHT};
static button_handle_t s_button_handles[EXAMPLE_BUTTON_NUM] = {NULL};

static int get_button_gpio(button_handle_t btn_hdl)
{
    for (size_t i = 0; i < EXAMPLE_BUTTON_NUM; i++) {
        if (s_button_handles[i] == btn_hdl) {
            return s_button_gpio[i];
        }
    }
    return -1;
}

#ifdef CONFIG_SUBCLASS_KEYBOARD
static void button_keyboard_cb(void *arg)
{
    button_handle_t button_hdl = (button_handle_t)arg;
    int button_gpio = get_button_gpio(button_hdl);
    uint8_t _keycode[6] = { 0 };
    switch (button_gpio) {
        case EXAMPLE_BUTTON_UP:
            _keycode[0] = HID_KEY_U;
            break;

        case EXAMPLE_BUTTON_DOWN:
            _keycode[0] = HID_KEY_D;
            break;

        case EXAMPLE_BUTTON_LEFT:
            _keycode[0] = HID_KEY_L;
            break;

        case EXAMPLE_BUTTON_RIGHT:
            _keycode[0] = HID_KEY_R;
            break;

        default:
            break;
    }
    tinyusb_hid_keyboard_report(_keycode);
    ESP_LOGI(TAG, "Keyboard %c", _keycode[0] - HID_KEY_A + 'a');
}
#else if CONFIG_SUBCLASS_MOUSE
static void button_mouse_cb(void *arg)
{
    button_handle_t button_hdl = (button_handle_t)arg;
    int button_gpio = get_button_gpio(button_hdl);
    int mouse_offset_x = 0, mouse_offset_y = 0;
    switch (button_gpio) {
        case EXAMPLE_BUTTON_UP:
            mouse_offset_y = -EXAMPLE_MOUSE_OFFSET_Y;
            break;

        case EXAMPLE_BUTTON_DOWN:
            mouse_offset_y = EXAMPLE_MOUSE_OFFSET_Y;
            break;

        case EXAMPLE_BUTTON_LEFT:
            mouse_offset_x = -EXAMPLE_MOUSE_OFFSET_X;
            break;

        case EXAMPLE_BUTTON_RIGHT:
            mouse_offset_x = EXAMPLE_MOUSE_OFFSET_X;
            break;

        default:
            break;
    }
    tinyusb_hid_mouse_move_report(mouse_offset_x, mouse_offset_y, 0, 0);
    ESP_LOGI(TAG, "Mouse x=%d y=%d", mouse_offset_x, mouse_offset_y);
}
#endif

void app_main(void)
{
    /* board init, peripherals will be initialized based on menuconfig */
    iot_board_init();

#ifdef CONFIG_BOARD_ESP32S3_USB_OTG_EV
    iot_board_usb_set_mode(USB_DEVICE_MODE);
    iot_board_usb_device_set_power(false, false);
#endif

    /* buttons init, buttons used to simulate keyboard or mouse events */
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .active_level = 0,
        },
    };

    for (size_t i = 0; i < EXAMPLE_BUTTON_NUM; i++) {
        cfg.gpio_button_config.gpio_num = s_button_gpio[i];
        s_button_handles[i] = iot_button_create(&cfg);
        if(s_button_handles[i] == NULL) {
            ESP_LOGE(TAG, "button=%d created failed", s_button_gpio[i]);
            assert(0);
        }
    }

    /* Install tinyusb driver, Please enable `CONFIG_TINYUSB_HID_ENABLED` in menuconfig */
    tinyusb_config_t tusb_cfg = {
        .descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false // In the most cases you need to use a `false` value
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

#ifdef CONFIG_SUBCLASS_KEYBOARD
    button_cb_t button_cb = button_keyboard_cb;
    ESP_LOGI(TAG, "Keyboard demo");
#else if CONFIG_SUBCLASS_MOUSE
    button_cb_t button_cb = button_mouse_cb;
    ESP_LOGI(TAG, "Mouse demo");
#endif

    /* register button callback, send HID report when click button */
    for (size_t i = 0; i < EXAMPLE_BUTTON_NUM; i++) {
        iot_board_button_register_cb(s_button_handles[i], BUTTON_SINGLE_CLICK, button_cb);
    }

    while (1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}