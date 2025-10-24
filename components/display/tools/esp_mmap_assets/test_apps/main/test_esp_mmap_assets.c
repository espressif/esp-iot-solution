/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "string.h"
#include "esp_spiffs.h"

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "mmap_generate_factory.h"
#include "mmap_generate_assets_build.h"

static const char *TAG = "assets_test";

static int is_png(const uint8_t *raw_data, size_t len)
{
    const uint8_t png_signature[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (len < sizeof(png_signature)) {
        return false;
    }
    return memcmp(png_signature, raw_data, sizeof(png_signature)) == 0;
}

static int is_jpg(const uint8_t *raw_data, size_t len)
{
    const uint8_t jpg_signature[] = {0xFF, 0xD8, 0xFF,  0xE0,  0x00,  0x10, 0x4A,  0x46, 0x49, 0x46};
    if (len < sizeof(jpg_signature)) {
        return false;
    }
    return memcmp(jpg_signature, raw_data, sizeof(jpg_signature)) == 0;
}

void print_file_list(mmap_assets_handle_t handle, int file_num, bool mmap_enable)
{
    uint8_t src_data[512];

    for (int i = 0; i < file_num; i++) {
        const char *name = mmap_assets_get_name(handle, i);
        const uint8_t *offset_addr = mmap_assets_get_mem(handle, i);
        int size = mmap_assets_get_size(handle, i);
        int width = mmap_assets_get_width(handle, i);
        int height = mmap_assets_get_height(handle, i);

        ESP_LOGI(TAG, "name:[%-10s] mem:[%p] size:[%8d bytes] w:[%4d] h:[%4d]", name, offset_addr, size, width, height);

        if (mmap_enable) {
            memcpy(src_data, offset_addr, sizeof(src_data));
        } else {
            mmap_assets_copy_mem(handle, (size_t)offset_addr, src_data, sizeof(src_data));
        }

        if (strstr(name, ".png")) {
            TEST_ASSERT_TRUE(is_png(src_data, size));
        } else if (strstr(name, ".jpg")) {
            TEST_ASSERT_TRUE(is_jpg(src_data, size));
        }
    }
}

TEST_CASE("test assets mmap table", "[mmap_assets][mmap_enable][Independent partition]")
{
    mmap_assets_handle_t asset_handle;

    const mmap_assets_config_t config = {
        .partition_label = "assets_build",
        .max_files = MMAP_ASSETS_BUILD_FILES,
        .checksum = MMAP_ASSETS_BUILD_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = false,
            .full_check = true,
            .metadata_check = true,
        },
    };

    TEST_ESP_OK(mmap_assets_new(&config, &asset_handle));

    int stored_files = mmap_assets_get_stored_files(asset_handle);
    ESP_LOGI(TAG, "stored_files:%d", stored_files);

    print_file_list(asset_handle, MMAP_ASSETS_BUILD_FILES, true);

    mmap_assets_del(asset_handle);
}

TEST_CASE("test assets mmap table", "[mmap_assets][mmap_enable][Append Partition]")
{
    mmap_assets_handle_t asset_handle;

    const mmap_assets_config_t config = {
        .partition_label = "factory",
        .max_files = MMAP_FACTORY_FILES,
        .checksum = MMAP_FACTORY_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .full_check = true,
        },
    };

    TEST_ESP_OK(mmap_assets_new(&config, &asset_handle));

    int stored_files = mmap_assets_get_stored_files(asset_handle);
    ESP_LOGI(TAG, "stored_files:%d", stored_files);

    print_file_list(asset_handle, MMAP_FACTORY_FILES, true);

    mmap_assets_del(asset_handle);
}

TEST_CASE("test assets mmap table", "[mmap_assets][mmap_enable][Prebuilt Partition]")
{
    mmap_assets_handle_t asset_handle;

    const mmap_assets_config_t config = {
        .partition_label = "assets_prebuilt",
        .flags = {
            .mmap_enable = true,
            .full_check = true,
        },
    };

    TEST_ESP_OK(mmap_assets_new(&config, &asset_handle));

    int stored_files = mmap_assets_get_stored_files(asset_handle);
    ESP_LOGI(TAG, "stored_files:%d", stored_files);

    print_file_list(asset_handle, stored_files, true);

    mmap_assets_del(asset_handle);
}

TEST_CASE("test assets mmap table", "[mmap_assets][File System]")
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "assets_storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    mmap_assets_handle_t asset_handle;

    const mmap_assets_config_t config = {
        .partition_label = "/spiffs/assets_build.bin",
        .flags = {
            .use_fs = true,
            .mmap_enable = false,
            .full_check = true,
        },
    };

    TEST_ESP_OK(mmap_assets_new(&config, &asset_handle));

    int stored_files = mmap_assets_get_stored_files(asset_handle);
    ESP_LOGI(TAG, "stored_files:%d", stored_files);

    print_file_list(asset_handle, stored_files, false);

    mmap_assets_del(asset_handle);

    ESP_LOGI(TAG, "SPIFFS unmounted");
    esp_vfs_spiffs_unregister(conf.partition_label);
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (800)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    printf("mmap assets TEST \n");
    unity_run_menu();
}
