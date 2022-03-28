/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
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
#include "esp_log.h"
#include "esp_err.h"
#include "dfu_device.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"

static char* TAG = "esp_dfu";

#define BUFFSIZE CONFIG_TINYUSB_DFU_BUFSIZE
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };

static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
static int binary_file_length = 0;
/*deal with all receive packet*/
static bool image_header_was_checked = false;

static esp_partition_t *ota_partition;
static uint32_t current_index = 0;

static esp_err_t ota_start(uint8_t* ota_write_data, uint32_t data_read)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */

    if (image_header_was_checked == false) {
        esp_app_desc_t new_app_info;

        ESP_LOGI(TAG, "Starting OTA example");

        const esp_partition_t *running = esp_ota_get_running_partition();

        ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
                running->type, running->subtype, running->address);

        update_partition = esp_ota_get_next_update_partition(NULL);
        assert(update_partition != NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
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
    err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
    if (err != ESP_OK) {
        esp_ota_abort(update_handle);
        return err;
    }
    binary_file_length += data_read;
    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
    return err;
}

static esp_err_t ota_complete(void)
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
    ESP_LOGI(TAG, "Bin size: %d, addr: %p", ota_partition->size, ota_partition->address);
  }
  if (current_index > ota_partition->size) {
    ESP_LOGI(TAG, "read error %d,%d\n", current_index, ota_partition->size);
    return 0;
  }

  if (current_index == ota_partition->size) {   // upload done
    ESP_LOGI(TAG, "Upload done");
    current_index = 0;
    free(ota_partition);
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
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  ESP_LOGI(TAG, "mounted\n");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  ESP_LOGI(TAG, "umounted\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{

}

//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
  if ( state == DFU_DNBUSY )
  {
    // For this example
    // - Atl0 Flash is fast : 1   ms
    // - Alt1 EEPROM is slow: 100 ms
    return (alt == 0) ? 1 : 100;
  }
  else if (state == DFU_MANIFEST)
  {
    // since we don't buffer entire image and do any flashing in manifest stage
    return 0;
  }

  return 0;
}

// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
  (void) alt;
  (void) block_num;

  ESP_LOGI(TAG, "Received Alt %u BlockNum %u of length %u", alt, block_num, length);

  esp_err_t ret = ota_start(data, length);

  if (ret == ESP_OK) {
    tud_dfu_finish_flashing(DFU_STATUS_OK);
  } else {
    tud_dfu_finish_flashing(DFU_STATUS_ERR_WRITE);
  }
}

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt)
{
  (void) alt;
  ESP_LOGI(TAG, "Download completed, enter manifestation\r\n");

  esp_err_t ret = ota_complete();
  // flashing op for manifest is complete without error
  // Application can perform checksum, should it fail, use appropriate status such as errVERIFY.
  if (ret == ESP_OK) {
    tud_dfu_finish_flashing(DFU_STATUS_OK);
  } else {
    tud_dfu_finish_flashing(DFU_STATUS_ERR_VERIFY);
  }

}

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