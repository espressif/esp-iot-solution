/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_msc_host.h"
#include "usb/usb_host.h"

static const char *TAG = "usb_host_msc";
static esp_msc_host_handle_t msc_handle = NULL;
static SemaphoreHandle_t msc_running = NULL;

static void host_msc_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MSC_HOST_CONNECT:
        ESP_LOGI(TAG, "MSC device connected");
        break;
    case ESP_MSC_HOST_DISCONNECT:
        ESP_LOGI(TAG, "MSC device disconnected");
        break;
    case ESP_MSC_HOST_INSTALL:
        ESP_LOGI(TAG, "MSC device installed");
        xSemaphoreTake(msc_running, portMAX_DELAY);
        break;
    case ESP_MSC_HOST_UNINSTALL:
        ESP_LOGI(TAG, "MSC device uninstalled");
        xSemaphoreGive(msc_running);
        break;
    default:
        break;
    }
}

esp_err_t host_msc_init(void)
{
    if (msc_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    msc_running = xSemaphoreCreateBinary();
    assert(msc_running);
    xSemaphoreGive(msc_running);

    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_HOST_EVENT, ESP_EVENT_ANY_ID, &host_msc_event_handler, NULL));
    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG()
    };
    esp_msc_host_install(&msc_host_config, &msc_handle);
    return ESP_OK;
}

esp_err_t host_msc_deinit(void)
{
    if (msc_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_msc_host_uninstall(msc_handle);
    xSemaphoreTake(msc_running, portMAX_DELAY);
    xSemaphoreGive(msc_running);
    msc_handle = NULL;
    return ESP_OK;
}
