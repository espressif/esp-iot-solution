/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_log.h"
#include "usb_device_uvc.h"
#include "usb_cam.h"
#include "esp_spiffs.h"
#ifdef CONFIG_ESP32_S3_USB_OTG
#include "bsp/esp-bsp.h"
#endif

void app_main(void)
{
#ifdef CONFIG_ESP32_S3_USB_OTG
    bsp_usb_mode_select_device();
#endif

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "avi",
        .max_files = 5,   // This decides the maximum number of files that can be created on the storage
        .format_if_mount_failed = false
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    ESP_ERROR_CHECK(usb_cam1_init());
#if CONFIG_UVC_SUPPORT_TWO_CAM
    ESP_ERROR_CHECK(usb_cam2_init());
#endif
    ESP_ERROR_CHECK(uvc_device_init());
}
