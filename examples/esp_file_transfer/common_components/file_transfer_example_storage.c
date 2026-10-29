/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

#include "file_transfer_example_common.h"
#include "file_transfer_example_internal.h"

static const char *TAG = "ft_storage";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

esp_err_t file_transfer_example_storage_init(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 4096,
        .disk_status_check_enable = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
        .use_one_fat = true,
#endif
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
                        FILE_TRANSFER_EXAMPLE_MOUNT_POINT, "storage",
                        &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FATFS mount failed: %s", esp_err_to_name(err));
        return err;
    }

    if (mkdir(FILE_TRANSFER_EXAMPLE_RECV_DIR, 0755) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Failed to create %s, errno=%d",
                 FILE_TRANSFER_EXAMPLE_RECV_DIR, errno);
        return ESP_FAIL;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    err = esp_vfs_fat_info(FILE_TRANSFER_EXAMPLE_MOUNT_POINT,
                           &total_bytes, &free_bytes);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "FATFS mounted: total=%" PRIu64 ", free=%" PRIu64,
                 total_bytes, free_bytes);
    }
    return ESP_OK;
}
