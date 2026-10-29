/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include <spi_flash_mmap.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_mmap_assets.h"

static const char *TAG = "mmap_assets";

/**
 * @brief Memory Mapped Assets (MMAP) Binary Data Structure
 *
 * +-------------------------------------------------------------------------+
 * |                      MMAP Assets Binary Header (32 Bytes)               |
 * +-------------------------------------------------------------------------+
 * | Offset | Length | Description                                           |
 * |--------|--------|-------------------------------------------------------|
 * | 0x00   |  4 B   | Magic Number ("MMAP")                                 |
 * | 0x04   |  4 B   | Version Number (e.g., 0x00010000 for v1.0.0)          |
 * | 0x08   |  4 B   | name_len: Fixed allocation for each asset name        |
 * | 0x0C   |  4 B   | files: Total number of bundled assets                 |
 * | 0x10   |  4 B   | checksum: CRC of the asset table                      |
 * | 0x14   |  4 B   | payload_len: Total length of combined Table and Data  |
 * | 0x18   |  8 B   | reserved: Reserved bytes for future expansion         |
 * +-------------------------------------------------------------------------+
 * |                      Asset Table Entries (table_len Bytes)              |
 * +-------------------------------------------------------------------------+
 * | For each asset (Iterates `files` times, stride = name_len + 12):        |
 * | Offset | Length     | Description                                       |
 * |--------|------------|---------------------------------------------------|
 * | +0     | name_len   | Null-terminated asset name string                 |
 * | +name  |  4 B       | Size of the asset payload                         |
 * | +name+4|  4 B       | Offset of payload relative to Data Payload block  |
 * | +name+8|  2 B       | Width (for graphic assets, 0 otherwise)           |
 * | +name+10| 2 B       | Height (for graphic assets, 0 otherwise)          |
 * +-------------------------------------------------------------------------+
 * |                             Data Payload                                |
 * +-------------------------------------------------------------------------+
 * | Contiguous blob containing all raw asset files, addressed via:          |
 * | Data Payload Start Offset = 32 (Header) + table_len                     |
 * | Asset Memory Pointer = Data Payload Start Offset + Asset Offset         |
 * |                                                                         |
 * | Sub-Asset Payload Structure (at Asset Memory Pointer):                  |
 * | Offset | Length     | Description                                       |
 * |--------|------------|---------------------------------------------------|
 * | +0     |  2 B       | ASSETS_FILE_MAGIC_HEAD (0x5A5A)                   |
 * | +2     | (size-2) B | Actual raw file / image data bytes                |
 * +-------------------------------------------------------------------------+
 */
const uint8_t MMAP_MAGIC_BYTES[4] = {'M', 'M', 'A', 'P'};

#define ASSETS_FILE_MAGIC_HEAD  0x5A5A
#define ASSETS_FILE_MAGIC_LEN   2

#pragma pack(1)
typedef struct {
    uint8_t magic[4];       /*!< Magic number: "MMAP" */
    uint32_t version;       /*!< Version number */
    uint32_t name_len;      /*!< Length of the asset name in table (including \0) */
    uint32_t files;         /*!< Total number of assets */
    uint32_t checksum;      /*!< Checksum of the table data */
    uint32_t payload_len;   /*!< Total length of combined Asset Table + Payload Data */
    uint32_t reserved[2];
} mmap_assets_header_t;
#pragma pack()

typedef struct {
    const char *asset_name;
    const char *asset_mem;
    uint32_t asset_size;
    uint32_t asset_offset;
    uint16_t asset_width;
    uint16_t asset_height;
} mmap_assets_item_t;

typedef struct {
    FILE *file_handle;                      /*!< File handle when using file system */
    esp_partition_mmap_handle_t *mmap_handle;
    const esp_partition_t *partition;
    mmap_assets_item_t *item;
    int max_asset;
    SemaphoreHandle_t mutex;          /*!< Mutex for file operations thread safety */
    struct {
        unsigned int mmap_enable: 1;        /*!< Flag to indicate if memory-mapped I/O is enabled */
        unsigned int use_fs: 1;             /*!< Flag to indicate if using file system instead of partition */
        unsigned int reserved: 30;          /*!< Reserved for future use */
    } flags;
    int stored_files;
    void *ram_table;                        /*!< Dynamically allocated table for non-mmap fetching */
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
    uint8_t *checksum_buffer = NULL;

    ESP_GOTO_ON_FALSE(config && ret_item, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    /* Allocate and initialize the main asset handle */
    map_asset = (mmap_assets_t *)calloc(1, sizeof(mmap_assets_t));
    ESP_GOTO_ON_FALSE(map_asset, ESP_ERR_NO_MEM, err, TAG, "no mem for map_asset handle");

    /* Initialize synchronization mutex for thread-safe access */
    map_asset->mutex = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(map_asset->mutex, ESP_ERR_NO_MEM, err, TAG, "no mem for mutex");

    /* Configure storage backend configuration flag (File System vs. Raw Partition) */
    map_asset->flags.use_fs = config->flags.use_fs;
    map_asset->flags.mmap_enable = config->flags.mmap_enable;

    if (config->flags.use_fs) {

        FILE *fp = fopen(config->partition_label, "rb");
        ESP_GOTO_ON_FALSE(fp, ESP_ERR_NOT_FOUND, err, TAG, "Can not open file \"%s\"", config->partition_label);
        map_asset->file_handle = fp;
        map_asset->flags.mmap_enable = 0;
    } else {
        const esp_partition_t *running = esp_ota_get_running_partition();
        ESP_GOTO_ON_FALSE(running, ESP_ERR_NOT_FOUND, err, TAG, "Can not find running partition");

        const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, config->partition_label);
        ESP_GOTO_ON_FALSE(partition, ESP_ERR_NOT_FOUND, err, TAG, "Can not find \"%s\" in partition table", config->partition_label);
        map_asset->partition = partition;
    }

    int stored_files = 0;
    int stored_len = 0;
    uint32_t stored_chksum = 0;
    uint32_t calculated_checksum = 0;

    uint32_t offset = 0;
    uint32_t size = 0;

    if (map_asset->flags.use_fs) {
        offset = 0;

        fseek(map_asset->file_handle, 0, SEEK_END);
        size = ftell(map_asset->file_handle);
        fseek(map_asset->file_handle, 0, SEEK_SET);
    } else {
        const esp_partition_t *partition = map_asset->partition;
        const esp_partition_t *running = esp_ota_get_running_partition();

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
    }

    /* Read and validate the MMAP binary format header */
    mmap_assets_header_t header = {0};

    if (map_asset->flags.use_fs) {
        fseek(map_asset->file_handle, 0, SEEK_SET);
        size_t br = fread(&header, sizeof(mmap_assets_header_t), 1, map_asset->file_handle);
        ESP_GOTO_ON_FALSE(br == 1, ESP_ERR_INVALID_SIZE, err, TAG, "fread header failed");
    } else if (map_asset->flags.mmap_enable) {
        const esp_partition_t *partition = map_asset->partition;
        int free_pages = spi_flash_mmap_get_free_pages(ESP_PARTITION_MMAP_DATA);
        uint32_t storage_size = free_pages * 64 * 1024;
        ESP_LOGD(TAG, "The storage free size is %ld KB", storage_size / 1024);
        ESP_LOGD(TAG, "The partition size is %ld KB", size / 1024);
        ESP_GOTO_ON_FALSE((storage_size > size), ESP_ERR_INVALID_SIZE, err, TAG, "The free size is less than %s partition required", partition->label);

        mmap_handle = (esp_partition_mmap_handle_t *)malloc(sizeof(esp_partition_mmap_handle_t));
        ESP_GOTO_ON_ERROR(esp_partition_mmap(partition, offset, size, ESP_PARTITION_MMAP_DATA, &root, mmap_handle), err, TAG, "esp_partition_mmap failed");

        memcpy(&header, root, sizeof(mmap_assets_header_t));
    } else {
        const esp_partition_t *partition = map_asset->partition;
        esp_partition_read(partition, 0, &header, sizeof(mmap_assets_header_t));
    }

    bool is_legacy = false;
    uint32_t stride = 0;
    uint32_t computed_table_len = 0;
    uint32_t table_header_offset = 0;

    if (memcmp(header.magic, MMAP_MAGIC_BYTES, 4) == 0) {
        // New format
        stored_files = header.files;
        stored_chksum = header.checksum;
        stored_len = header.payload_len;
        stride = header.name_len + 12;
        table_header_offset = sizeof(mmap_assets_header_t);
        computed_table_len = stored_files * stride;
    } else {
        // Legacy format fallback
        is_legacy = true;
        uint32_t *legacy_header = (uint32_t *)&header;
        uint32_t legacy_files = legacy_header[0];
        uint32_t legacy_chksum = legacy_header[1];
        uint32_t legacy_len = legacy_header[2];

        // Ensure we don't assume config parameter defaults if they contradict the binary
        stored_files = (config->max_files > 0) ? config->max_files : legacy_files;
        stored_chksum = (config->checksum > 0) ? config->checksum : legacy_chksum;
        stored_len = legacy_len;

        stride = CONFIG_MMAP_FILE_NAME_LENGTH + 12;
        table_header_offset = 12; // Legacy header is 12 bytes
        computed_table_len = stored_files * stride;

        ESP_LOGW(TAG, "Fallback to legacy format (name_len=%d)", CONFIG_MMAP_FILE_NAME_LENGTH);
    }

    /* Optional: Perform full checksum validation on the payload table */
    if (config->flags.full_check) {
        if (map_asset->flags.use_fs || !map_asset->flags.mmap_enable) {
            uint32_t read_offset = table_header_offset;
            uint32_t bytes_left = stored_len;
            checksum_buffer = malloc(1024);
            ESP_GOTO_ON_FALSE(checksum_buffer, ESP_ERR_NO_MEM, err, TAG, "no mem for checksum buffer");

            while (bytes_left > 0) {
                uint32_t read_size = (bytes_left > 1024) ? 1024 : bytes_left;
                if (map_asset->flags.use_fs) {
                    fseek(map_asset->file_handle, read_offset, SEEK_SET);
                    size_t bytes_read = fread(checksum_buffer, 1, read_size, map_asset->file_handle);
                    if (bytes_read != read_size) {
                        ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_SIZE, err, TAG, "fread failed");
                    }
                } else {
                    if (esp_partition_read(map_asset->partition, read_offset, checksum_buffer, read_size) != ESP_OK) {
                        ESP_GOTO_ON_ERROR(ESP_FAIL, err, TAG, "esp_partition_read failed");
                    }
                }

                calculated_checksum += compute_checksum(checksum_buffer, read_size);
                read_offset += read_size;
                bytes_left -= read_size;
            }
            free(checksum_buffer);
            checksum_buffer = NULL;
            calculated_checksum &= 0xFFFF;
        } else {
            calculated_checksum = compute_checksum((uint8_t *)(root + table_header_offset), stored_len);
        }
    }

    if (config->flags.full_check) {
        ESP_GOTO_ON_FALSE(calculated_checksum == stored_chksum, ESP_ERR_INVALID_CRC, err, TAG, "bad full checksum");
    }

    if (config->flags.app_bin_check) {
        if (config->max_files != stored_files) {
            ret = ESP_ERR_INVALID_SIZE;
            ESP_LOGE(TAG, "Asset count mismatch: App registered %d, Binary contains %d", config->max_files, stored_files);
        }

        if (config->checksum != stored_chksum) {
            ret = ESP_ERR_INVALID_CRC;
            ESP_LOGE(TAG, "Checksum mismatch: App expected 0x%04X, Binary contains 0x%04X", (unsigned int)config->checksum, (unsigned int)stored_chksum);
        }
    }

    map_asset->stored_files = stored_files;

    item = (mmap_assets_item_t *)malloc(sizeof(mmap_assets_item_t) * stored_files);
    ESP_GOTO_ON_FALSE(item, ESP_ERR_NO_MEM, err, TAG, "no mem for asset item");

    /* Populate the asset table by dynamically striding through the binary table block */
    if (map_asset->flags.mmap_enable) {
        const char *table_base = (const char *)root + table_header_offset;
        const char *data_base = table_base + computed_table_len;
        for (int i = 0; i < stored_files; i++) {
            const char *entry = table_base + i * stride;
            (item + i)->asset_name = entry;
            (item + i)->asset_size = *(uint32_t *)(entry + stride - 12);
            (item + i)->asset_offset = *(uint32_t *)(entry + stride - 8);
            (item + i)->asset_width = *(uint16_t *)(entry + stride - 4);
            (item + i)->asset_height = *(uint16_t *)(entry + stride - 2);
            (item + i)->asset_mem = data_base + (item + i)->asset_offset;
        }
    } else {
        char *ram_table = malloc(computed_table_len);
        ESP_GOTO_ON_FALSE(ram_table, ESP_ERR_NO_MEM, err, TAG, "no mem for ram_table");
        map_asset->ram_table = ram_table;

        if (map_asset->flags.use_fs) {
            fseek(map_asset->file_handle, table_header_offset, SEEK_SET);
            size_t bytes_read = fread(ram_table, 1, computed_table_len, map_asset->file_handle);
            ESP_GOTO_ON_FALSE(bytes_read == computed_table_len, ESP_ERR_INVALID_SIZE, err, TAG, "fread ram_table failed");
        } else {
            ret = esp_partition_read(map_asset->partition, table_header_offset, ram_table, computed_table_len);
            ESP_GOTO_ON_ERROR(ret, err, TAG, "esp_partition_read ram_table failed");
        }

        const char *data_base_offset = (const char *)(size_t)(table_header_offset + computed_table_len);

        for (int i = 0; i < stored_files; i++) {
            const char *entry = ram_table + i * stride;
            (item + i)->asset_name = entry;
            (item + i)->asset_size = *(uint32_t *)(entry + stride - 12);
            (item + i)->asset_offset = *(uint32_t *)(entry + stride - 8);
            (item + i)->asset_width = *(uint16_t *)(entry + stride - 4);
            (item + i)->asset_height = *(uint16_t *)(entry + stride - 2);
            (item + i)->asset_mem = data_base_offset + (item + i)->asset_offset;
        }
    }

    /* Optional: Verify sub-file metadata magic head numbers */
    for (int i = 0; i < stored_files; i++) {
        ESP_LOGD(TAG, "[%d], offset:[%" PRIu32 "], size:[%" PRIu32 "], name:%s, %p",
                 i,
                 (item + i)->asset_offset,
                 (item + i)->asset_size,
                 (item + i)->asset_name,
                 (item + i)->asset_mem);

        if (config->flags.metadata_check) {
            uint16_t magic_data, *magic_ptr = NULL;
            if (map_asset->flags.mmap_enable) {
                magic_ptr = (uint16_t *)(item + i)->asset_mem;
            } else if (map_asset->flags.use_fs) {
                fseek(map_asset->file_handle, (int)(size_t)(item + i)->asset_mem, SEEK_SET);
                fread(&magic_data, ASSETS_FILE_MAGIC_LEN, 1, map_asset->file_handle);
                magic_ptr = &magic_data;
            } else {
                esp_partition_read(map_asset->partition, (int)(size_t)(item + i)->asset_mem, &magic_data, ASSETS_FILE_MAGIC_LEN);
                magic_ptr = &magic_data;
            }
            ESP_GOTO_ON_FALSE(*magic_ptr == ASSETS_FILE_MAGIC_HEAD, ESP_ERR_INVALID_CRC, err, TAG,
                              "(%s) bad file magic header", (item + i)->asset_name);
        }
    }

    map_asset->mmap_handle = mmap_handle;
    map_asset->item = item;
    map_asset->max_asset = stored_files;
    *ret_item = (mmap_assets_handle_t)map_asset;

    ESP_LOGD(TAG, "new asset handle:@%p", map_asset);

    uint8_t bin_ver_major = is_legacy ? 0 : ((header.version >> 16) & 0xFF);
    uint8_t bin_ver_minor = is_legacy ? 0 : ((header.version >> 8) & 0xFF);
    uint8_t bin_ver_patch = is_legacy ? 0 : (header.version & 0xFF);

    ESP_LOGI(TAG, "MMAP Assets [%s] mounted successfully. (Lib: v%d.%d.%d, Bin: v%d.%d.%d)",
             config->partition_label,
             ESP_MMAP_ASSETS_VER_MAJOR, ESP_MMAP_ASSETS_VER_MINOR, ESP_MMAP_ASSETS_VER_PATCH,
             bin_ver_major, bin_ver_minor, bin_ver_patch);

    return ret;

err:
    if (checksum_buffer) {
        free(checksum_buffer);
    }
    if (item) {
        free(item);
    }
    if (map_asset && map_asset->ram_table) {
        free(map_asset->ram_table);
    }

    if (mmap_handle) {
        esp_partition_munmap(*mmap_handle);
        free(mmap_handle);
    }

    if (map_asset) {
        if (map_asset->file_handle) {
            fclose(map_asset->file_handle);
        }
        if (map_asset->mutex) {
            vSemaphoreDelete(map_asset->mutex);
        }
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

    if (map_asset->file_handle) {
        fclose(map_asset->file_handle);
    }

    if (map_asset->mutex) {
        vSemaphoreDelete(map_asset->mutex);
    }

    if (map_asset->item) {
        free(map_asset->item);
    }

    if (map_asset->ram_table) {
        free(map_asset->ram_table);
    }

    if (map_asset) {
        free(map_asset);
    }

    return ESP_OK;
}

int mmap_assets_get_stored_files(mmap_assets_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, -1, TAG, "handle is invalid");
    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    return map_asset->stored_files;
}

size_t mmap_assets_copy_mem(mmap_assets_handle_t handle, size_t offset, void *dest_buffer, size_t size)
{
    esp_err_t ret = ESP_OK;
    size_t bytes_copied = 0;

    ESP_RETURN_ON_FALSE(handle, 0, TAG, "handle is invalid");
    ESP_RETURN_ON_FALSE(dest_buffer, 0, TAG, "dest_buffer is invalid");
    ESP_RETURN_ON_FALSE(offset, 0, TAG, "Invalid offset: %zu", offset);

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);

    ESP_GOTO_ON_FALSE(xSemaphoreTake(map_asset->mutex, portMAX_DELAY) == pdTRUE,
                      ESP_ERR_TIMEOUT, err, TAG, "Failed to acquire file mutex");

    if (map_asset->flags.mmap_enable) {
        memcpy(dest_buffer, (void *)offset, size);
        bytes_copied = size;
    } else if (map_asset->flags.use_fs) {
        fseek(map_asset->file_handle, offset, SEEK_SET);
        bytes_copied = fread(dest_buffer, 1, size, map_asset->file_handle);

    } else {
        ret = esp_partition_read(map_asset->partition, offset, dest_buffer, size);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "esp_partition_read failed");
        bytes_copied = size;
    }

    xSemaphoreGive(map_asset->mutex);
    return bytes_copied;

err:
    if (map_asset->mutex) {
        xSemaphoreGive(map_asset->mutex);
    }
    return 0;
}

size_t mmap_assets_copy_by_index(mmap_assets_handle_t handle, int index, void *dest_buffer, size_t size)
{
    ESP_RETURN_ON_FALSE(handle, 0, TAG, "handle is invalid");
    ESP_RETURN_ON_FALSE(dest_buffer, 0, TAG, "dest_buffer is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, 0, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    uint32_t asset_size = (map_asset->item + index)->asset_size - ASSETS_FILE_MAGIC_LEN;
    if (size > asset_size) {
        size = asset_size;
    }

    const char *asset_mem = (map_asset->item + index)->asset_mem;

    if (map_asset->flags.mmap_enable) {
        memcpy(dest_buffer, asset_mem + ASSETS_FILE_MAGIC_LEN, size);
        return size;
    } else {
        size_t mem_offset = (size_t)asset_mem + ASSETS_FILE_MAGIC_LEN;
        return mmap_assets_copy_mem(handle, mem_offset, dest_buffer, size);
    }
}

const uint8_t *mmap_assets_get_mem(mmap_assets_handle_t handle, int index)
{
    ESP_RETURN_ON_FALSE(handle, NULL, TAG, "handle is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, NULL, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    return (const uint8_t *)((map_asset->item + index)->asset_mem + ASSETS_FILE_MAGIC_LEN);
}

const char *mmap_assets_get_name(mmap_assets_handle_t handle, int index)
{
    ESP_RETURN_ON_FALSE(handle, NULL, TAG, "handle is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, NULL, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    return (map_asset->item + index)->asset_name;
}

int mmap_assets_get_size(mmap_assets_handle_t handle, int index)
{
    ESP_RETURN_ON_FALSE(handle, -1, TAG, "handle is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, -1, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    return (map_asset->item + index)->asset_size;
}

int mmap_assets_get_width(mmap_assets_handle_t handle, int index)
{
    ESP_RETURN_ON_FALSE(handle, -1, TAG, "handle is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, -1, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    return (map_asset->item + index)->asset_width;
}

int mmap_assets_get_height(mmap_assets_handle_t handle, int index)
{
    ESP_RETURN_ON_FALSE(handle, -1, TAG, "handle is invalid");

    mmap_assets_t *map_asset = (mmap_assets_t *)(handle);
    ESP_RETURN_ON_FALSE(map_asset->max_asset > index, -1, TAG,
                        "Invalid index: %d. Maximum index is %d.", index, map_asset->max_asset);

    return (map_asset->item + index)->asset_height;
}
