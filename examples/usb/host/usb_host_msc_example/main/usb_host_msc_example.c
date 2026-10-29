/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_msc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "app_wifi.h"
#include "app_http_server.h"

#define BASE_PATH "/usb"
#define MSC_SPEED_TEST_FILE_PATH BASE_PATH "/msc_speed_test.bin"
#define MSC_SPEED_TEST_TOTAL_SIZE (32 * 1024 * 1024)
#define MSC_SPEED_TEST_BUFFER_SIZE (64 * 1024)
#define MSC_SPEED_TEST_TASK_STACK_SIZE 4096
#define MSC_SPEED_TEST_TASK_PRIORITY 4

static const char *TAG = "usb_host_msc_example";
static TaskHandle_t s_speed_test_task_handle = NULL;

static void log_errno_error(const char *message)
{
    ESP_LOGE(TAG, "%s, errno=%d (%s)", message, errno, strerror(errno));
}

static double bytes_per_second_to_mb(size_t bytes, int64_t elapsed_us)
{
    if (elapsed_us <= 0) {
        return 0.0;
    }
    return ((double)bytes * 1000000.0) / ((double)elapsed_us * 1024.0 * 1024.0);
}

static esp_err_t write_speed_test_file(FILE *file, const uint8_t *buffer, size_t buffer_size, size_t total_size, size_t *bytes_written)
{
    while (*bytes_written < total_size) {
        size_t chunk_size = total_size - *bytes_written;
        if (chunk_size > buffer_size) {
            chunk_size = buffer_size;
        }

        // Use fixed-size chunks to keep the write path stable during the benchmark.
        size_t written = fwrite(buffer, 1, chunk_size, file);
        if (written != chunk_size) {
            ESP_LOGE(TAG, "Failed to write speed test file, written %zu/%zu bytes, errno=%d (%s)", written, chunk_size, errno, strerror(errno));
            return ESP_FAIL;
        }
        *bytes_written += written;
    }

    return ESP_OK;
}

static esp_err_t read_speed_test_file(FILE *file, uint8_t *buffer, size_t buffer_size, size_t total_size, size_t *bytes_read)
{
    while (*bytes_read < total_size) {
        size_t chunk_size = total_size - *bytes_read;
        if (chunk_size > buffer_size) {
            chunk_size = buffer_size;
        }

        // Read back the whole file so the result reflects the mounted VFS path.
        size_t read_len = fread(buffer, 1, chunk_size, file);
        if (read_len != chunk_size) {
            if (ferror(file)) {
                ESP_LOGE(TAG, "Failed to read speed test file, read %zu/%zu bytes, errno=%d (%s)", read_len, chunk_size, errno, strerror(errno));
            } else {
                ESP_LOGE(TAG, "Unexpected EOF while reading speed test file, read %zu/%zu bytes", read_len, chunk_size);
            }
            return ESP_FAIL;
        }
        *bytes_read += read_len;
    }

    return ESP_OK;
}

static void print_speed_test_result_table(size_t write_bytes, int64_t write_time_us, size_t read_bytes, int64_t read_time_us)
{
    double write_speed = bytes_per_second_to_mb(write_bytes, write_time_us);
    double read_speed = bytes_per_second_to_mb(read_bytes, read_time_us);

    ESP_LOGI(TAG, "+-----------+------------+----------+--------------+");
    ESP_LOGI(TAG, "| Operation | Bytes      | Time (s) | Speed (MB/s) |");
    ESP_LOGI(TAG, "+-----------+------------+----------+--------------+");
    ESP_LOGI(TAG, "| Write     | %10zu | %8.3f | %12.2f |", write_bytes, (double)write_time_us / 1000000.0, write_speed);
    ESP_LOGI(TAG, "| Read      | %10zu | %8.3f | %12.2f |", read_bytes, (double)read_time_us / 1000000.0, read_speed);
    ESP_LOGI(TAG, "+-----------+------------+----------+--------------+");
}

static void msc_speed_test_task(void *arg)
{
    esp_msc_host_handle_t host_handle = (esp_msc_host_handle_t)arg;
    FILE *file = NULL;
    uint8_t *buffer = NULL;
    bool host_locked = false;
    bool speed_test_file_created = false;
    size_t write_bytes = 0;
    size_t read_bytes = 0;
    int64_t write_time_us = 0;
    int64_t read_time_us = 0;

    ESP_LOGI(TAG, "MSC speed test start, file: %s, size: %d bytes", MSC_SPEED_TEST_FILE_PATH, MSC_SPEED_TEST_TOTAL_SIZE);

    buffer = malloc(MSC_SPEED_TEST_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate speed test buffer");
        goto cleanup;
    }

    for (size_t i = 0; i < MSC_SPEED_TEST_BUFFER_SIZE; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    // Hold the MSC host lock so the device cannot be unmounted while the benchmark is accessing FATFS.
    if (esp_msc_host_lock(host_handle, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock MSC host");
        goto cleanup;
    }
    host_locked = true;

    file = fopen(MSC_SPEED_TEST_FILE_PATH, "wb");
    if (file == NULL) {
        log_errno_error("Failed to open speed test file for writing");
        goto cleanup;
    }
    speed_test_file_created = true;

    int64_t start_us = esp_timer_get_time();
    if (write_speed_test_file(file, buffer, MSC_SPEED_TEST_BUFFER_SIZE, MSC_SPEED_TEST_TOTAL_SIZE, &write_bytes) != ESP_OK) {
        goto cleanup;
    }
    if (fflush(file) != 0) {
        log_errno_error("Failed to flush speed test file");
        goto cleanup;
    }
    if (fsync(fileno(file)) != 0) {
        log_errno_error("Failed to sync speed test file");
        goto cleanup;
    }
    write_time_us = esp_timer_get_time() - start_us;

    if (fclose(file) != 0) {
        file = NULL;
        log_errno_error("Failed to close speed test file after writing");
        goto cleanup;
    }
    file = NULL;

    file = fopen(MSC_SPEED_TEST_FILE_PATH, "rb");
    if (file == NULL) {
        log_errno_error("Failed to open speed test file for reading");
        goto cleanup;
    }

    start_us = esp_timer_get_time();
    if (read_speed_test_file(file, buffer, MSC_SPEED_TEST_BUFFER_SIZE, MSC_SPEED_TEST_TOTAL_SIZE, &read_bytes) != ESP_OK) {
        goto cleanup;
    }
    read_time_us = esp_timer_get_time() - start_us;

    if (fclose(file) != 0) {
        file = NULL;
        log_errno_error("Failed to close speed test file after reading");
        goto cleanup;
    }
    file = NULL;

    print_speed_test_result_table(write_bytes, write_time_us, read_bytes, read_time_us);

cleanup:
    if (file != NULL && fclose(file) != 0) {
        log_errno_error("Failed to close speed test file during cleanup");
    }
    if (speed_test_file_created && remove(MSC_SPEED_TEST_FILE_PATH) != 0 && errno != ENOENT) {
        log_errno_error("Failed to remove speed test file");
    }
    if (host_locked) {
        esp_msc_host_unlock(host_handle);
    }
    free(buffer);
    s_speed_test_task_handle = NULL;
    ESP_LOGI(TAG, "MSC speed test finished");
    vTaskDelete(NULL);
}

static void start_msc_speed_test(esp_msc_host_handle_t host_handle)
{
    if (s_speed_test_task_handle != NULL) {
        ESP_LOGW(TAG, "MSC speed test is already running");
        return;
    }

    BaseType_t ret = xTaskCreate(msc_speed_test_task, "msc_speed_test", MSC_SPEED_TEST_TASK_STACK_SIZE, host_handle, MSC_SPEED_TEST_TASK_PRIORITY,
                                 &s_speed_test_task_handle);
    if (ret != pdPASS) {
        s_speed_test_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create MSC speed test task");
    }
}

static void esp_msc_host_handler(esp_msc_host_handle_t handle, esp_msc_host_event_t event_id, void *user_ctx)
{
    switch (event_id) {
    case ESP_MSC_HOST_CONNECT:
        ESP_LOGI(TAG, "MSC device connected");
        break;
    case ESP_MSC_HOST_DISCONNECT:
        ESP_LOGI(TAG, "MSC device disconnected");
        break;
    case ESP_MSC_HOST_DEVICE_INSTALL:
        ESP_LOGI(TAG, "MSC device install");
        break;
    case ESP_MSC_HOST_DEVICE_UNINSTALL:
        ESP_LOGI(TAG, "MSC device uninstall");
        break;
    case ESP_MSC_HOST_VFS_REGISTER:
        ESP_LOGI(TAG, "MSC VFS Register");
        start_msc_speed_test(handle);
        break;
    case ESP_MSC_HOST_VFS_UNREGISTER:
        ESP_LOGI(TAG, "MSC device disconnected");
        break;
    default:
        break;
    }
}

void app_main(void)
{
    esp_msc_host_config_t msc_host_config = {
        .base_path = BASE_PATH,
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG(),
        .event_cb = esp_msc_host_handler,
    };
    esp_msc_host_handle_t host_handle = NULL;
    msc_host_config.host_driver_config.task_priority = 23;
    esp_msc_host_install(&msc_host_config, &host_handle);

    app_wifi_main();
    start_file_server(BASE_PATH);
}
