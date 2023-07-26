/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"

#include "esp_flash_encrypt.h"

#include "bootloader_common.h"

#include "bootloader_flash_priv.h"

#include "bootloader_custom_utility.h"

#define FLASH_SECTOR_SIZE                   0x1000

static const char *TAG = "boot_c_uti";

// Read ota_info partition and fill array from two otadata structures.
esp_err_t bootloader_custom_utility_read_otadata(const esp_partition_pos_t *ota_info, esp_ota_select_entry_t *two_otadata)
{
    const esp_ota_select_entry_t *ota_select_map;
    if (ota_info->offset == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // partition table has OTA data partition
    if (ota_info->size < 2 * SPI_SEC_SIZE) {
        ESP_LOGE(TAG, "ota_info partition size %" PRIu32 " is too small (minimum %d bytes)", ota_info->size, (2 * SPI_SEC_SIZE));
        return ESP_FAIL; // can't proceed
    }

    ESP_LOGD(TAG, "OTA data offset 0x%" PRIx32 "", ota_info->offset);
    ota_select_map = bootloader_mmap(ota_info->offset, ota_info->size);
    if (!ota_select_map) {
        ESP_LOGE(TAG, "bootloader_mmap(0x%" PRIx32 ", 0x%" PRIx32 ") failed", ota_info->offset, ota_info->size);
        return ESP_FAIL; // can't proceed
    }

    memcpy(&two_otadata[0], ota_select_map, sizeof(esp_ota_select_entry_t));
    memcpy(&two_otadata[1], (uint8_t *)ota_select_map + SPI_SEC_SIZE, sizeof(esp_ota_select_entry_t));
    bootloader_munmap(ota_select_map);

    return ESP_OK;
}

static esp_err_t write_otadata(esp_ota_select_entry_t *otadata, uint32_t offset, bool write_encrypted)
{
    esp_err_t err = bootloader_flash_erase_sector(offset / FLASH_SECTOR_SIZE);
    if (err == ESP_OK) {
        err = bootloader_flash_write(offset, otadata, sizeof(esp_ota_select_entry_t), write_encrypted);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error in write_otadata operation. err = 0x%x", err);
    }
    return err;
}

static esp_ota_img_states_t set_new_state_otadata(void)
{
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    ESP_LOGD(TAG, "Monitoring the first boot of the app is enabled.");
    return ESP_OTA_IMG_NEW;
#else
    return ESP_OTA_IMG_UNDEFINED;
#endif
}

esp_err_t bootloader_custom_utility_updata_ota_data(const bootloader_state_t *bs, int boot_index)
{
    esp_ota_select_entry_t otadata[2];
    if (bootloader_custom_utility_read_otadata(&bs->ota_info, otadata) != ESP_OK) {
        return ESP_FAIL;
    }

    uint8_t ota_app_count = bs->app_count;
    if (boot_index >= ota_app_count) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "otadata[0] ota_seq: %" PRIu32 ", otadata[1] ota_seq: %" PRIu32 ", ota_app_count is %" PRIu8 "", otadata[0].ota_seq, otadata[1].ota_seq, ota_app_count);

    //esp32_idf use two sector for store information about which partition is running
    //it defined the two sector as ota data partition,two structure esp_ota_select_entry_t is saved in the two sector
    //named data in first sector as otadata[0], second sector data as otadata[1]
    //e.g.
    //if otadata[0].ota_seq == otadata[1].ota_seq == 0xFFFFFFFF,means ota info partition is in init status
    //so it will boot factory application(if there is),if there's no factory application,it will boot ota[0] application
    //if otadata[0].ota_seq != 0 and otadata[1].ota_seq != 0,it will choose a max seq ,and get value of max_seq%max_ota_app_number
    //and boot a subtype (mask 0x0F) value is (max_seq - 1)%max_ota_app_number,so if want switch to run ota[x],can use next formulas.
    //for example, if otadata[0].ota_seq = 4, otadata[1].ota_seq = 5, and there are 8 ota application,
    //current running is (5-1)%8 = 4,running ota[4],so if we want to switch to run ota[7],
    //we should add otadata[0].ota_seq (is 4) to 4 ,(8-1)%8=7,then it will boot ota[7]
    //if      A=(B - C)%D
    //then    B=(A + C)%D + D*n ,n= (0,1,2...)
    //so current ota app sub type id is x , dest bin subtype is y,total ota app count is n
    //seq will add (x + n*1 + 1 - seq)%n

    int active_otadata = bootloader_common_get_active_otadata(otadata);
    if (active_otadata != -1) {
        uint32_t seq = otadata[active_otadata].ota_seq;
        ESP_LOGD(TAG, "active otadata[%d].ota_seq is %" PRIu32 "", active_otadata, otadata[active_otadata].ota_seq);
        uint32_t i = 0;
        while (seq > (boot_index + 1) % ota_app_count + i * ota_app_count) {
            i++;
        }
        int next_otadata = (~active_otadata) & 1; // if 0 -> will be next 1. and if 1 -> will be next 0.
        otadata[next_otadata].ota_seq = (boot_index + 1) % ota_app_count + i * ota_app_count;
        otadata[next_otadata].ota_state = set_new_state_otadata();
        otadata[next_otadata].crc = bootloader_common_ota_select_crc(&otadata[next_otadata]);
        bool write_encrypted = esp_flash_encryption_enabled();
        ESP_LOGD(TAG, "next_otadata is %d and i=%" PRIu32 " and rewrite seq is %" PRIu32 "", next_otadata, i, otadata[next_otadata].ota_seq);
        return write_otadata(&otadata[next_otadata], bs->ota_info.offset + FLASH_SECTOR_SIZE * next_otadata, write_encrypted);
    } else {
        /* Both OTA slots are invalid, probably because unformatted... */
        int next_otadata = 0;
        otadata[next_otadata].ota_seq = boot_index + 1;
        otadata[next_otadata].ota_state = set_new_state_otadata();
        otadata[next_otadata].crc = bootloader_common_ota_select_crc(&otadata[next_otadata]);
        bool write_encrypted = esp_flash_encryption_enabled();
        ESP_LOGD(TAG, "next_otadata is %d and rewrite seq is %" PRIu32 "", next_otadata, otadata[next_otadata].ota_seq);
        return write_otadata(&otadata[next_otadata], bs->ota_info.offset + FLASH_SECTOR_SIZE * next_otadata, write_encrypted);
    }
}
