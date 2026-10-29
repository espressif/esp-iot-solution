/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "app_usb.h"
#include "usb_descriptors.h"
#include "esp_log.h"
#include "esp_board_manager.h"
#if CONFIG_HID_TOUCH_ENABLE
#include "app_touch.h"
#endif
#include "app_lcd.h"

static const char *TAG = "usb_extend_screen";

void app_main(void)
{
    ESP_LOGI(TAG, "USB extend screen example");
    ESP_ERROR_CHECK(esp_board_manager_print_board_info());
    ESP_ERROR_CHECK(app_lcd_init());
    ESP_ERROR_CHECK(app_usb_init());
#if CONFIG_HID_TOUCH_ENABLE
    ESP_ERROR_CHECK(app_touch_init());
#endif
}
