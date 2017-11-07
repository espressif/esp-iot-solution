#if 1

/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_alink.h"

#define BUFFSIZE 1024

static const char *TAG = "alink_upgrade";

/* operate handle : uninitialized value is zero ,every ota begin would exponential growth*/
static esp_ota_handle_t out_handle = 0;
static esp_partition_t operate_partition;

void platform_flash_program_start(void)
{
    esp_err_t err;
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if (esp_current_partition->type != ESP_PARTITION_TYPE_APP) {
        ALINK_LOGE("esp_current_partition->type != ESP_PARTITION_TYPE_APP");
        return;
    }

    esp_partition_t find_partition;
    memset(&operate_partition, 0, sizeof(esp_partition_t));
    /*choose which OTA image should we write to*/
    switch (esp_current_partition->subtype) {
    case ESP_PARTITION_SUBTYPE_APP_FACTORY:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    case  ESP_PARTITION_SUBTYPE_APP_OTA_0:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_1:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    default:
        break;
    }
    find_partition.type = ESP_PARTITION_TYPE_APP;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    assert(partition != NULL);
    memset(&operate_partition, 0, sizeof(esp_partition_t));
    err = esp_ota_begin( partition, OTA_SIZE_UNKNOWN, &out_handle);
    ALINK_ERROR_CHECK(err != ESP_OK, ; , "esp_ota_begin, err:%x", err);
    memcpy(&operate_partition, partition, sizeof(esp_partition_t));
    ALINK_LOGI("esp_ota_begin init OK");
}

int platform_flash_program_write_block(char *buffer, uint32_t length)
{
    ALINK_PARAM_CHECK(length <= 0);
    ALINK_PARAM_CHECK(buffer == NULL);

    esp_err_t err;
    static int binary_file_length = 0;
    char *ota_write_data = (char *)malloc(BUFFSIZE);
    ALINK_ERROR_CHECK(ota_write_data == NULL, ALINK_ERR, "malloc, err:%p", ota_write_data);
    memset(ota_write_data, 0, BUFFSIZE);
    memcpy(ota_write_data, buffer, length);
    err = esp_ota_write( out_handle, (const void *)ota_write_data, length);
    ALINK_ERROR_CHECK(err != ESP_OK, ALINK_ERR, "esp_ota_begin, err:%x", err);

    binary_file_length += length;
    ALINK_LOGI("Have written image length %d", binary_file_length);
    if (ota_write_data) free(ota_write_data);
    return 0;
}

int platform_flash_program_stop(void)
{
    esp_err_t err;
    err = esp_ota_end(out_handle);
    ALINK_ERROR_CHECK(err != ESP_OK, ALINK_ERR, "esp_ota_end failed! err:%d", err);

    err = esp_ota_set_boot_partition(&operate_partition);
    ALINK_ERROR_CHECK(err != ESP_OK, ALINK_ERR, "esp_ota_set_boot_partition failed! err:%d", err);

    return ESP_OK;
}
#endif

