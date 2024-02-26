/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tinyusb_hid.h"
#include "usb_descriptors.h"
#include "device/usbd.h"

static const char *TAG = "tinyusb_hid.h";

typedef struct {
    uint32_t report_id;    // Report identifier
    union {
        struct {
            uint8_t button;    // Button value
            int8_t x;          // X coordinate
            int8_t y;          // Y coordinate
            int8_t vertical;   // Vertical scroll
            int8_t horizontal; // Horizontal scroll
        } mouse_report;     // Mouse report
#if CONFIG_ENABLE_FULL_KEY_KEYBOARD
        struct {
            uint8_t modifier;     // Modifier keys
            uint8_t reserved;     // Reserved byte
            uint8_t keycode[15];  // Keycode
        } keyboard_report;  // Keyboard report
#else
        struct {
            uint8_t modifier;   // Modifier keys
            uint8_t reserved;   // Reserved byte
            uint8_t keycode[6]; // Keycodes
        } keyboard_report;  // Keyboard report
#endif
    };
} tinyusb_hid_report_t;

typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t hid_queue;
} tinyusb_hid_t;

static tinyusb_hid_t *s_tinyusb_hid = NULL;

void tinyusb_hid_mouse_move_report(int8_t x, int8_t y, int8_t vertical, int8_t horizontal)
{
    ESP_LOGD(TAG, "x=%d, y=%d, vertical=%d, horizontal=%d", x, y, vertical, horizontal);

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        tinyusb_hid_report_t report = {0};
        report.report_id = REPORT_ID_MOUSE;
        report.mouse_report.x = x;
        report.mouse_report.y = y;
        report.mouse_report.vertical = vertical;
        report.mouse_report.horizontal = horizontal;
        xQueueSend(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY);
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
        tinyusb_hid_report_t report = {0};
        report.report_id = REPORT_ID_MOUSE;
        report.mouse_report.button = buttons_map;
        xQueueSend(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY);
    }
}

#if CONFIG_ENABLE_FULL_KEY_KEYBOARD
void tinyusb_hid_keyboard_report(uint8_t modifier, uint8_t keycode[])
{
    ESP_LOGD(TAG, "Keycode array: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
             keycode[0], keycode[1], keycode[2], keycode[3], keycode[4], keycode[5],
             keycode[6], keycode[7], keycode[8], keycode[9], keycode[10], keycode[11],
             keycode[12], keycode[13], keycode[14]);
    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        tinyusb_hid_report_t report = {0};
        report.report_id = REPORT_ID_KEYBOARD;
        report.keyboard_report.modifier = modifier;
        memcpy(report.keyboard_report.keycode, keycode, sizeof(report.keyboard_report.keycode));
        xQueueSend(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY);
    }
}
#else
void tinyusb_hid_keyboard_report(uint8_t modifier, uint8_t keycode[])
{
    ESP_LOGD(TAG, "keycode = %u %u %u %u %u %u", keycode[0], keycode[1], keycode[2], keycode[3], keycode[4], keycode[5]);
    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        tinyusb_hid_report_t report = {0};
        report.report_id = REPORT_ID_KEYBOARD;
        report.keyboard_report.modifier = modifier;
        memcpy(report.keyboard_report.keycode, keycode, sizeof(report.keyboard_report.keycode));
        xQueueSend(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY);
    }
}
#endif

// tinyusb_hid_task function to process the HID reports
static void tinyusb_hid_task(void *arg)
{
    (void) arg;
    tinyusb_hid_report_t report;
    while (1) {
        if (xQueueReceive(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY)) {
            // Remote wakeup
            if (tud_suspended()) {
                // Wake up host if we are in suspend mode
                // and REMOTE_WAKEUP feature is enabled by host
                tud_remote_wakeup();
                xQueueReset(s_tinyusb_hid->hid_queue);
            } else {
                if (report.report_id == REPORT_ID_MOUSE) {
                    tud_hid_mouse_report(REPORT_ID_MOUSE, report.mouse_report.button, report.mouse_report.x, report.mouse_report.y, report.mouse_report.vertical, report.mouse_report.horizontal);
                } else if (report.report_id == REPORT_ID_KEYBOARD) {
                    tud_hid_n_report(0, REPORT_ID_KEYBOARD, &report.keyboard_report, sizeof(report.keyboard_report));
                } else {
                    // Unknown report
                    continue;
                }
                // Wait until report is sent
                if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
                    ESP_LOGW(TAG, "Report not sent");
                }
            }
        }
    }
}

esp_err_t tinyusb_hid_init(void)
{
    if (s_tinyusb_hid) {
        ESP_LOGW(TAG, "tinyusb_hid already initialized");
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    s_tinyusb_hid = calloc(1, sizeof(tinyusb_hid_t));
    ESP_RETURN_ON_FALSE(s_tinyusb_hid, ESP_ERR_NO_MEM, TAG, "calloc failed");
    s_tinyusb_hid->hid_queue = xQueueCreate(10, sizeof(tinyusb_hid_report_t));   // Adjust queue length and item size as per your requirement
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->hid_queue, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");
    xTaskCreate(tinyusb_hid_task, "tinyusb_hid_task", 4096, NULL, 5, &s_tinyusb_hid->task_handle);
    xTaskNotifyGive(s_tinyusb_hid->task_handle);
    return ret;
fail:
    free(s_tinyusb_hid);
    s_tinyusb_hid = NULL;
    return ret;
}

/************************************************** TinyUSB callbacks ***********************************************/
// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
    (void) itf;
    (void) len;

    xTaskNotifyGive(s_tinyusb_hid->task_handle);
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
