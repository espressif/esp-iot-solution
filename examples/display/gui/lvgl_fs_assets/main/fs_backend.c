/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "fs_backend.h"

#include <stdbool.h>
#include <stddef.h>
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_EXAMPLE_FS_SPIFFS
#include "esp_spiffs.h"
#elif CONFIG_EXAMPLE_FS_FATFS
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#elif CONFIG_EXAMPLE_FS_LITTLEFS
#include "esp_littlefs.h"
#endif

static const char *TAG = "fs_backend";
static const char *STORAGE_BASE_PATH = "/storage";
static const char *LVGL_IMAGE_PATH = "A:/storage/wifi_icon.jpeg";
static const char *LVGL_FONT_PATH = "A:/storage/kaiti_24.bin";
static bool s_is_mounted;

#if CONFIG_EXAMPLE_FS_FATFS
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
#endif

static const char *selected_fs_name(void)
{
#if CONFIG_EXAMPLE_FS_SPIFFS
    return "SPIFFS";
#elif CONFIG_EXAMPLE_FS_FATFS
    return "FATFS";
#elif CONFIG_EXAMPLE_FS_LITTLEFS
    return "LittleFS";
#else
    return "Unknown";
#endif
}

static const char *selected_partition_label(void)
{
#if CONFIG_EXAMPLE_FS_SPIFFS
    return "storage_spiffs";
#elif CONFIG_EXAMPLE_FS_FATFS
    return "storage_fatfs";
#elif CONFIG_EXAMPLE_FS_LITTLEFS
    return "storage_littlefs";
#else
    return "storage_spiffs";
#endif
}

static esp_err_t storage_mount(void)
{
#if CONFIG_EXAMPLE_FS_SPIFFS
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = selected_partition_label(),
        .max_files = 4,
        .format_if_mount_failed = false,
    };

    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&conf), TAG, "Failed to mount SPIFFS");

    size_t total = 0;
    size_t used = 0;
    ESP_RETURN_ON_ERROR(esp_spiffs_info(conf.partition_label, &total, &used), TAG, "Failed to query SPIFFS info");
    ESP_LOGI(TAG, "SPIFFS mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
    return ESP_OK;
#elif CONFIG_EXAMPLE_FS_FATFS
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = false,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };

    ESP_RETURN_ON_ERROR(
        esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_BASE_PATH, selected_partition_label(), &mount_config, &s_wl_handle),
        TAG,
        "Failed to mount FATFS");

    ESP_LOGI(TAG, "FATFS mounted with WL handle %d", (int)s_wl_handle);
    return ESP_OK;
#elif CONFIG_EXAMPLE_FS_LITTLEFS
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_BASE_PATH,
        .partition_label = selected_partition_label(),
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    ESP_RETURN_ON_ERROR(esp_vfs_littlefs_register(&conf), TAG, "Failed to mount LittleFS");

    size_t total = 0;
    size_t used = 0;
    ESP_RETURN_ON_ERROR(esp_littlefs_info(conf.partition_label, &total, &used), TAG, "Failed to query LittleFS info");
    ESP_LOGI(TAG, "LittleFS mounted: total=%u used=%u", (unsigned)total, (unsigned)used);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t fs_backend_mount(fs_assets_config_t *config)
{
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Invalid config");
    config->fs_name = selected_fs_name();

    if (s_is_mounted) {
        config->base_path = STORAGE_BASE_PATH;
        config->lvgl_image_path = LVGL_IMAGE_PATH;
        config->lvgl_font_path = LVGL_FONT_PATH;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(storage_mount(), TAG, "Storage mount failed");

    config->base_path = STORAGE_BASE_PATH;
    config->lvgl_image_path = LVGL_IMAGE_PATH;
    config->lvgl_font_path = LVGL_FONT_PATH;
    s_is_mounted = true;
    return ESP_OK;
}

void fs_backend_unmount(void)
{
    if (!s_is_mounted) {
        return;
    }

#if CONFIG_EXAMPLE_FS_SPIFFS
    esp_vfs_spiffs_unregister(selected_partition_label());
#elif CONFIG_EXAMPLE_FS_FATFS
    if (s_wl_handle != WL_INVALID_HANDLE) {
        esp_vfs_fat_spiflash_unmount_rw_wl(STORAGE_BASE_PATH, s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
    }
#elif CONFIG_EXAMPLE_FS_LITTLEFS
    esp_vfs_littlefs_unregister(selected_partition_label());
#endif

    s_is_mounted = false;
}
