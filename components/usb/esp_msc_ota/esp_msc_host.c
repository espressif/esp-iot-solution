/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_msc_ota_help.h"
#include "usb/usb_host.h"
#include "esp_msc_ota.h"
#include "esp_msc_host.h"

static const char *TAG = "esp_msc_host";

ESP_EVENT_DEFINE_BASE(ESP_MSC_HOST_EVENT);
SemaphoreHandle_t msc_read_semaphore;

typedef struct {
    EventGroupHandle_t usb_flags;
    const char *base_path;
    esp_vfs_fat_mount_config_t mount_config;
} esp_msc_host_t;

static esp_msc_host_t *q_msc_host = NULL;

typedef enum {
    DEVICE_CONNECTED = 0x4,
    DEVICE_DISCONNECTED = 0x8,
    HOST_TOBE_UNINSTALL = 0x10,
    HOST_UNINSTALL = 0x20,
    DEVICE_ADDRESS_MASK = 0x3FC0,
} app_event_t;

// Table to lookup msc host event name
static const char *host_event_name_table[] = {
    "ESP_MSC_HOST_CONNECT",
    "ESP_MSC_HOST_DISCONNECT",
    "ESP_MSC_HOST_INSTALL",
    "ESP_MSC_HOST_UNINSTALL",
    "ESP_MSC_HOST_VFS_REGISTER",
    "ESP_MSC_HOST_VFS_UNREGISTER",
};

static void esp_msc_host_dispatch_event(int32_t event_id, const void *event_data, size_t event_data_size)
{
    if (esp_event_post(ESP_MSC_HOST_EVENT, event_id, event_data, event_data_size, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post msc_host event: %s", host_event_name_table[event_id]);
    }
}

static void print_device_info(msc_host_device_info_t *info)
{
    const size_t megabyte = 1024 * 1024;
    uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;

    printf("Device info:\n");
    printf("\t Capacity: %llu MB\n", capacity);
    printf("\t Sector size: %" PRIu32 "\n", info->sector_size);
    printf("\t Sector count: %" PRIu32 "\n", info->sector_count);
    printf("\t PID: 0x%4X \n", info->idProduct);
    printf("\t VID: 0x%4X \n", info->idVendor);
    wprintf(L"\t iProduct: %S \n", info->iProduct);
    wprintf(L"\t iManufacturer: %S \n", info->iManufacturer);
    wprintf(L"\t iSerialNumber: %S \n", info->iSerialNumber);
}

static void _msc_event_cb(const msc_host_event_t *event, void *arg)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)arg;
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected");
        xEventGroupSetBits(msc_host->usb_flags, DEVICE_CONNECTED | (event->device.address << 6));
        esp_msc_host_dispatch_event(ESP_MSC_HOST_CONNECT, NULL, 0);
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        ESP_LOGI(TAG, "MSC device disconnected");
        xEventGroupSetBits(msc_host->usb_flags, DEVICE_DISCONNECTED);
        esp_msc_host_dispatch_event(ESP_MSC_HOST_DISCONNECT, NULL, 0);
    }
}

static bool wait_for_event(EventGroupHandle_t usb_flags, EventBits_t event, TickType_t timeout)
{
    return xEventGroupWaitBits(usb_flags, event, pdTRUE, pdTRUE, timeout) & event;
}

static void msc_host_task(void *args)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)args;
    esp_err_t err = ESP_OK;
    msc_host_device_handle_t msc_device;
    msc_host_device_info_t info;
    msc_host_vfs_handle_t vfs_handle;
    while (1) {

        ESP_LOGI(TAG, "Waiting for USB stick to be connected");
        EventBits_t bits;
        bits = xEventGroupWaitBits(msc_host->usb_flags, DEVICE_CONNECTED | DEVICE_ADDRESS_MASK | HOST_TOBE_UNINSTALL, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & HOST_TOBE_UNINSTALL) {
            break;
        }
        ESP_LOGI(TAG, "connection...");

        uint8_t device_address = (bits & DEVICE_ADDRESS_MASK) >> 6;
        err = msc_host_install_device(device_address, &msc_device);
        MSC_OTA_CHECK_GOTO(err == ESP_OK, "Failed to install MSC device", install_fail);
        esp_msc_host_dispatch_event(ESP_MSC_HOST_INSTALL, NULL, 0);

        err = msc_host_print_descriptors(msc_device);
        MSC_OTA_CHECK_CONTINUE(err == ESP_OK, "Failed to print descriptors");

        err = msc_host_get_device_info(msc_device, &info);
        MSC_OTA_CHECK_GOTO(err == ESP_OK, "Failed to get device info", host_uninstall);
#ifdef CONFIG_MSC_PRINT_DESC
        print_device_info(&info);
#endif
        err = msc_host_vfs_register(msc_device, msc_host->base_path, &msc_host->mount_config, &vfs_handle);
        MSC_OTA_CHECK_GOTO(err == ESP_OK, "Failed to register VFS", host_uninstall);

        esp_msc_host_dispatch_event(ESP_MSC_HOST_VFS_REGISTER, NULL, 0);
        xSemaphoreGive(msc_read_semaphore);

        while (!wait_for_event(msc_host->usb_flags, DEVICE_DISCONNECTED, 200)) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        // Ensure that no client reads the MSC before unloading the device
        xSemaphoreTake(msc_read_semaphore, portMAX_DELAY);
        err = msc_host_vfs_unregister(vfs_handle);
        esp_msc_host_dispatch_event(ESP_MSC_HOST_VFS_UNREGISTER, NULL, 0);
        MSC_OTA_CHECK_CONTINUE(err == ESP_OK, "Failed to unregister VFS");
host_uninstall:
        err = msc_host_uninstall_device(msc_device);
        MSC_OTA_CHECK_CONTINUE(err == ESP_OK, "Failed to uninstall MSC device");
        esp_msc_host_dispatch_event(ESP_MSC_HOST_UNINSTALL, NULL, 0);
install_fail:
    }

    ESP_LOGI(TAG, "Uninstall USB ...");
    err = msc_host_uninstall();
    MSC_OTA_CHECK_CONTINUE(err == ESP_OK, "Failed to uninstall MSC host");

    vTaskDelete(NULL);
}

// Handles common USB host library events
static void usb_event_task(void *args)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)args;
    bool has_clients = true;
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // Release devices once all clients has deregistered
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            has_clients = false;
            if (usb_host_device_free_all() == ESP_OK) {
                break;
            }
        }
        // can be deinitialized, and terminate this task.
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE && !has_clients) {
            break;
        }
    }

    xEventGroupSetBits(msc_host->usb_flags, HOST_UNINSTALL);
    vTaskDelete(NULL);
}

esp_err_t esp_msc_host_install(esp_msc_host_config_t *config, esp_msc_host_handle_t *handle)
{
    MSC_OTA_CHECK(config != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(handle != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(q_msc_host == NULL, "msc host already install ", ESP_ERR_INVALID_ARG);

    esp_err_t err = ESP_OK;
    esp_msc_host_t *msc_host = calloc(1, sizeof(esp_msc_host_t));

    MSC_OTA_CHECK(msc_host != NULL, "Failed to allocate memory for MSC host", ESP_ERR_NO_MEM);
    msc_host->base_path = config->base_path;
    memcpy(&msc_host->mount_config, &config->vfs_fat_mount_config, sizeof(esp_vfs_fat_mount_config_t));

    // Ensure that there are no read operations from the MSC by the client before unmounting the device.
    msc_read_semaphore = xSemaphoreCreateBinary();
    MSC_OTA_CHECK_GOTO(msc_read_semaphore != NULL, "Failed to create semaphore", install_fail);

    msc_host->usb_flags = xEventGroupCreate();
    MSC_OTA_CHECK_GOTO(msc_host->usb_flags != NULL, "Failed to create event group", install_fail);

    err = usb_host_install(&config->host_config);
    MSC_OTA_CHECK_GOTO(err == ESP_OK, "Failed to install USB host", install_fail);

    MSC_OTA_CHECK_CONTINUE(config->host_driver_config.callback == NULL, "The specified callback will be overridden by the internal callback function");
    msc_host_driver_config_t msc_config = {0};
    memcpy(&msc_config, &config->host_driver_config, sizeof(msc_host_driver_config_t));
    msc_config.callback = _msc_event_cb;
    msc_config.callback_arg = msc_host;

    BaseType_t task_created = xTaskCreate(usb_event_task, "usb_event", 4096, msc_host, 2, NULL);
    MSC_OTA_CHECK_GOTO(task_created == pdPASS, "Failed to create USB events task", usb_install_fail);

    err = msc_host_install(&msc_config);
    MSC_OTA_CHECK_GOTO(err == ESP_OK, "Failed to install MSC host", usb_install_fail);

    task_created = xTaskCreate(msc_host_task, "msc_host", 4096, msc_host, 5, NULL);
    MSC_OTA_CHECK_GOTO(task_created == pdPASS, "Failed to create MSC host task", msc_install_fail);

    *handle = (esp_msc_host_handle_t)msc_host;
    q_msc_host = msc_host;

    ESP_LOGI(TAG, "MSC Host Install Done\n");
    return ESP_OK;

msc_install_fail:
    msc_host_uninstall();
    wait_for_event(msc_host->usb_flags, HOST_UNINSTALL, portMAX_DELAY);

usb_install_fail:
    usb_host_uninstall();

install_fail:
    if (msc_host->usb_flags != NULL) {
        vEventGroupDelete(msc_host->usb_flags);
    }

    if (msc_read_semaphore) {
        vSemaphoreDelete(msc_read_semaphore); // free the semaphore
    }

    free(msc_host);
    return ESP_FAIL;
}

esp_err_t esp_msc_host_uninstall(esp_msc_host_handle_t handle)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    MSC_OTA_CHECK(msc_host != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);
    ESP_LOGI(TAG, "Please remove the USB flash drive.");
    xEventGroupSetBits(msc_host->usb_flags, HOST_TOBE_UNINSTALL);
    wait_for_event(msc_host->usb_flags, HOST_UNINSTALL, portMAX_DELAY);
    usb_host_uninstall();
    if (msc_host->usb_flags != NULL) {
        vEventGroupDelete(msc_host->usb_flags);
    }
    free(msc_host);
    q_msc_host = NULL;
    if (msc_read_semaphore) {
        vSemaphoreDelete(msc_read_semaphore); // free the semaphore
    }
    ESP_LOGI(TAG, "MSC Host Uninstall Done");
    return ESP_OK;
}
