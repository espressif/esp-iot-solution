/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include <spi_flash_mmap.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include "esp_mmap_assets.h"

static const char *TAG = "mmap_assets";

/**
 * @brief Asset table structure, contains detailed information for each asset.
 */
#pragma pack(1)
typedef struct {
    char asset_name[32];  /*!< Name of the asset */
    int asset_size;          /*!< Size of the asset */
    int asset_offset;        /*!< Offset of the asset */
    int asset_width;         /*!< Width of the asset */
    int asset_height;        /*!< Height of the asset */
} mmap_assets_table_t;
#pragma pack()

typedef struct {
    const char *asset_mem;
    const mmap_assets_table_t *table;
} mmap_assets_item_t;

typedef struct {
    esp_partition_mmap_handle_t * mmap_handle;
    mmap_assets_item_t *item;
    int max_asset;
} mmap_assets_t;

esp_err_t mmap_assets_new(const mmap_assets_config_t *config, mmap_assets_handle_t *ret_item)
{
    esp_err_t ret = ESP_OK;
    const void *root;
    mmap_assets_item_t *item = NULL;
    mmap_assets_t *map_asset = NULL;
    esp_partition_mmap_handle_t *mmap_handle = NULL;

    ESP_GOTO_ON_FALSE(config && ret_item, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    map_asset = (mmap_assets_t *)calloc(1, sizeof(mmap_assets_t));
    ESP_GOTO_ON_FALSE(map_asset, ESP_ERR_NO_MEM, err, TAG, "no mem for map_asset handle");

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, config->partition_label);
    ESP_GOTO_ON_FALSE(partition, ESP_ERR_NOT_FOUND, err, TAG, "Can not find %s in partition table", config->partition_label);

    int free_pages = spi_flash_mmap_get_free_pages(ESP_PARTITION_MMAP_DATA);
    uint32_t storage_size = free_pages * 64 * 1024;
    ESP_LOGD(TAG, "The storage free size is %ld KB", storage_size / 1024);
    ESP_LOGD(TAG, "The partition size is %ld KB", partition->size / 1024);
    ESP_GOTO_ON_FALSE((storage_size > partition->size), ESP_ERR_INVALID_SIZE, err, TAG, "The free size is less than %s partition required", partition->label);

    mmap_handle = (esp_partition_mmap_handle_t *)malloc(sizeof(esp_partition_mmap_handle_t));
    ESP_GOTO_ON_ERROR(esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &root, mmap_handle), err, TAG, "esp_partition_mmap failed");

    item = (mmap_assets_item_t *)malloc(sizeof(mmap_assets_item_t) * config->max_files);
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "no mem for asset item");

    if (config->max_files != *(int *)(root + 0)) {
        ret = ESP_ERR_INVALID_SIZE;
        ESP_LOGE(TAG, "File num mismatch: Generate %d, Stored %d", config->max_files, *(int *)(root + 0));
    }

    if (config->checksum != *(uint32_t *)(root + 4)) {
        ret = ESP_ERR_INVALID_CRC;
        ESP_LOGE(TAG, "Checksum mismatch: Generate 0x%x, Stored 0x%x", (unsigned int)config->checksum, *(unsigned int *)(root + 4));
    }

    mmap_assets_table_t *table = (mmap_assets_table_t *)(root + 8);
    for (int i = 0; i < config->max_files; i++) {
        (item + i)->asset_mem = (void *)(root + 8 + config->max_files * sizeof(mmap_assets_table_t) + table[i].asset_offset);
        (item + i)->table = (mmap_assets_table_t *)(table + i);
        ESP_LOGD(TAG, "[%d], offset:[%07d], size:[%d], name:%s, %p",
                 i,
                 (item + i)->table->asset_offset,
                 (item + i)->table->asset_size,
                 (item + i)->table->asset_name,
                 (item + i)->asset_mem);
    }

    map_asset->mmap_handle = mmap_handle;
    map_asset->item = item;
    map_asset->max_asset = config->max_files;
    *ret_item = (mmap_assets_handle_t)map_asset;

    ESP_LOGD(TAG, "new asset handle:@%p", map_asset);

    ESP_LOGI(TAG, "asset handle create success, version: %d.%d.%d",
             ESP_MMAP_ASSETS_VER_MAJOR, ESP_MMAP_ASSETS_VER_MINOR, ESP_MMAP_ASSETS_VER_PATCH);

    return ret;

err:
    if (item) {
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
        free(map_asset->item);
    }

    if (map_asset) {
        free(map_asset);
    }

    return ESP_OK;
}

const uint8_t * mmap_assets_get_mem(mmap_assets_handle_t handle, int index)
{
    assert(handle && "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    if (map_asset->max_asset > index) {
        return (const uint8_t *)(map_asset->item + index)->asset_mem;
    } else {
        ESP_LOGE(TAG, "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);
        return NULL;
    }
}

const char * mmap_assets_get_name(mmap_assets_handle_t handle, int index)
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
