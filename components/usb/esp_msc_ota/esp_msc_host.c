/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_check.h"
#include "usb/usb_host.h"
#include "esp_msc_host.h"

static const char *TAG = "esp_msc_host";

#define MSC_HOST_UNMOUNT_WAIT_TICKS       pdMS_TO_TICKS(5000)
#define MSC_HOST_STATE_MOUNTED_BIT        BIT0
#define MSC_HOST_STATE_DEV_CONNECTED_BIT  BIT1
#define MSC_HOST_STATE_DEV_INSTALLED_BIT  BIT2
#define MSC_HOST_STATE_MSC_TASK_DONE_BIT  BIT3
#define MSC_HOST_STATE_USB_TASK_DONE_BIT  BIT4
#define MSC_HOST_STATE_ACTIVE_MASK        (MSC_HOST_STATE_MOUNTED_BIT      \
                                           | MSC_HOST_STATE_DEV_CONNECTED_BIT \
                                           | MSC_HOST_STATE_DEV_INSTALLED_BIT)

typedef enum {
    MSC_HOST_MSG_CONNECTED,
    MSC_HOST_MSG_DISCONNECTED,
    MSC_HOST_MSG_STOP,
} esp_msc_host_msg_id_t;

typedef struct {
    esp_msc_host_msg_id_t id;
    uint8_t device_address;
} esp_msc_host_msg_t;

typedef struct {
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t idle_sem;
    uint32_t ref_count;
    bool mounted;
    bool unmount_pending;
} msc_host_access_guard_t;

struct esp_msc_host_ctx {
    QueueHandle_t event_queue;
    EventGroupHandle_t state_events;
    msc_host_access_guard_t access_guard;
    const char *base_path;
    usb_host_config_t host_config;
    esp_vfs_fat_mount_config_t mount_config;
    esp_msc_host_event_cb_t event_cb;
    void *event_cb_arg;
    bool skip_init_usb_host_driver;
};

typedef struct esp_msc_host_ctx esp_msc_host_t;

static void access_idle_take_if_available(msc_host_access_guard_t *guard)
{
    xSemaphoreTake(guard->idle_sem, 0);
}

static void access_idle_give(msc_host_access_guard_t *guard)
{
    xSemaphoreGive(guard->idle_sem);
}

static void access_guard_deinit(msc_host_access_guard_t *guard);

static esp_err_t access_guard_init(msc_host_access_guard_t *guard)
{
    ESP_RETURN_ON_FALSE(guard != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    memset(guard, 0, sizeof(*guard));
    guard->mutex = xSemaphoreCreateMutex();
    guard->idle_sem = xSemaphoreCreateBinary();
    if (guard->idle_sem) {
        access_idle_give(guard);
    }

    if (guard->mutex == NULL || guard->idle_sem == NULL) {
        access_guard_deinit(guard);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void access_guard_deinit(msc_host_access_guard_t *guard)
{
    if (guard == NULL) {
        return;
    }

    if (guard->mutex) {
        vSemaphoreDelete(guard->mutex);
    }
    if (guard->idle_sem) {
        vSemaphoreDelete(guard->idle_sem);
    }
    memset(guard, 0, sizeof(*guard));
}

static esp_err_t access_guard_mount(msc_host_access_guard_t *guard)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->mutex != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(guard->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "Failed to lock access guard");

    guard->mounted = true;
    guard->unmount_pending = false;
    if (guard->ref_count == 0) {
        access_idle_give(guard);
    }

    xSemaphoreGive(guard->mutex);
    return ESP_OK;
}

static esp_err_t access_guard_request_unmount(msc_host_access_guard_t *guard)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->mutex != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(guard->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "Failed to lock access guard");

    guard->mounted = false;
    guard->unmount_pending = true;
    if (guard->ref_count == 0) {
        access_idle_give(guard);
    }

    xSemaphoreGive(guard->mutex);
    return ESP_OK;
}

static esp_err_t access_guard_complete_unmount(msc_host_access_guard_t *guard)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->mutex != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(guard->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "Failed to lock access guard");

    guard->mounted = false;
    guard->unmount_pending = false;
    guard->ref_count = 0;
    access_idle_give(guard);

    xSemaphoreGive(guard->mutex);
    return ESP_OK;
}

static esp_err_t access_guard_wait_idle(msc_host_access_guard_t *guard, TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->idle_sem != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    if (xSemaphoreTake(guard->idle_sem, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    access_idle_give(guard);
    return ESP_OK;
}

static esp_err_t access_guard_acquire(msc_host_access_guard_t *guard, TickType_t timeout)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->mutex != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(guard->mutex, timeout) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "Failed to lock access guard");

    if (!guard->mounted || guard->unmount_pending) {
        xSemaphoreGive(guard->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (guard->ref_count == 0) {
        access_idle_take_if_available(guard);
    }
    guard->ref_count++;

    xSemaphoreGive(guard->mutex);
    return ESP_OK;
}

static esp_err_t access_guard_release(msc_host_access_guard_t *guard)
{
    ESP_RETURN_ON_FALSE(guard != NULL && guard->mutex != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(xSemaphoreTake(guard->mutex, portMAX_DELAY) == pdTRUE, ESP_ERR_TIMEOUT, TAG, "Failed to lock access guard");

    if (guard->ref_count == 0) {
        xSemaphoreGive(guard->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    guard->ref_count--;
    if (guard->ref_count == 0) {
        access_idle_give(guard);
    }

    xSemaphoreGive(guard->mutex);
    return ESP_OK;
}

static void esp_msc_host_dispatch_event(esp_msc_host_t *msc_host, esp_msc_host_event_t event_id)
{
    if (msc_host->event_cb) {
        msc_host->event_cb((esp_msc_host_handle_t)msc_host, event_id, msc_host->event_cb_arg);
    }
}

static void print_device_info(msc_host_device_info_t *info)
{
    const size_t megabyte = 1024 * 1024;
    uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;

    printf("----\n");
    printf("Device info:\n");
    printf("\t Capacity: %llu MB\n", capacity);
    printf("\t Sector size: %" PRIu32 "\n", info->sector_size);
    printf("\t Sector count: %" PRIu32 "\n", info->sector_count);
    printf("\t PID: 0x%4X \n", info->idProduct);
    printf("\t VID: 0x%4X \n", info->idVendor);
    wprintf(L"\t iProduct: %ls \n", info->iProduct);
    wprintf(L"\t iManufacturer: %ls \n", info->iManufacturer);
    wprintf(L"\t iSerialNumber: %ls \n", info->iSerialNumber);
    printf("----\n");
}

static void _msc_event_cb(const msc_host_event_t *event, void *arg)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)arg;
    if (event->event == MSC_DEVICE_CONNECTED) {
        if (msc_host->state_events) {
            xEventGroupSetBits(msc_host->state_events, MSC_HOST_STATE_DEV_CONNECTED_BIT);
        }
        esp_msc_host_msg_t msg = {
            .id = MSC_HOST_MSG_CONNECTED,
            .device_address = event->device.address,
        };
        if (xQueueSend(msc_host->event_queue, &msg, 0) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue MSC device connected event");
        }
        esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_CONNECT);
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        if (msc_host->state_events) {
            xEventGroupClearBits(msc_host->state_events,
                                 MSC_HOST_STATE_DEV_CONNECTED_BIT | MSC_HOST_STATE_MOUNTED_BIT);
        }
        access_guard_request_unmount(&msc_host->access_guard);
        esp_msc_host_msg_t msg = {
            .id = MSC_HOST_MSG_DISCONNECTED,
        };
        if (xQueueSend(msc_host->event_queue, &msg, 0) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue MSC device disconnected event");
        }
        esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_DISCONNECT);
    }
}

static void msc_host_task(void *args)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)args;
    msc_host_device_handle_t msc_device = NULL;
    msc_host_vfs_handle_t vfs_handle = NULL;
    bool device_installed = false;
    bool vfs_registered = false;
    bool stop_requested = false;

    ESP_LOGI(TAG, "Waiting for USB stick to be connected");

    for (;;) {
        esp_msc_host_msg_t msg;
        xQueueReceive(msc_host->event_queue, &msg, portMAX_DELAY);

        switch (msg.id) {
        case MSC_HOST_MSG_STOP:
            stop_requested = true;
            goto teardown;

        case MSC_HOST_MSG_CONNECTED:
            if (device_installed) {
                ESP_LOGW(TAG, "Ignore connected event while MSC device is installed");
                continue;
            }
            ESP_LOGI(TAG, "connection...");

            if (msc_host_install_device(msg.device_address, &msc_device) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to install MSC device");
                goto teardown;
            }
            device_installed = true;
            if (msc_host->state_events) {
                xEventGroupSetBits(msc_host->state_events, MSC_HOST_STATE_DEV_INSTALLED_BIT);
            }
            esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_DEVICE_INSTALL);

#ifdef CONFIG_MSC_PRINT_DESC
            msc_host_print_descriptors(msc_device);
#endif
            {
                msc_host_device_info_t info;
                if (msc_host_get_device_info(msc_device, &info) == ESP_OK) {
                    print_device_info(&info);
                }
            }

            if (msc_host_vfs_register(msc_device, msc_host->base_path,
                                      &msc_host->mount_config, &vfs_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to register VFS");
                goto teardown;
            }
            vfs_registered = true;

            if (access_guard_mount(&msc_host->access_guard) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to mark MSC VFS mounted");
                goto teardown;
            }
            if (msc_host->state_events) {
                xEventGroupSetBits(msc_host->state_events, MSC_HOST_STATE_MOUNTED_BIT);
            }
            esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_VFS_REGISTER);
            continue;

        case MSC_HOST_MSG_DISCONNECTED:
            if (!device_installed) {
                ESP_LOGW(TAG, "Ignore disconnected event while waiting for USB stick");
                continue;
            }
            goto teardown;
        }

teardown:
        /* Idempotent unwind: each block is guarded by its own flag so this
         * label handles both partial-open failures on CONNECT and full
         * unmount on DISCONNECT. */
        if (vfs_registered) {
            if (access_guard_request_unmount(&msc_host->access_guard) == ESP_OK) {
                if (access_guard_wait_idle(&msc_host->access_guard, MSC_HOST_UNMOUNT_WAIT_TICKS) != ESP_OK) {
                    ESP_LOGE(TAG, "Timed out waiting for MSC VFS users, force unregister");
                }
            } else {
                ESP_LOGE(TAG, "Failed to request MSC VFS unmount");
            }
            if (msc_host->state_events) {
                xEventGroupClearBits(msc_host->state_events, MSC_HOST_STATE_MOUNTED_BIT);
            }
            if (msc_host_vfs_unregister(vfs_handle) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to unregister VFS");
            }
            access_guard_complete_unmount(&msc_host->access_guard);
            esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_VFS_UNREGISTER);
            vfs_handle = NULL;
            vfs_registered = false;
        }

        if (device_installed) {
            if (msc_host_uninstall_device(msc_device) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to uninstall MSC device");
            }
            if (msc_host->state_events) {
                xEventGroupClearBits(msc_host->state_events, MSC_HOST_STATE_DEV_INSTALLED_BIT);
            }
            esp_msc_host_dispatch_event(msc_host, ESP_MSC_HOST_DEVICE_UNINSTALL);
            msc_device = NULL;
            device_installed = false;
        }

        // Finish task shutdown only after any installed MSC device and VFS resources are unwound.
        if (stop_requested) {
            xEventGroupSetBits(msc_host->state_events, MSC_HOST_STATE_MSC_TASK_DONE_BIT);
            vTaskDelete(NULL);
            return;
        }

    }
}

// Handles common USB host library events
static void usb_event_task(void *args)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)args;

    bool has_clients = true;
    bool has_devices = false;
    while (has_clients) {
        uint32_t event_flags;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGI(TAG, "Get FLAGS_NO_CLIENTS");
            if (ESP_OK == usb_host_device_free_all()) {
                ESP_LOGI(TAG, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
                has_clients = false;
            } else {
                ESP_LOGI(TAG, "Wait for the FLAGS_ALL_FREE");
                has_devices = true;
            }
        }
        if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "Get FLAGS_ALL_FREE");
            has_clients = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices, uninstall USB Host library");

    // Clean up USB Host
    vTaskDelay(100); // Short delay to allow clients clean-up
    ESP_ERROR_CHECK(usb_host_uninstall());
    ESP_LOGD(TAG, "USB Host library is uninstalled");
    xEventGroupSetBits(msc_host->state_events, MSC_HOST_STATE_USB_TASK_DONE_BIT);
    vTaskDelete(NULL);
}

esp_err_t esp_msc_host_install(esp_msc_host_config_t *config, esp_msc_host_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    esp_err_t ret = ESP_OK;
    bool usb_event_task_started = false;
    bool usb_host_installed = false;
    bool msc_driver_installed = false;
    esp_msc_host_t *msc_host = calloc(1, sizeof(esp_msc_host_t));

    ESP_RETURN_ON_FALSE(msc_host != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for MSC host");
    msc_host->base_path = config->base_path;
    memcpy(&msc_host->mount_config, &config->vfs_fat_mount_config, sizeof(esp_vfs_fat_mount_config_t));
    msc_host->event_cb = config->event_cb;
    msc_host->event_cb_arg = config->event_cb_arg;
    msc_host->skip_init_usb_host_driver = config->skip_init_usb_host_driver;

    ESP_GOTO_ON_ERROR(access_guard_init(&msc_host->access_guard), install_fail, TAG, "Failed to create access guard");

    msc_host->event_queue = xQueueCreate(4, sizeof(esp_msc_host_msg_t));
    ESP_GOTO_ON_FALSE(msc_host->event_queue != NULL, ESP_ERR_NO_MEM, install_fail, TAG, "Failed to create event queue");
    msc_host->state_events = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(msc_host->state_events != NULL, ESP_ERR_NO_MEM, install_fail, TAG, "Failed to create state event group");

    msc_host->host_config = config->host_config;

    if (config->host_driver_config.callback != NULL) {
        ESP_LOGW(TAG, "The specified callback will be overridden by the internal callback function");
    }
    msc_host_driver_config_t msc_config = {0};
    memcpy(&msc_config, &config->host_driver_config, sizeof(msc_host_driver_config_t));
    msc_config.callback = _msc_event_cb;
    msc_config.callback_arg = msc_host;

    BaseType_t task_created = pdPASS;
    if (!config->skip_init_usb_host_driver) {
        ESP_GOTO_ON_ERROR(usb_host_install(&msc_host->host_config), install_fail, TAG, "Failed to install USB Host library");
        usb_host_installed = true;
        task_created = xTaskCreate(usb_event_task, "usb_event", 4096, msc_host, 2, NULL);
        ESP_GOTO_ON_FALSE(task_created == pdPASS, ESP_FAIL, install_fail, TAG, "Failed to create USB events task");
        usb_event_task_started = true;
    }

    ESP_GOTO_ON_ERROR(msc_host_install(&msc_config), install_fail, TAG, "Failed to install MSC host");
    msc_driver_installed = true;

    task_created = xTaskCreate(msc_host_task, "msc_host", 4096, msc_host, 5, NULL);
    ESP_GOTO_ON_FALSE(task_created == pdPASS, ESP_FAIL, msc_install_fail, TAG, "Failed to create MSC host task");

    *handle = (esp_msc_host_handle_t)msc_host;

    ESP_LOGI(TAG, "MSC Host Install Done");
    return ESP_OK;

msc_install_fail:
    if (msc_driver_installed) {
        msc_host_uninstall();
    }
    if (usb_event_task_started) {
        xEventGroupWaitBits(msc_host->state_events, MSC_HOST_STATE_USB_TASK_DONE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        usb_event_task_started = false;
        usb_host_installed = false;
    }

install_fail:
    if (usb_event_task_started) {
        xEventGroupWaitBits(msc_host->state_events, MSC_HOST_STATE_USB_TASK_DONE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        usb_event_task_started = false;
        usb_host_installed = false;
    } else if (usb_host_installed) {
        esp_err_t uninstall_ret = usb_host_uninstall();
        if (uninstall_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to uninstall USB Host library");
        }
    }
    if (msc_host->event_queue != NULL) {
        vQueueDelete(msc_host->event_queue);
    }
    if (msc_host->state_events != NULL) {
        vEventGroupDelete(msc_host->state_events);
    }

    access_guard_deinit(&msc_host->access_guard);

    free(msc_host);
    return ret;
}

esp_err_t esp_msc_host_uninstall(esp_msc_host_handle_t handle)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    ESP_RETURN_ON_FALSE(msc_host != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(msc_host->state_events != NULL, ESP_ERR_INVALID_STATE, TAG, "Host not initialised");
    ESP_RETURN_ON_FALSE(!(xEventGroupGetBits(msc_host->state_events) & MSC_HOST_STATE_ACTIVE_MASK),
                        ESP_ERR_INVALID_STATE, TAG, "MSC device is still active");
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Uninstall MSC host");
    esp_msc_host_msg_t msg = {
        .id = MSC_HOST_MSG_STOP,
    };
    xQueueSend(msc_host->event_queue, &msg, portMAX_DELAY);
    xEventGroupWaitBits(msc_host->state_events, MSC_HOST_STATE_MSC_TASK_DONE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_RETURN_ON_ERROR(msc_host_uninstall(), TAG, "Failed to uninstall MSC host");

    if (!msc_host->skip_init_usb_host_driver) {
        xEventGroupWaitBits(msc_host->state_events, MSC_HOST_STATE_USB_TASK_DONE_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }

    if (msc_host->event_queue != NULL) {
        vQueueDelete(msc_host->event_queue);
    }
    if (msc_host->state_events != NULL) {
        vEventGroupDelete(msc_host->state_events);
    }
    access_guard_deinit(&msc_host->access_guard);
    free(msc_host);
    ESP_LOGI(TAG, "MSC Host Uninstall Done");
    return ret;
}

bool esp_msc_host_is_mounted(esp_msc_host_handle_t handle)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    if (msc_host == NULL || msc_host->state_events == NULL) {
        return false;
    }

    return (xEventGroupGetBits(msc_host->state_events) & MSC_HOST_STATE_MOUNTED_BIT) != 0;
}

esp_err_t esp_msc_host_wait_mounted(esp_msc_host_handle_t handle, TickType_t timeout)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    ESP_RETURN_ON_FALSE(msc_host != NULL && msc_host->state_events != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    EventBits_t bits = xEventGroupWaitBits(msc_host->state_events, MSC_HOST_STATE_MOUNTED_BIT, pdFALSE, pdTRUE, timeout);
    return (bits & MSC_HOST_STATE_MOUNTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t esp_msc_host_lock(esp_msc_host_handle_t handle, TickType_t timeout)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    ESP_RETURN_ON_FALSE(msc_host != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    return access_guard_acquire(&msc_host->access_guard, timeout);
}

esp_err_t esp_msc_host_unlock(esp_msc_host_handle_t handle)
{
    esp_msc_host_t *msc_host = (esp_msc_host_t *)handle;
    ESP_RETURN_ON_FALSE(msc_host != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    return access_guard_release(&msc_host->access_guard);
}
