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
#include "esp_private/usb_phy.h"
#include "tinyusb_hid.h"
#include "usb_descriptors.h"
#include "device/usbd.h"
#include "lamp_array_matrix.h"

static const char *TAG = "tinyusb_hid.h";

typedef struct {
    TaskHandle_t task_handle;
    QueueHandle_t hid_queue;
} tinyusb_hid_t;

static tinyusb_hid_t *s_tinyusb_hid = NULL;

//--------------------------------------------------------------------+
// USB PHY config
//--------------------------------------------------------------------+
static void usb_phy_init(void)
{
    usb_phy_handle_t phy_hdl;
    // Configure USB PHY
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &phy_hdl);
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

void tinyusb_hid_keyboard_report(hid_report_t report)
{
    static bool use_full_key = false;
    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    } else {
        switch (report.report_id) {
        case REPORT_ID_FULL_KEY_KEYBOARD:
            use_full_key = true;
            break;
        case REPORT_ID_KEYBOARD: {
            if (use_full_key) {
                hid_report_t _report = {0};
                _report.report_id = REPORT_ID_FULL_KEY_KEYBOARD;
                xQueueSend(s_tinyusb_hid->hid_queue, &_report, 0);
                use_full_key = false;
            }
            break;
        }
        default:
            break;
        }

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
                if (report.report_id == REPORT_ID_KEYBOARD) {
                    tud_hid_n_report(0, REPORT_ID_KEYBOARD, &report.keyboard_report, sizeof(report.keyboard_report));
                } else if (report.report_id == REPORT_ID_FULL_KEY_KEYBOARD) {
                    tud_hid_n_report(0, REPORT_ID_FULL_KEY_KEYBOARD, &report.keyboard_full_key_report, sizeof(report.keyboard_full_key_report));
                } else if (report.report_id == REPORT_ID_CONSUMER) {
                    tud_hid_n_report(0, REPORT_ID_CONSUMER, &report.consumer_report, sizeof(report.consumer_report));
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

    usb_phy_init();
    tud_init(BOARD_TUD_RHPORT);

    s_tinyusb_hid->hid_queue = xQueueCreate(10, sizeof(hid_report_t));   // Adjust queue length and item size as per your requirement
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->hid_queue, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");
    xTaskCreate(tusb_device_task, "TinyUSB", 4096, NULL, 5, NULL);
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

    switch (report_id) {
    case REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES: {
        return GetLampArrayAttributesReport(buffer);
        break;
    }
    case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE: {
        return GetLampAttributesReport(buffer);
        break;
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
    case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST: {
        SetLampAttributesId(buffer);
        break;
    }
    case REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE: {
        SetMultipleLamps(buffer);
        break;
    }
    case REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE: {
        SetLampRange(buffer);
        break;
    }
    case REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL: {
        SetAutonomousMode(buffer);
        break;
    }
    default: {
        break;
    }
    }
}

// Invoked when device is mounted
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Mount");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Un-Mount");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    ESP_LOGI(TAG, "USB Suspend");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resume");
}
