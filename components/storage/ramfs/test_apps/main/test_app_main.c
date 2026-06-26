/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "ramfs.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "wear_levelling.h"

// Some resources are lazy allocated by newlib/VFS on first file operation, the threshold is left for that case.
#define TEST_MEMORY_LEAK_THRESHOLD (-600)
#define TEST_FATFS_WARMUP_FILE     "/fatfs/WARMUP.TMP"

static const char *TAG = "ramfs_test_app";

static size_t before_free_8bit;
static size_t before_free_32bit;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;

    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit;
    size_t after_free_32bit;

    // Keep later tests isolated when an earlier assertion exits before explicit unregister.
    (void)ramfs_unregister("/ram");
    (void)ramfs_unregister("/ram2");

    after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    FILE *warmup_file;
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 8,
        .allocation_unit_size = 4096,
        .disk_status_check_enable = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
        .use_one_fat = false,
#endif
    };

    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_mount_rw_wl("/fatfs", "storage", &mount_config, &s_wl_handle));

    // Consume FATFS/newlib lazy allocations before Unity starts per-test leak accounting.
    warmup_file = fopen(TEST_FATFS_WARMUP_FILE, "wb");
    if (!warmup_file) {
        ESP_LOGE(TAG, "FATFS warmup open failed: path=%s errno=%d", TEST_FATFS_WARMUP_FILE, errno);
    } else {
        if (fputs("warmup", warmup_file) < 0) {
            ESP_LOGE(TAG, "FATFS warmup write failed: errno=%d", errno);
        }
        if (fclose(warmup_file) != 0) {
            ESP_LOGE(TAG, "FATFS warmup close failed: errno=%d", errno);
        }
        if (unlink(TEST_FATFS_WARMUP_FILE) != 0) {
            ESP_LOGE(TAG, "FATFS warmup unlink failed: path=%s errno=%d", TEST_FATFS_WARMUP_FILE, errno);
        }
    }

    printf(" ____      _    __  __ _____ ____    _____         _\r\n");
    printf("|  _ \\    / \\  |  \\/  |  ___/ ___|  |_   _|__  ___| |_\r\n");
    printf("| |_) |  / _ \\ | |\\/| | |_  \\___ \\    | |/ _ \\/ __| __|\r\n");
    printf("|  _ <  / ___ \\| |  | |  _|  ___) |   | |  __/\\__ \\ |_\r\n");
    printf("|_| \\_\\/_/   \\_\\_|  |_|_|   |____/    |_|\\___||___/\\__|\r\n");
    unity_run_menu();
}
