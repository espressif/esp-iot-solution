/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_log.h"
#include "usb_device_uvc.h"
#include "usb_cam.h"

void app_main(void)
{
    ESP_ERROR_CHECK(usb_cam1_init());
#if CONFIG_UVC_SUPPORT_TWO_CAM
    ESP_ERROR_CHECK(usb_cam2_init());
#endif
    ESP_ERROR_CHECK(uvc_device_init());
}
