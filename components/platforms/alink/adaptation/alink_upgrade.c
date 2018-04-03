/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "esp_alink.h"
#include "esp_alink_log.h"

static const char *TAG = "alink_upgrade";

/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
static esp_ota_handle_t update_handle = 0 ;
static const esp_partition_t *update_partition = NULL;
static int binary_file_length = 0;

void platform_flash_program_start(void)
{
    alink_err_t ret = 0;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    assert(configured == running); /* fresh from reset, should be running from configured boot partition */
    ALINK_LOGI("Running partition type %d subtype %d (offset 0x%08x)",
               configured->type, configured->subtype, configured->address);

    update_partition = esp_ota_get_next_update_partition(NULL);
    ALINK_LOGI("Writing to partition subtype %d at offset 0x%x",
               update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

    ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ; , "esp_ota_begin failed, retor=%d", ret);

    ALINK_LOGI("esp_ota_begin succeeded");
}

int platform_flash_program_write_block(_IN_ char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK(length);
    ALINK_PARAM_CHECK(buffer);

    alink_err_t ret = 0;

    ret = esp_ota_write(update_handle, (const void *)buffer, length);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "Error: esp_ota_write failed! ret=0x%x", ret);

    binary_file_length += length;
    ALINK_LOGI("Have written image length %d", binary_file_length);

    return ALINK_OK;
}

int platform_flash_program_stop(void)
{
    alink_err_t ret = 0;
    ALINK_LOGI("Total Write binary data length : %d", binary_file_length);

    ret = esp_ota_end(update_handle);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "esp_ota_end failed!");

    ret = esp_ota_set_boot_partition(update_partition);
    ALINK_ERROR_CHECK(ret != ESP_OK, ALINK_ERR, "esp_ota_set_boot_partition failed! ret=0x%x", ret);

    return ALINK_OK;
}
