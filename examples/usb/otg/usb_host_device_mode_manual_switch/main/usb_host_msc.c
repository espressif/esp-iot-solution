/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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

static void host_msc_event_handler(esp_msc_host_handle_t handle, esp_msc_host_event_t event_id, void *user_ctx)
{
    switch (event_id) {
    case ESP_MSC_HOST_CONNECT:
        ESP_LOGI(TAG, "MSC device connected");
        break;
    case ESP_MSC_HOST_DISCONNECT:
        ESP_LOGI(TAG, "MSC device disconnected");
        break;
    case ESP_MSC_HOST_DEVICE_INSTALL:
        ESP_LOGI(TAG, "MSC device installed");
        xSemaphoreTake(msc_running, portMAX_DELAY);
        break;
    case ESP_MSC_HOST_DEVICE_UNINSTALL:
        ESP_LOGI(TAG, "MSC device uninstalled");
        xSemaphoreGive(msc_running);
        break;
    default:
        break;
    }
}

static esp_err_t _usb_port_power(bool power)
{
    ESP_LOGI(TAG, "Set root port power: %s", power ? "true" : "false");
    return usb_host_lib_set_root_port_power(power);
}

esp_err_t host_msc_init(void)
{
    _usb_port_power(true);

    if (msc_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    msc_running = xSemaphoreCreateBinary();
    assert(msc_running);
    xSemaphoreGive(msc_running);

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG(),
        .event_cb = host_msc_event_handler,
    };
    esp_msc_host_install(&msc_host_config, &msc_handle);
    return ESP_OK;
}

esp_err_t host_msc_deinit(void)
{
    if (msc_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    _usb_port_power(false);

    if (xSemaphoreTake(msc_running, pdMS_TO_TICKS(3000)) != pdTRUE) {
        ESP_LOGW(TAG, "MSC device is still active, unplug it before switching mode");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_msc_host_uninstall(msc_handle);
    xSemaphoreGive(msc_running);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to uninstall MSC host");

    vSemaphoreDelete(msc_running);
    msc_running = NULL;
    msc_handle = NULL;
    return ESP_OK;
}
