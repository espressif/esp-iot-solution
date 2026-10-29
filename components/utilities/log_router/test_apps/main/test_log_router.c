/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "unity.h"
#include "unity_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#include "log_router.h"

#define TEST_MOUNT_PATH "/log"
#define TEST_PARTITION_LABEL "log"
#define TEST_LOG_FILE_NAME "log.txt"
#define TEST_LOG_COUNT 100

static wl_handle_t wl_handle = WL_INVALID_HANDLE;
static const char *TAG = "test_log_router";

TEST_CASE("test_log_router_fatfs", "[log_router][fatfs]")
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 4096
    };
    TEST_ESP_OK(esp_vfs_fat_spiflash_mount_rw_wl(TEST_MOUNT_PATH, TEST_PARTITION_LABEL, &mount_config, &wl_handle));

    TEST_ESP_OK(esp_log_router_to_file("/log/log.txt", TAG, ESP_LOG_WARN));
    TEST_ESP_OK(esp_log_router_to_file("/log/log2.txt", TAG, ESP_LOG_ERROR));

    esp_log_router_dump_file_messages("/log/log.txt");
    esp_log_router_dump_file_messages("/log/log2.txt");

    for (int i = 0; i < TEST_LOG_COUNT; i++) {
        ESP_LOGI(TAG, "Test log %d", i);
        ESP_LOGW(TAG, "Test log %d", i);
        ESP_LOGE(TAG, "Test log %d", i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log.txt"));
    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log2.txt"));
    TEST_ESP_OK(esp_log_delete_router("/log/log.txt"));
    TEST_ESP_OK(esp_log_delete_router("/log/log2.txt"));

    TEST_ESP_OK(esp_vfs_fat_spiflash_unmount_rw_wl(TEST_MOUNT_PATH, wl_handle));
}

TEST_CASE("test_log_router_spiffs", "[log_router][spiffs]")
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = TEST_MOUNT_PATH,
        .partition_label = TEST_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    TEST_ESP_OK(esp_vfs_spiffs_register(&conf));

    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    TEST_ESP_OK(esp_log_router_to_file("/log/log.txt", TAG, ESP_LOG_WARN));
    TEST_ESP_OK(esp_log_router_to_file("/log/log2.txt", TAG, ESP_LOG_ERROR));

    esp_log_router_dump_file_messages("/log/log.txt");
    esp_log_router_dump_file_messages("/log/log2.txt");

    for (int i = 0; i < TEST_LOG_COUNT; i++) {
        ESP_LOGI(TAG, "Test log %d", i);
        ESP_LOGW(TAG, "Test log %d", i);
        ESP_LOGE(TAG, "Test log %d", i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log.txt"));
    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log2.txt"));

    TEST_ESP_OK(esp_log_delete_router("/log/log.txt"));
    TEST_ESP_OK(esp_log_delete_router("/log/log2.txt"));

    esp_vfs_spiffs_unregister(TEST_PARTITION_LABEL);
}

TEST_CASE("test_log_router_fatfs_tag", "[log_router][spiffs][tag]")
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 4096
    };

    TEST_ESP_OK(esp_vfs_fat_spiflash_mount_rw_wl(TEST_MOUNT_PATH, TEST_PARTITION_LABEL, &mount_config, &wl_handle));

    TEST_ESP_OK(esp_log_router_to_file("/log/log.txt", TAG, ESP_LOG_WARN));
    TEST_ESP_OK(esp_log_router_to_file("/log/log2.txt", NULL, ESP_LOG_ERROR));

    esp_log_router_dump_file_messages("/log/log.txt");
    esp_log_router_dump_file_messages("/log/log2.txt");

    for (int i = 0; i < TEST_LOG_COUNT; i++) {
        ESP_LOGI(TAG, "Test log %d", i);
        ESP_LOGW(TAG, "Test log %d", i);
        ESP_LOGE("test", "Test log %d", i);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log.txt"));
    TEST_ESP_OK(esp_log_router_dump_file_messages("/log/log2.txt"));
    TEST_ESP_OK(esp_log_delete_router("/log/log.txt"));
    TEST_ESP_OK(esp_log_delete_router("/log/log2.txt"));

    TEST_ESP_OK(esp_vfs_fat_spiflash_unmount_rw_wl(TEST_MOUNT_PATH, wl_handle));
}

void app_main(void)
{
    unity_run_menu();
}
