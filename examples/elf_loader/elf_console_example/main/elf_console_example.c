/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_littlefs.h"

#include "nvs_flash.h"

#ifdef CONFIG_ELF_SHELL
#include "elf_shell.h"
#endif

#ifdef CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#define FS_BASE_PATH CONFIG_ELF_FILE_SYSTEM_BASE_PATH
#else
#define FS_BASE_PATH "/storage"
#endif

static const char *TAG = "elf_console";

static void fs_init(void)
{
    size_t total = 0, used = 0;
    esp_vfs_littlefs_conf_t conf = {
        .base_path = FS_BASE_PATH,
        .partition_label = "storage",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&conf));
    ESP_ERROR_CHECK(esp_littlefs_info(conf.partition_label, &total, &used));

    ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
}

int app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    fs_init();

#ifdef CONFIG_ELF_SHELL
    shell_init();
#endif

    return 0;
}
