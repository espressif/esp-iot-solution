/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_ota_raw.h"
#include "esp_ble_ota_raw.h"

#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"

#define OTA_TASK_STACK_SIZE                 8192
#define OTA_PRE_ERASE_CHUNK_SIZE            (64 * 1024)

static const char *TAG = "ble_ota_raw";

static RingbufHandle_t s_ringbuf = NULL;
static esp_ota_handle_t s_out_handle;
static TaskHandle_t s_ota_task_handle = NULL;

static esp_err_t erase_partition_in_chunks(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t offset = 0; offset < partition->size; offset += OTA_PRE_ERASE_CHUNK_SIZE) {
        size_t erase_size = OTA_PRE_ERASE_CHUNK_SIZE;
        if ((offset + erase_size) > partition->size) {
            erase_size = partition->size - offset;
        }

        esp_err_t err = esp_partition_erase_range(partition, offset, erase_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "erase failed at 0x%zx, size=0x%zx (%s)",
                     offset, erase_size, esp_err_to_name(err));
            return err;
        }

        ESP_LOGI(TAG, "erase success at 0x%zx, size=0x%zx", offset, erase_size);
        /* Yield between erase chunks so IDLE can run and feed TWDT. */
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    return ESP_OK;
}

static size_t write_to_ringbuf(const uint8_t *data, size_t size)
{
    BaseType_t done = xRingbufferSend(s_ringbuf, (void *)data, size, (TickType_t)portMAX_DELAY);
    if (done) {
        return size;
    }
    return 0;
}

static void ota_task(void *arg)
{
    (void)arg;

    esp_partition_t *partition_ptr = NULL;
    esp_partition_t partition;
    const esp_partition_t *next_partition = NULL;
    bool ota_in_progress = false;

    uint32_t recv_len = 0, fw_length = 0;
    uint8_t *data = NULL;
    size_t item_size = 0;
    ESP_LOGI(TAG, "ota_task start");

    partition_ptr = (esp_partition_t *)esp_ota_get_boot_partition();
    if (partition_ptr == NULL) {
        ESP_LOGE(TAG, "boot partition NULL!");
        goto OTA_ERROR;
    }
    if (partition_ptr->type != ESP_PARTITION_TYPE_APP) {
        ESP_LOGE(TAG, "esp_current_partition->type != ESP_PARTITION_TYPE_APP");
        goto OTA_ERROR;
    }

    if (partition_ptr->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    } else {
        next_partition = esp_ota_get_next_update_partition(partition_ptr);
        if (next_partition) {
            partition.subtype = next_partition->subtype;
        } else {
            partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        }
    }
    partition.type = ESP_PARTITION_TYPE_APP;

    partition_ptr = (esp_partition_t *)esp_partition_find_first(partition.type, partition.subtype, NULL);
    if (partition_ptr == NULL) {
        ESP_LOGE(TAG, "partition NULL!");
        goto OTA_ERROR;
    }

    memcpy(&partition, partition_ptr, sizeof(esp_partition_t));
    ESP_LOGI(TAG, "pre-erase OTA partition, size=%" PRIu32, (uint32_t)partition.size);
    if (erase_partition_in_chunks(&partition) != ESP_OK) {
        goto OTA_ERROR;
    }

    /* Partition has been erased up-front; avoid one-shot erase inside ota_begin. */
    if (esp_ota_begin(&partition, OTA_WITH_SEQUENTIAL_WRITES, &s_out_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed!");
        goto OTA_ERROR;
    }
    ota_in_progress = true;

    ESP_LOGI(TAG, "wait firmware data from ringbuf!");

    if (s_ringbuf == NULL) {
        ESP_LOGE(TAG, "Ring buffer not initialized");
        goto OTA_ERROR;
    }

    for (;;) {
        data = (uint8_t *)xRingbufferReceive(s_ringbuf, &item_size, (TickType_t)portMAX_DELAY);
        if (data == NULL) {
            ESP_LOGE(TAG, "xRingbufferReceive returned NULL");
            goto OTA_ERROR;
        }

        ESP_LOGI(TAG, "recv: %u, recv_total:%" PRIu32, item_size, recv_len + item_size);

        if (item_size != 0) {
            if (esp_ota_write(s_out_handle, (const void *)data, item_size) != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed!");
                vRingbufferReturnItem(s_ringbuf, (void *)data);
                goto OTA_ERROR;
            }
            recv_len += item_size;
            vRingbufferReturnItem(s_ringbuf, (void *)data);

            if (fw_length == 0) {
                fw_length = esp_ble_ota_raw_get_fw_length();
            }
            if (fw_length != 0 && recv_len >= fw_length) {
                break;
            }
        }
    }

    if (esp_ota_end(s_out_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        goto OTA_ERROR;
    }
    ota_in_progress = false;

    if (esp_ota_set_boot_partition(&partition) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed!");
        goto OTA_ERROR;
    }

    ESP_LOGI(TAG, "OTA success, restart in 0.2 seconds");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();

OTA_ERROR:
    if (ota_in_progress) {
        esp_ota_abort(s_out_handle);
        ota_in_progress = false;
    }
    s_ota_task_handle = NULL;
    ESP_LOGE(TAG, "OTA failed");
    vTaskDelete(NULL);
}

bool ble_ota_raw_ringbuf_init(uint32_t ringbuf_size)
{
    if (ringbuf_size == 0) {
        ringbuf_size = BLE_OTA_RAW_RINGBUF_DEFAULT_SIZE;
    }
    if (s_ota_task_handle != NULL) {
        ESP_LOGE(TAG, "Cannot reinitialize ring buffer while OTA task is running");
        return false;
    }
    if (s_ringbuf != NULL) {
        vRingbufferDelete(s_ringbuf);
    }
    s_ringbuf = xRingbufferCreate(ringbuf_size, RINGBUF_TYPE_BYTEBUF);
    return s_ringbuf != NULL;
}

bool ble_ota_raw_task_init(void)
{
    if (s_ringbuf == NULL) {
        ESP_LOGE(TAG, "Ring buffer not initialized, call ble_ota_raw_ringbuf_init first");
        return false;
    }

    if (s_ota_task_handle != NULL) {
        ESP_LOGW(TAG, "OTA task already created");
        return true;
    }

    BaseType_t ret = xTaskCreate(&ota_task, "ota_task", OTA_TASK_STACK_SIZE, NULL, 5, &s_ota_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        s_ota_task_handle = NULL;
        return false;
    }
    return true;
}

bool ble_ota_raw_recv_fw_cb(const uint8_t *buf, uint32_t length)
{
    if (buf == NULL || length == 0) {
        ESP_LOGE(TAG, "Invalid firmware chunk input");
        return false;
    }

    if (s_ringbuf == NULL) {
        ESP_LOGE(TAG, "Ring buffer not initialized");
        return false;
    }

    if (write_to_ringbuf(buf, length) != length) {
        ESP_LOGE(TAG, "Failed to write firmware chunk to ring buffer");
        return false;
    }
    return true;
}
