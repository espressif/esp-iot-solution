/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "msc_host_vfs.h"
#include "msc_host.h"
#include "usb/usb_host.h"

/**
 * @brief MSC host events reported through esp_msc_host_event_cb_t.
 */
typedef enum {
    ESP_MSC_HOST_CONNECT,        /*!< MSC device connected */
    ESP_MSC_HOST_DISCONNECT,     /*!< MSC device disconnected */
    ESP_MSC_HOST_DEVICE_INSTALL, /*!< MSC device installed */
    ESP_MSC_HOST_DEVICE_UNINSTALL, /*!< MSC device uninstalled */
    ESP_MSC_HOST_VFS_REGISTER,   /*!< VFS driver registered */
    ESP_MSC_HOST_VFS_UNREGISTER, /*!< VFS driver unregistered */
} esp_msc_host_event_t;

/**
 * @brief Opaque MSC host handle.
 *
 * The concrete type is private to the component; users must only pass the
 * value returned by esp_msc_host_install() to the host APIs.
 */
typedef struct esp_msc_host_ctx *esp_msc_host_handle_t;

/**
 * @brief MSC host event callback.
 *
 * This callback is optional and is the only notification path for MSC host
 * events. The handle identifies the host instance that generated the event.
 */
typedef void (*esp_msc_host_event_cb_t)(esp_msc_host_handle_t handle, esp_msc_host_event_t event, void *user_ctx);

#define DEFAULT_USB_HOST_CONFIG()       \
{                                       \
    .intr_flags = ESP_INTR_FLAG_LEVEL1, \
}

#define DEFAULT_MSC_HOST_DRIVER_CONFIG() \
{                                      \
    .create_backround_task = true,     \
    .task_priority = 5,                \
    .stack_size = 4096,                \
}

#define DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG() \
{                                      \
    .format_if_mount_failed = false,   \
    .max_files = 3,                    \
    .allocation_unit_size = 1024,      \
}

/**
 * @brief MSC host driver configuration passed to esp_msc_host_install().
 */
typedef struct {
    const char *base_path;                            /*!< Base path for mounting FATFS. */
    usb_host_config_t host_config;                    /*!< Configuration structure of the USB Host Library. Provided in the usb_host_install() function */
    msc_host_driver_config_t host_driver_config;      /*!< MSC configuration structure. Do not register the callback variable */
    esp_vfs_fat_mount_config_t vfs_fat_mount_config;  /*!< Configuration arguments for msc_host_vfs_register function */
    bool skip_init_usb_host_driver;                   /*!< Skip USB Host Library install/uninstall and event handling task. The application must install USB Host and call usb_host_lib_handle_events() */
    esp_msc_host_event_cb_t event_cb;                 /*!< Optional direct callback for MSC host events */
    void *event_cb_arg;                               /*!< User context for event_cb */
} esp_msc_host_config_t;

/**
 * @brief Check if the MSC VFS is mounted.
 *
 * @param[in] handle Handle for the MSC host driver
 * @return true if the VFS is mounted
 * @return false if the VFS is not mounted or the handle is invalid
 */
bool esp_msc_host_is_mounted(esp_msc_host_handle_t handle);

/**
 * @brief Block until the MSC VFS is mounted (or timeout elapses).
 *
 * Returns immediately with ESP_OK when the VFS is already mounted. Otherwise
 * the calling task is suspended and woken up as soon as the internal MSC host
 * task finishes mounting the device.
 *
 * @param[in] handle  Handle for the MSC host driver
 * @param[in] timeout Maximum time to wait, in FreeRTOS ticks. Use portMAX_DELAY
 *                    to wait indefinitely, or 0 for a non-blocking poll.
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle is invalid
 * @return ESP_ERR_TIMEOUT if the VFS was not mounted before the timeout expired
 */
esp_err_t esp_msc_host_wait_mounted(esp_msc_host_handle_t handle, TickType_t timeout);

/**
 * @brief Lock MSC file access before reading from the mounted VFS.
 *
 * This prevents the host task from unregistering VFS while a client is
 * actively reading files from the MSC device.
 *
 * @param[in] handle Handle for the MSC host driver
 * @param[in] timeout Timeout in FreeRTOS ticks
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle is invalid
 * @return ESP_ERR_INVALID_STATE if VFS is not mounted
 * @return ESP_ERR_TIMEOUT if the lock cannot be taken in time
 */
esp_err_t esp_msc_host_lock(esp_msc_host_handle_t handle, TickType_t timeout);

/**
 * @brief Unlock MSC file access.
 *
 * @param[in] handle Handle for the MSC host driver
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if handle is invalid
 */
esp_err_t esp_msc_host_unlock(esp_msc_host_handle_t handle);

/**
 * @brief Install the MSC USB HOST
 *
 * @note When the USB flash drive is inserted, do not call uninstall immediately afterward.
 * @param[in] config See esp_msc_host_config_t for details.
 * @param[out] handle Handle for the MSC host driver
 * @return esp_err_t
 *         ESP_ERR_INVALID_ARG if any of the parameters are invalid.
 *         ESP_ERR_NO_MEM if memory can not be allocated for the driver.
 *         ESP_FAIL if the driver fails to install.
 *         ESP_OK on success.
 */
esp_err_t esp_msc_host_install(esp_msc_host_config_t *config, esp_msc_host_handle_t *handle);

/**
 * @brief Uninstall the MSC USB HOST
 *
 * @note When the USB flash drive is inserted, you need to pull out the USB flash drive.
 * @param[in] handle Handle for the MSC host driver
 * @return esp_err_t
 *         ESP_ERR_INVALID_ARG Invalid argument.
 *         ESP_ERR_INVALID_STATE if an MSC device is still connected or mounted.
 *         ESP_OK on success.
 */
esp_err_t esp_msc_host_uninstall(esp_msc_host_handle_t handle);

#ifdef __cplusplus
}
#endif
