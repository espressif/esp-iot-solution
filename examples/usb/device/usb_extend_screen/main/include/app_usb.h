/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "tusb.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JPEG_BUFFER_SIZE (300*1024)

typedef struct {
    uint8_t press_down;
    uint8_t index;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} __attribute__((packed)) touch_report_t;

_Static_assert(CONFIG_ESP_LCD_TOUCH_MAX_POINTS == 5, "CONFIG_ESP_LCD_TOUCH_MAX_POINTS must be 4");

typedef struct {
    uint32_t report_id;    // Report identifier
    struct {
        touch_report_t data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];  // Touch report
        // uint16_t scan_time;
        uint8_t cnt;
    } touch_report;
} __attribute__((packed)) hid_report_t;

/**
 * @brief Initialize tinyusb device.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: No memory
 */
esp_err_t app_usb_init(void);

#if CFG_TUD_HID
/**
 * @brief Report key press in the keyboard, using array here
 *
 * @param report hid report data
 */
void tinyusb_hid_keyboard_report(hid_report_t report);

esp_err_t app_hid_init(void);
#endif

#if CFG_TUD_VENDOR
esp_err_t app_vendor_init(void);
#endif

#if CFG_TUD_AUDIO
esp_err_t app_uac_init(void);
#endif

#ifdef __cplusplus
}
#endif
