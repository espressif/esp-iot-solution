/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"
#include "msc_host_vfs.h"
#include "msc_host.h"
#include "usb/usb_host.h"

/**
 * @brief Declare Event Base for ESP MSC HOST
 *
 */
ESP_EVENT_DECLARE_BASE(ESP_MSC_HOST_EVENT);

typedef enum {
    ESP_MSC_HOST_CONNECT,        /*!< MSC device connected */
    ESP_MSC_HOST_DISCONNECT,     /*!< MSC device disconnected */
    ESP_MSC_HOST_INSTALL,        /*!< MSC host driver installed */
    ESP_MSC_HOST_UNINSTALL,      /*!< MSC host driver uninstalled */
    ESP_MSC_HOST_VFS_REGISTER,   /*!< VFS driver registered */
    ESP_MSC_HOST_VFS_UNREGISTER, /*!< VFS driver unregistered */
} esp_msc_host_event_t;

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

typedef struct {
    const char *base_path;                            /*!< Base path for mounting FATFS. */
    usb_host_config_t host_config;                    /*!< Configuration structure of the USB Host Library. Provided in the usb_host_install() function */
    msc_host_driver_config_t host_driver_config;      /*!< MSC configuration structure. Do not register the callback variable */
    esp_vfs_fat_mount_config_t vfs_fat_mount_config;  /*!< Configuration arguments for msc_host_vfs_register function */
} esp_msc_host_config_t;

/**
 * @brief Handle for the MSC host driver
 *
 */
typedef void *esp_msc_host_handle_t;

/**
 * @brief To ensure that the program doesn't log out of the vfs file system while reading or writing to the FATFS
 *
 */
extern SemaphoreHandle_t msc_read_semaphore;

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
 *         ESP_OK on success.
 */
esp_err_t esp_msc_host_uninstall(esp_msc_host_handle_t handle);

#ifdef __cplusplus
}
#endif
