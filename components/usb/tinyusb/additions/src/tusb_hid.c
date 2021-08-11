// Copyright 2020-2021 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdint.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "tusb_hid.h"
#include "descriptors_control.h"

static const char *TAG = "tusb_hid";
static bool s_keyboard_pressed = false;

void tinyusb_hid_mouse_move_report(int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
    ESP_LOGD(TAG, "x=%d, y=%d, vertical=%d, horizontal=%d", x, y, vertical, horizontal);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
        // skip if hid is not ready yet
        if (!tud_hid_ready()) {
            ESP_LOGW(TAG, "tinyusb not ready %s %d", __func__, __LINE__);
            return;
        }

        tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, x, y, vertical, horizontal);

    }
}

void tinyusb_hid_mouse_button_report(uint8_t buttons_map)
{
    ESP_LOGD(TAG, "buttons_map = %u", buttons_map);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
        // skip if hid is not ready yet
        if (!tud_hid_ready()) {
            ESP_LOGW(TAG, "tinyusb not ready %s %d", __func__, __LINE__);
            return;
        }

        tud_hid_mouse_report(REPORT_ID_MOUSE, buttons_map, 0, 0, 0, 0);

    }
}

void tinyusb_hid_keyboard_report(uint8_t keycode[])
{
    ESP_LOGD(TAG, "keycode = %u %u %u %u %u %u", keycode[0], keycode[1], keycode[2], keycode[3], keycode[4], keycode[5]);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
        // skip if hid is not ready yet
        if (!tud_hid_ready()) {
            ESP_LOGW(TAG, "tinyusb not ready %s %d", __func__, __LINE__);
            return;
        }

        uint8_t _keycode[6] = { 0 };
        _keycode[0] = keycode[0];
        _keycode[1] = keycode[1];
        _keycode[2] = keycode[2];
        _keycode[3] = keycode[3];
        _keycode[4] = keycode[4];
        _keycode[5] = keycode[5];

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, _keycode);
        s_keyboard_pressed = true;
    }
}

/************************************************** TinyUSB callbacks ***********************************************/
// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint8_t len)
{
    (void) itf;
    (void) len;
    uint8_t report_id = report[0];

    if (report_id == REPORT_ID_KEYBOARD && s_keyboard_pressed) {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        s_keyboard_pressed = false;
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    // TODO set LED based on CAPLOCK, NUMLOCK etc...
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}
