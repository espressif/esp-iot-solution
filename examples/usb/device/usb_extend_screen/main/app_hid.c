/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_usb.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tusb.h"
#include "device/usbd.h"
#include "tusb_config.h"
#include "usb_descriptors.h"

static const char *TAG = "tinyusb_hid.h";

typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t hid_queue;
} tinyusb_hid_t;

static tinyusb_hid_t *s_tinyusb_hid = NULL;

//--------------------------------------------------------------------+
// HID callbacks
//--------------------------------------------------------------------+
#if CFG_TUD_HID

void tinyusb_hid_keyboard_report(hid_report_t report)
{
    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        xQueueSend(s_tinyusb_hid->hid_queue, &report, 0);
    }
}

// tinyusb_hid_task function to process the HID reports
static void tinyusb_hid_task(void *arg)
{
    (void) arg;
    hid_report_t report;
    while (1) {
        if (xQueueReceive(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY)) {
            // Remote wakeup
            if (tud_suspended()) {
                // Wake up host if we are in suspend mode
                // and REMOTE_WAKEUP feature is enabled by host
                tud_remote_wakeup();
                xQueueReset(s_tinyusb_hid->hid_queue);
            } else {
                if (report.report_id == REPORT_ID_TOUCH) {
                    tud_hid_n_report(0, REPORT_ID_TOUCH, &report.touch_report, sizeof(report.touch_report));
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
#endif

esp_err_t app_hid_init(void)
{
    if (s_tinyusb_hid) {
        ESP_LOGW(TAG, "tinyusb_hid already initialized");
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    s_tinyusb_hid = calloc(1, sizeof(tinyusb_hid_t));
    ESP_RETURN_ON_FALSE(s_tinyusb_hid, ESP_ERR_NO_MEM, TAG, "calloc failed");
    s_tinyusb_hid->hid_queue = xQueueCreate(10, sizeof(hid_report_t));   // Adjust queue length and item size as per your requirement
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->hid_queue, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");

    xTaskCreate(tinyusb_hid_task, "tinyusb_hid_task", 4096, NULL, CONFIG_HID_TASK_PRIORITY, &s_tinyusb_hid->task_handle);
    /*!< Make sure tinyusb_hid_task create successfully */
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->task_handle, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");
    xTaskNotifyGive(s_tinyusb_hid->task_handle);
    return ESP_OK;
fail:
    free(s_tinyusb_hid);
    s_tinyusb_hid = NULL;
    return ret;
}

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

    switch (report_id) {
    case REPORT_ID_MAX_COUNT: {
        buffer[0] = CONFIG_ESP_LCD_TOUCH_MAX_POINTS;
        return 1;
    }
    default: {
        break;
    }
    }

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

    switch (report_id) {
    default: {
        break;
    }
    }
}
