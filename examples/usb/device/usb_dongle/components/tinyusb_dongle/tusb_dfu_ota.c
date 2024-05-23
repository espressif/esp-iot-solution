/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * After device is enumerated in dfu mode run the following commands
 *
 * To transfer firmware from host to device (best to test with text file)
 *
 * $ dfu-util -d cafe -a 0 -D [filename]
 * $ dfu-util -d cafe -a 1 -D [filename]
 *
 * To transfer firmware from device to host:
 *
 * $ dfu-util -d cafe -a 0 -U [filename]
 * $ dfu-util -d cafe -a 1 -U [filename]
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tusb.h"
#include "tinyusb.h"
#include "nvs_flash.h"

#include "esp_app_format.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

static char* TAG = "esp_dfu_ota";

#define BUFFSIZE CONFIG_TINYUSB_DFU_BUFSIZE

static esp_ota_handle_t update_handle = 0;
static esp_partition_t const *update_partition = NULL;
static int binary_file_length = 0;
/*deal with all receive packet*/
static bool image_header_was_checked = false;

static const esp_partition_t *ota_partition;
static uint32_t current_index = 0;

esp_err_t ota_start(uint8_t const *ota_write_data, uint32_t data_read)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    if (image_header_was_checked == false) {
        esp_app_desc_t new_app_info;

        ESP_LOGI(TAG, "Starting OTA example");

        const esp_partition_t *running = esp_ota_get_running_partition();

        ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%"PRIx32")",
                 running->type, running->subtype, running->address);

        update_partition = esp_ota_get_next_update_partition(NULL);
        assert(update_partition != NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%"PRIx32"",
                 update_partition->subtype, update_partition->address);
        if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
            // check current version with downloading
            memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
            ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

            esp_app_desc_t running_app_info;
            if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
            }

            const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
            esp_app_desc_t invalid_app_info;
            if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
            }

            // check current version with last invalid partition
            if (last_invalid_app != NULL) {
                if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                    ESP_LOGW(TAG, "New version is the same as invalid version.");
                    ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                    ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
                }
            }

            image_header_was_checked = true;

            err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                return err;
            }
            ESP_LOGI(TAG, "esp_ota_begin succeeded");
        } else {
            ESP_LOGE(TAG, "received package is not fit len");
            esp_ota_abort(update_handle);
            return err;
        }
    }
    err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
    if (err != ESP_OK) {
        esp_ota_abort(update_handle);
        return err;
    }
    binary_file_length += data_read;
    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
    return err;
}

esp_err_t ota_complete(void)
{
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "OTA done, please restart system!");
    return ESP_OK;
}

static uint16_t upload_bin(uint8_t* data, uint16_t length)
{
    uint16_t read_size = length;
    if (ota_partition == NULL) {
        ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_MIN, NULL);
        if (ota_partition == NULL) {
            ESP_LOGE(TAG, "not found ota_0");
            return 0;
        }
        ESP_LOGI(TAG, "Bin size: %"PRIu32", addr: %p", ota_partition->size, (void *)ota_partition->address);
    }
    if (current_index > ota_partition->size) {
        ESP_LOGI(TAG, "read error %"PRIu32",%"PRIu32"\n", current_index, ota_partition->size);
        return 0;
    }

    if (current_index == ota_partition->size) {   // upload done
        ESP_LOGI(TAG, "Upload done");
        current_index = 0;
        ota_partition = NULL;
        return 0;
    } else if (current_index + read_size > ota_partition->size) {     // the last data
        read_size = ota_partition->size - current_index;
    }
    ESP_ERROR_CHECK(esp_partition_read(ota_partition, current_index, data, read_size));

    current_index += read_size;

    return read_size;
}

//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
    (void) block_num;
    (void) length;
    ESP_LOGI(TAG, "upload data,block_num: %d,length: %d\n", block_num, length);

    uint16_t xfer_len = upload_bin(data, length);

    return xfer_len;
}

// Invoked when the Host has terminated a download or upload transfer
void tud_dfu_abort_cb(uint8_t alt)
{
    (void) alt;
    ESP_LOGI(TAG, "Host aborted transfer\r\n");
}

// Invoked when a DFU_DETACH request is received
void tud_dfu_detach_cb(void)
{
    ESP_LOGI(TAG, "Host detach, upload/download done\r\n");
    //esp_restart();
}
