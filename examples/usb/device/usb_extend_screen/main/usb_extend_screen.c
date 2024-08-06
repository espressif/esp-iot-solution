/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "app_usb.h"
#include "usb_descriptors.h"
#include "esp_log.h"
#include "app_touch.h"
#include "app_lcd.h"

static const char *TAG = "usb_extend_screen";

void app_main(void)
{
    ESP_LOGI(TAG, "USB extend screen example");
    app_usb_init();
    app_lcd_init();
    app_touch_init();
}
