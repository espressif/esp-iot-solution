/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include <spi_flash_mmap.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_mmap_assets.h"

static const char *TAG = "mmap_assets";

#define ASSETS_FILE_NUM_OFFSET  0
#define ASSETS_CHECKSUM_OFFSET  4
#define ASSETS_TABLE_LEN        8
#define ASSETS_TABLE_OFFSET     12

#define ASSETS_FILE_MAGIC_HEAD  0x5A5A
#define ASSETS_FILE_MAGIC_LEN   2

/**
 * @brief Asset table structure, contains detailed information for each asset.
 */
#pragma pack(1)
typedef struct {
    char asset_name[CONFIG_MMAP_FILE_NAME_LENGTH];  /*!< Name of the asset */
    uint32_t asset_size;          /*!< Size of the asset */
    uint32_t asset_offset;        /*!< Offset of the asset */
    uint16_t asset_width;         /*!< Width of the asset */
    uint16_t asset_height;        /*!< Height of the asset */
} mmap_assets_table_t;
#pragma pack()

typedef struct {
    const char *asset_mem;
    const mmap_assets_table_t *table;
} mmap_assets_item_t;

typedef struct {
    esp_partition_mmap_handle_t *mmap_handle;
    const esp_partition_t *partition;
    mmap_assets_item_t *item;
    int max_asset;
    struct {
        unsigned int mmap_enable: 1;        /*!< Flag to indicate if memory-mapped I/O is enabled */
        unsigned int reserved: 31;          /*!< Reserved for future use */
    } flags;
    int stored_files;
} mmap_assets_t;

static uint32_t compute_checksum(const uint8_t *data, uint32_t length)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum & 0xFFFF;
}

esp_err_t mmap_assets_new(const mmap_assets_config_t *config, mmap_assets_handle_t *ret_item)
{
    esp_err_t ret = ESP_OK;
    const void *root = NULL;
    mmap_assets_item_t *item = NULL;
    mmap_assets_t *map_asset = NULL;
    esp_partition_mmap_handle_t *mmap_handle = NULL;

    ESP_GOTO_ON_FALSE(config && ret_item, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    map_asset = (mmap_assets_t *)calloc(1, sizeof(mmap_assets_t));
    ESP_GOTO_ON_FALSE(map_asset, ESP_ERR_NO_MEM, err, TAG, "no mem for map_asset handle");

    map_asset->flags.mmap_enable = config->flags.mmap_enable;

    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_GOTO_ON_FALSE(running, ESP_ERR_NOT_FOUND, err, TAG, "Can not find running partition");

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, config->partition_label);
    ESP_GOTO_ON_FALSE(partition, ESP_ERR_NOT_FOUND, err, TAG, "Can not find \"%s\" in partition table", config->partition_label);
    map_asset->partition = partition;

    int stored_files = 0;
    int stored_len = 0;
    uint32_t stored_chksum = 0;
    uint32_t calculated_checksum = 0;

    uint32_t offset = 0;
    uint32_t size = 0;

    if ((partition->address == running->address) && (partition->size == running->size)) {

        const esp_partition_pos_t part = {
            .offset = running->address,
            .size = running->size,
        };
        esp_image_metadata_t metadata;
        esp_image_get_metadata(&part, &metadata);

        offset = metadata.image_len;
        ESP_LOGD(TAG, "partition:%s(0x%x), offset:0x%X", partition->label, (unsigned int)partition->address, (unsigned int)offset);
        ESP_GOTO_ON_FALSE(partition->size > offset, ESP_ERR_INVALID_SIZE, err, TAG, "Partition %s size is insufficient", partition->label);
    }

    size = partition->size - offset;

    if (map_asset->flags.mmap_enable) {
        int free_pages = spi_flash_mmap_get_free_pages(ESP_PARTITION_MMAP_DATA);
        uint32_t storage_size = free_pages * 64 * 1024;
        ESP_LOGD(TAG, "The storage free size is %ld KB", storage_size / 1024);
        ESP_LOGD(TAG, "The partition size is %ld KB", size / 1024);
        ESP_GOTO_ON_FALSE((storage_size > size), ESP_ERR_INVALID_SIZE, err, TAG, "The free size is less than %s partition required", partition->label);

        mmap_handle = (esp_partition_mmap_handle_t *)malloc(sizeof(esp_partition_mmap_handle_t));
        ESP_GOTO_ON_ERROR(esp_partition_mmap(partition, offset, size, ESP_PARTITION_MMAP_DATA, &root, mmap_handle), err, TAG, "esp_partition_mmap failed");

        stored_files = *(int *)(root + ASSETS_FILE_NUM_OFFSET);
        stored_chksum = *(uint32_t *)(root + ASSETS_CHECKSUM_OFFSET);
        stored_len = *(uint32_t *)(root + ASSETS_TABLE_LEN);

        if (config->flags.full_check) {
            calculated_checksum = compute_checksum((uint8_t *)(root + ASSETS_TABLE_OFFSET), stored_len);
        }
    } else {
        esp_partition_read(partition, ASSETS_FILE_NUM_OFFSET, &stored_files, sizeof(stored_files));
        esp_partition_read(partition, ASSETS_CHECKSUM_OFFSET, &stored_chksum, sizeof(stored_chksum));
        esp_partition_read(partition, ASSETS_TABLE_LEN, &stored_len, sizeof(stored_len));

        if (config->flags.full_check) {
            uint32_t read_offset = ASSETS_TABLE_OFFSET;
            uint32_t bytes_left = stored_len;
            uint8_t buffer[1024];

            while (bytes_left > 0) {
                uint32_t read_size = (bytes_left > sizeof(buffer)) ? sizeof(buffer) : bytes_left;
                ESP_GOTO_ON_ERROR(esp_partition_read(partition, read_offset, buffer, read_size), err, TAG, "esp_partition_read failed");

                calculated_checksum += compute_checksum(buffer, read_size);
                read_offset += read_size;
                bytes_left -= read_size;
            }
            calculated_checksum &= 0xFFFF;
        }
    }

    if (config->flags.full_check) {
        ESP_GOTO_ON_FALSE(calculated_checksum == stored_chksum, ESP_ERR_INVALID_CRC, err, TAG, "bad full checksum");
    }

    if (config->flags.app_bin_check) {
        if (config->max_files != stored_files) {
            ret = ESP_ERR_INVALID_SIZE;
            ESP_LOGE(TAG, "bad input file num: Input %d, Stored %d", config->max_files, stored_files);
        }

        if (config->checksum != stored_chksum) {
            ret = ESP_ERR_INVALID_CRC;
            ESP_LOGE(TAG, "bad input chksum: Input 0x%x, Stored 0x%x", (unsigned int)config->checksum, (unsigned int)stored_chksum);
        }
    }

    map_asset->stored_files = stored_files;

    item = (mmap_assets_item_t *)malloc(sizeof(mmap_assets_item_t) * config->max_files);
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "no mem for asset item");

    if (map_asset->flags.mmap_enable) {
        mmap_assets_table_t *table = (mmap_assets_table_t *)(root + ASSETS_TABLE_OFFSET);
        for (int i = 0; i < config->max_files; i++) {
            (item + i)->table = (table + i);
            (item + i)->asset_mem = (void *)(root + ASSETS_TABLE_OFFSET + config->max_files * sizeof(mmap_assets_table_t) + table[i].asset_offset);
        }
    } else {
        mmap_assets_table_t *table = malloc(sizeof(mmap_assets_table_t) * config->max_files);
        for (int i = 0; i < config->max_files; i++) {
            esp_partition_read(partition, ASSETS_TABLE_OFFSET + i * sizeof(mmap_assets_table_t), (table + i), sizeof(mmap_assets_table_t));
            (item + i)->table = (table + i);
            (item + i)->asset_mem = (char *)(ASSETS_TABLE_OFFSET + config->max_files * sizeof(mmap_assets_table_t) + table[i].asset_offset);
        }
    }

    for (int i = 0; i < config->max_files; i++) {
        ESP_LOGD(TAG, "[%d], offset:[%" PRIu32 "], size:[%" PRIu32 "], name:%s, %p",
                 i,
                 (item + i)->table->asset_offset,
                 (item + i)->table->asset_size,
                 (item + i)->table->asset_name,
                 (item + i)->asset_mem);

        if (config->flags.metadata_check) {
            uint16_t magic_data, *magic_ptr = NULL;
            if (map_asset->flags.mmap_enable) {
                magic_ptr = (uint16_t *)(item + i)->asset_mem;
            } else {
                esp_partition_read(map_asset->partition, (int)(item + i)->asset_mem, &magic_data, ASSETS_FILE_MAGIC_LEN);
                magic_ptr = &magic_data;
            }
            ESP_GOTO_ON_FALSE(*magic_ptr == ASSETS_FILE_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG,
                              "(%s) bad file magic header", (item + i)->table->asset_name);
        }
    }

    map_asset->mmap_handle = mmap_handle;
    map_asset->item = item;
    map_asset->max_asset = config->max_files;
    *ret_item = (mmap_assets_handle_t)map_asset;

    ESP_LOGD(TAG, "new asset handle:@%p", map_asset);

    ESP_LOGI(TAG, "Partition '%s' successfully created, version: %d.%d.%d",
             config->partition_label, ESP_MMAP_ASSETS_VER_MAJOR, ESP_MMAP_ASSETS_VER_MINOR, ESP_MMAP_ASSETS_VER_PATCH);

    return ret;

err:
    if (item) {
        if (false == map_asset->flags.mmap_enable) {
            free((void *)(item + 0)->table);
        }
        free(item);
    }

    if (mmap_handle) {
        esp_partition_munmap(*mmap_handle);
        free(mmap_handle);
    }

    if (map_asset) {
        free(map_asset);
    }

    return ret;
}

esp_err_t mmap_assets_del(mmap_assets_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->mmap_handle) {
        esp_partition_munmap(*(map_asset->mmap_handle));
        free(map_asset->mmap_handle);
    }

    if (map_asset->item) {
        if (false == map_asset->flags.mmap_enable) {
            free((void *)(map_asset->item + 0)->table);
        }
        free(map_asset->item);
    }

    if (map_asset) {
        free(map_asset);
    }

    return ESP_OK;
}

int mmap_assets_get_stored_files(mmap_assets_handle_t handle)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    return map_asset->stored_files;
}

size_t mmap_assets_copy_mem(mmap_assets_handle_t handle, size_t offset, void *dest_buffer, size_t size)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (true == map_asset->flags.mmap_enable) {
        memcpy(dest_buffer, (void *)offset, size);
        return size;
    } else if (offset) {
        esp_partition_read(map_asset->partition, offset, dest_buffer, size);
        return size;
    } else {
        ESP_LOGE(TAG, "Invalid offset: %zu.", offset);
        return 0;
    }
}

const uint8_t *mmap_assets_get_mem(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (const uint8_t *)((map_asset->item + index)->asset_mem + ASSETS_FILE_MAGIC_LEN);
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return NULL;
    }
}

const char *mmap_assets_get_name(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (map_asset->item + index)->table->asset_name;
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return NULL;
    }
}

int mmap_assets_get_size(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (map_asset->item + index)->table->asset_size;
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return -1;
    }
}

int mmap_assets_get_width(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (map_asset->item + index)->table->asset_width;
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return -1;
    }
}

int mmap_assets_get_height(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (map_asset->item + index)->table->asset_height;
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return -1;
    }
}
