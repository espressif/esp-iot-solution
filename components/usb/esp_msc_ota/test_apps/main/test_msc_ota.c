/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "unity.h"
#include "esp_msc_host.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"

static const char *TAG = "esp_msc_ota_test";

#define MSC_HOST_TEST_CONNECT          BIT0
#define MSC_HOST_TEST_DISCONNECT       BIT1
#define MSC_HOST_TEST_VFS_REGISTER     BIT2
#define MSC_HOST_TEST_VFS_UNREGISTER   BIT3
#define MSC_HOST_TEST_DEVICE_INSTALL   BIT4
#define MSC_HOST_TEST_DEVICE_UNINSTALL BIT5
#define MSC_HOST_TEST_ALL_EVENTS       (MSC_HOST_TEST_CONNECT | MSC_HOST_TEST_DISCONNECT | MSC_HOST_TEST_VFS_REGISTER | MSC_HOST_TEST_VFS_UNREGISTER | MSC_HOST_TEST_DEVICE_INSTALL | MSC_HOST_TEST_DEVICE_UNINSTALL)

#define MSC_HOST_TEST_FILE_PATH        "/usb/ota_test.bin"
#define MSC_HOST_TEST_STRESS_FILE_PATH "/usb/stress.bin"
#define MSC_HOST_TEST_STRESS_SIZE      (4 * 1024)

static void msc_ota_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MSC_OTA_START:
        ESP_LOGI(TAG, "ESP_MSC_OTA_START");
        break;
    case ESP_MSC_OTA_READY_UPDATE:
        ESP_LOGI(TAG, "ESP_MSC_OTA_READY_UPDATE");
        break;
    case ESP_MSC_OTA_WRITE_FLASH:
        float progress = *(float *)event_data;
        printf("ESP_MSC_OTA_WRITE_FLASH %.2f%%\r", progress * 100);
        break;
    case ESP_MSC_OTA_FAILED: {
        esp_err_t err = *(esp_err_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_FAILED, err: %s", esp_err_to_name(err));
        break;
    }
    case ESP_MSC_OTA_GET_IMG_DESC:
        ESP_LOGI(TAG, "ESP_MSC_OTA_GET_IMG_DESC");
        break;
    case ESP_MSC_OTA_VERIFY_CHIP_ID:
        esp_chip_id_t chip_id = *(esp_chip_id_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_VERIFY_CHIP_ID, chip_id: %08x", chip_id);
        break;
    case ESP_MSC_OTA_UPDATE_BOOT_PARTITION:
        esp_partition_subtype_t subtype = *(esp_partition_subtype_t *)event_data;
        ESP_LOGI(TAG, "ESP_MSC_OTA_UPDATE_BOOT_PARTITION, subtype: %d", subtype);
        break;
    case ESP_MSC_OTA_FINISH:
        ESP_LOGI(TAG, "ESP_MSC_OTA_FINISH");
        break;
    case ESP_MSC_OTA_ABORT:
        ESP_LOGI(TAG, "ESP_MSC_OTA_ABORT");
        break;
    }
}

static void msc_host_test_event_handler(esp_msc_host_handle_t handle, esp_msc_host_event_t event, void *user_ctx)
{
    EventGroupHandle_t event_group = (EventGroupHandle_t)user_ctx;
    if (event_group == NULL) {
        return;
    }

    switch (event) {
    case ESP_MSC_HOST_CONNECT:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_CONNECT);
        break;
    case ESP_MSC_HOST_DISCONNECT:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_DISCONNECT);
        break;
    case ESP_MSC_HOST_DEVICE_INSTALL:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_DEVICE_INSTALL);
        break;
    case ESP_MSC_HOST_DEVICE_UNINSTALL:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_DEVICE_UNINSTALL);
        break;
    case ESP_MSC_HOST_VFS_REGISTER:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_VFS_REGISTER);
        break;
    case ESP_MSC_HOST_VFS_UNREGISTER:
        xEventGroupSetBits(event_group, MSC_HOST_TEST_VFS_UNREGISTER);
        break;
    default:
        break;
    }
}

static void wait_for_msc_host_event(EventGroupHandle_t event_group, EventBits_t event, TickType_t timeout)
{
    EventBits_t bits = xEventGroupWaitBits(event_group, event, pdTRUE, pdFALSE, timeout);
    TEST_ASSERT_TRUE((bits & event) == event);
}

static esp_err_t _usb_port_power(bool power)
{
    ESP_LOGI(TAG, "Set root port power: %s", power ? "true" : "false");
    return usb_host_lib_set_root_port_power(power);
}

static esp_msc_host_config_t default_msc_host_config(void)
{
    return (esp_msc_host_config_t) {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = DEFAULT_USB_HOST_CONFIG(),
    };
}

static esp_msc_ota_config_t default_msc_ota_config(esp_msc_host_handle_t host_handle)
{
    return (esp_msc_ota_config_t) {
        .host_handle = host_handle,
        .ota_bin_path = MSC_HOST_TEST_FILE_PATH,
        .wait_msc_connect = pdMS_TO_TICKS(5000),
    };
}

typedef struct {
    esp_msc_host_handle_t host_handle;
    TaskHandle_t notify_task;
    volatile bool stop;
    uint32_t lock_ok_count;
    uint32_t lock_fail_count;
    uint32_t write_ok_count;
    uint32_t write_fail_count;
    uint32_t read_ok_count;
    uint32_t read_fail_count;
    uint32_t verify_ok_count;
    uint32_t verify_fail_count;
    uint32_t unexpected_unlock_count;
    bool alloc_failed;
} msc_file_access_test_ctx_t;

static void msc_file_access_task(void *arg)
{
    msc_file_access_test_ctx_t *ctx = (msc_file_access_test_ctx_t *)arg;
    uint8_t *write_buf = malloc(MSC_HOST_TEST_STRESS_SIZE);
    uint8_t *read_buf  = malloc(MSC_HOST_TEST_STRESS_SIZE);
    if (write_buf == NULL || read_buf == NULL) {
        ctx->alloc_failed = true;
        goto done;
    }

    uint32_t seed = 0;
    while (!ctx->stop) {
        esp_err_t ret = esp_msc_host_lock(ctx->host_handle, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ctx->lock_fail_count++;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        ctx->lock_ok_count++;

        /* Fill each round with a distinct pattern so a stale read is detectable. */
        seed++;
        for (size_t i = 0; i < MSC_HOST_TEST_STRESS_SIZE; i++) {
            write_buf[i] = (uint8_t)(seed + i);
        }

        FILE *file = fopen(MSC_HOST_TEST_STRESS_FILE_PATH, "wb");
        if (file == NULL) {
            ctx->write_fail_count++;
            goto next;
        }
        size_t written = fwrite(write_buf, 1, MSC_HOST_TEST_STRESS_SIZE, file);
        fclose(file);
        if (written != MSC_HOST_TEST_STRESS_SIZE) {
            ctx->write_fail_count++;
            goto next;
        }
        ctx->write_ok_count++;

        file = fopen(MSC_HOST_TEST_STRESS_FILE_PATH, "rb");
        if (file == NULL) {
            ctx->read_fail_count++;
            goto next;
        }
        size_t read_n = fread(read_buf, 1, MSC_HOST_TEST_STRESS_SIZE, file);
        fclose(file);
        if (read_n != MSC_HOST_TEST_STRESS_SIZE) {
            ctx->read_fail_count++;
            goto next;
        }
        ctx->read_ok_count++;

        /* Both write and read succeeded inside a single lock window – contents
         * must match; any mismatch here would indicate FS/cache corruption. */
        if (memcmp(read_buf, write_buf, MSC_HOST_TEST_STRESS_SIZE) == 0) {
            ctx->verify_ok_count++;
        } else {
            ctx->verify_fail_count++;
        }

next:
        ret = esp_msc_host_unlock(ctx->host_handle);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ctx->unexpected_unlock_count++;
        }
        /* Small yield keeps the pressure high without starving other tasks. */
        vTaskDelay(1);
    }

done:
    free(write_buf);
    free(read_buf);
    xTaskNotifyGive(ctx->notify_task);
    vTaskDelete(NULL);
}

esp_msc_ota_handle_t handle;

TEST_CASE("Test memory leaks", "[memory leaks][MSC OTA]")
{
    esp_msc_host_config_t msc_host_config = default_msc_host_config();
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    TEST_ESP_OK(_usb_port_power(false));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

TEST_CASE("MSC host root port power recovers after disconnect while locked", "[MSC HOST][root_port_power][Auto]")
{
    EventGroupHandle_t host_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(host_events);

    usb_host_config_t host_config = DEFAULT_USB_HOST_CONFIG();
    host_config.root_port_unpowered = true;

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = host_config,
        .event_cb = msc_host_test_event_handler,
        .event_cb_arg = host_events,
    };
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));

    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ESP_OK(_usb_port_power(true));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_REGISTER, pdMS_TO_TICKS(10000));
    TEST_ASSERT_TRUE(esp_msc_host_is_mounted(host_handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_msc_host_uninstall(host_handle));

    TEST_ESP_OK(esp_msc_host_lock(host_handle, pdMS_TO_TICKS(1000)));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ESP_OK(_usb_port_power(false));
    vTaskDelay(pdMS_TO_TICKS(1000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(8000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));
    TEST_ASSERT_FALSE(esp_msc_host_is_mounted(host_handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_msc_host_unlock(host_handle));
    ESP_LOGI(TAG, "Unlocking host failed as expected");

    xEventGroupClearBits(host_events, MSC_HOST_TEST_ALL_EVENTS);
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ESP_OK(_usb_port_power(true));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_REGISTER, pdMS_TO_TICKS(10000));
    TEST_ASSERT_TRUE(esp_msc_host_is_mounted(host_handle));

    TEST_ESP_OK(_usb_port_power(false));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(8000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));

    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vEventGroupDelete(host_events);
}

TEST_CASE("MSC host file access survives repeated root port power toggles", "[MSC HOST][root_port_power][Auto]")
{
    EventGroupHandle_t host_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(host_events);

    usb_host_config_t host_config = DEFAULT_USB_HOST_CONFIG();
    host_config.root_port_unpowered = true;

    esp_msc_host_config_t msc_host_config = {
        .base_path = "/usb",
        .host_driver_config = DEFAULT_MSC_HOST_DRIVER_CONFIG(),
        .vfs_fat_mount_config = DEFAULT_ESP_VFS_FAT_MOUNT_CONFIG(),
        .host_config = host_config,
        .event_cb = msc_host_test_event_handler,
        .event_cb_arg = host_events,
    };
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));

    TEST_ESP_OK(_usb_port_power(true));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_REGISTER, pdMS_TO_TICKS(10000));
    TEST_ASSERT_TRUE(esp_msc_host_is_mounted(host_handle));

    msc_file_access_test_ctx_t access_ctx = {
        .host_handle = host_handle,
        .notify_task = xTaskGetCurrentTaskHandle(),
    };
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(msc_file_access_task, "msc_file_access", 4096, &access_ctx, 5, NULL));
    vTaskDelay(pdMS_TO_TICKS(500));

    for (int i = 0; i < 3; i++) {
        xEventGroupClearBits(host_events, MSC_HOST_TEST_ALL_EVENTS);
        TEST_ESP_OK(_usb_port_power(false));
        wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(10000));
        wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));
        TEST_ASSERT_FALSE(esp_msc_host_is_mounted(host_handle));

        vTaskDelay(pdMS_TO_TICKS(1200));
        xEventGroupClearBits(host_events, MSC_HOST_TEST_ALL_EVENTS);
        TEST_ESP_OK(_usb_port_power(true));
        wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_REGISTER, pdMS_TO_TICKS(10000));
        TEST_ASSERT_TRUE(esp_msc_host_is_mounted(host_handle));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    access_ctx.stop = true;
    TEST_ASSERT_NOT_EQUAL(0, ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)));
    ESP_LOGI(TAG, "file access stats: lock_ok=%" PRIu32 ", lock_fail=%" PRIu32
             ", write_ok=%" PRIu32 ", write_fail=%" PRIu32
             ", read_ok=%" PRIu32 ", read_fail=%" PRIu32
             ", verify_ok=%" PRIu32 ", verify_fail=%" PRIu32,
             access_ctx.lock_ok_count, access_ctx.lock_fail_count,
             access_ctx.write_ok_count, access_ctx.write_fail_count,
             access_ctx.read_ok_count, access_ctx.read_fail_count,
             access_ctx.verify_ok_count, access_ctx.verify_fail_count);
    TEST_ASSERT_FALSE_MESSAGE(access_ctx.alloc_failed, "stress buffer alloc failed");
    TEST_ASSERT_GREATER_THAN_UINT32(0, access_ctx.lock_ok_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0, access_ctx.lock_fail_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0, access_ctx.write_ok_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0, access_ctx.read_ok_count);
    TEST_ASSERT_GREATER_THAN_UINT32(0, access_ctx.verify_ok_count);
    TEST_ASSERT_EQUAL_UINT32(0, access_ctx.verify_fail_count);
    TEST_ASSERT_EQUAL_UINT32(0, access_ctx.unexpected_unlock_count);

    TEST_ESP_OK(_usb_port_power(false));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(10000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));

    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vEventGroupDelete(host_events);
}

TEST_CASE("MSC OTA one-shot API updates from mounted USB disk", "[MSC OTA][manual]")
{
    EventGroupHandle_t host_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(host_events);
    /* Default event loop is provided by the test fixture (setUp/tearDown). */
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));

    esp_msc_host_config_t msc_host_config = default_msc_host_config();
    msc_host_config.event_cb = msc_host_test_event_handler;
    msc_host_config.event_cb_arg = host_events;
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));

    esp_msc_ota_config_t config = default_msc_ota_config(host_handle);
    TEST_ESP_OK(esp_msc_ota(&config));

    TEST_ESP_OK(_usb_port_power(false));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(10000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));
    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vEventGroupDelete(host_events);
}

TEST_CASE("MSC OTA begin and abort cleans up staged update", "[MSC OTA][Auto]")
{
    EventGroupHandle_t host_events = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(host_events);
    /* Default event loop is provided by the test fixture (setUp/tearDown). */
    ESP_ERROR_CHECK(esp_event_handler_register(ESP_MSC_OTA_EVENT, ESP_EVENT_ANY_ID, &msc_ota_event_handler, NULL));

    esp_msc_host_config_t msc_host_config = default_msc_host_config();
    msc_host_config.event_cb = msc_host_test_event_handler;
    msc_host_config.event_cb_arg = host_events;
    esp_msc_host_handle_t host_handle = NULL;
    TEST_ESP_OK(esp_msc_host_install(&msc_host_config, &host_handle));

    esp_msc_ota_handle_t msc_ota_handle = NULL;
    esp_msc_ota_config_t config = default_msc_ota_config(host_handle);
    TEST_ESP_OK(esp_msc_ota_begin(&config, &msc_ota_handle));
    TEST_ASSERT_EQUAL(ESP_MSC_OTA_BEGIN, esp_msc_ota_get_status(msc_ota_handle));
    TEST_ESP_OK(esp_msc_ota_abort(msc_ota_handle));

    TEST_ESP_OK(_usb_port_power(false));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_VFS_UNREGISTER, pdMS_TO_TICKS(10000));
    wait_for_msc_host_event(host_events, MSC_HOST_TEST_DEVICE_UNINSTALL, pdMS_TO_TICKS(3000));
    TEST_ESP_OK(esp_msc_host_uninstall(host_handle));
    vEventGroupDelete(host_events);
}
