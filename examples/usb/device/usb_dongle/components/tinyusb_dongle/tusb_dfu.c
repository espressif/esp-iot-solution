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

#include "esp_log.h"
#include "tusb.h"
#include "tinyusb.h"
#include "tinyusb_dfu_ota.h"

static char* TAG = "esp_dfu";

//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    if (state == DFU_DNBUSY) {
        // For this example
        // - Atl0 Flash is fast : 1   ms
        // - Alt1 EEPROM is slow: 100 ms
        return (alt == 0) ? 1 : 100;
    } else if (state == DFU_MANIFEST) {
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
