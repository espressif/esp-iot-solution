/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "app_usb_host.h"
#include "app_uac_manager.h"
#include "app_wifi.h"
#include "app_web.h"

static const char *TAG = "uac_mic_spk_demo";

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(app_usb_host_start());
    ESP_ERROR_CHECK(app_uac_manager_start());
    ESP_ERROR_CHECK(app_wifi_start());
    ESP_ERROR_CHECK(app_web_start());

    ESP_LOGI(TAG, "USB UAC web console is ready at http://192.168.4.1");
}
