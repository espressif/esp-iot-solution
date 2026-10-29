/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs.h"
#include "esp_check.h"
#include "esp_msc_ota.h"
#include "usb/usb_host.h"
#include "msc_host_vfs.h"
#include "esp_msc_host.h"

static const char *TAG = "esp_msc_ota";

ESP_EVENT_DEFINE_BASE(ESP_MSC_OTA_EVENT);

#define IMAGE_HEADER_SIZE (1024)

/* This is kept sufficiently large enough to cover image format headers
 * and also this defines default minimum OTA buffer chunk size */
#define DEFAULT_OTA_BUF_SIZE (IMAGE_HEADER_SIZE)

struct esp_msc_ota_ctx {
    bool bulk_flash_erase;
    bool file_locked;
    const esp_partition_t *update_partition;
    esp_msc_ota_status_t status;
    char *ota_upgrade_buf;
    size_t ota_upgrade_buf_size;
    esp_ota_handle_t update_handle;
    const char *ota_bin_path;
    esp_msc_host_handle_t host_handle;
    FILE *file;
    uint32_t binary_file_len;
    uint32_t binary_file_read_len;
};

typedef struct esp_msc_ota_ctx esp_msc_ota_t;

#define OTA_MIN_HEADER_SIZE (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
_Static_assert(DEFAULT_OTA_BUF_SIZE >= OTA_MIN_HEADER_SIZE, "OTA data buffer must fit the image header");

// Table to lookup ota event name
static const char *ota_event_name_table[] = {
    "ESP_MSC_OTA_START",
    "ESP_MSC_OTA_READY_UPDATE",
    "ESP_MSC_OTA_WRITE_FLASH",
    "ESP_MSC_OTA_FAILED",
    "ESP_MSC_OTA_GET_IMG_DESC",
    "ESP_MSC_OTA_VERIFY_CHIP_ID",
    "ESP_MSC_OTA_UPDATE_BOOT_PARTITION",
    "ESP_MSC_OTA_FINISH",
    "ESP_MSC_OTA_ABORT",
};

static void esp_msc_ota_dispatch_event(int32_t event_id, const void *event_data, size_t event_data_size)
{
    esp_err_t err = esp_event_post(ESP_MSC_OTA_EVENT, event_id, event_data, event_data_size, portMAX_DELAY);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to post msc_ota event: %s", ota_event_name_table[event_id]);
    }
}

static void esp_msc_ota_dispatch_failed(esp_err_t err)
{
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_FAILED, &err, sizeof(err));
}

static bool _file_exists(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

static bool _msc_is_connected(esp_msc_ota_t *msc_ota)
{
    return esp_msc_host_is_mounted(msc_ota->host_handle);
}

static esp_err_t _msc_lock(esp_msc_ota_t *msc_ota)
{
    return esp_msc_host_lock(msc_ota->host_handle, portMAX_DELAY);
}

static void _msc_unlock(esp_msc_ota_t *msc_ota)
{
    esp_msc_host_unlock(msc_ota->host_handle);
}

static esp_err_t _wait_for_msc_connect(esp_msc_ota_t *msc_ota, TickType_t timeout)
{
    return esp_msc_host_wait_mounted(msc_ota->host_handle, timeout);
}

static void _close_ota_file(esp_msc_ota_t *msc_ota)
{
    if (msc_ota->file) {
        fclose(msc_ota->file);
        msc_ota->file = NULL;
    }

    if (msc_ota->file_locked) {
        _msc_unlock(msc_ota);
        msc_ota->file_locked = false;
    }
}

esp_msc_ota_status_t esp_msc_ota_get_status(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    if (msc_ota == NULL) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_MSC_OTA_INIT;
    }
    return msc_ota->status;
}

static esp_err_t _read_header(esp_msc_ota_t *msc_ota)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(_msc_is_connected(msc_ota), ESP_ERR_INVALID_STATE, TAG, "msc can't be disconnect");

    /*
     * `data_read_size` holds the number of bytes needed to read the complete
     * image header. We deliberately require at least OTA_MIN_HEADER_SIZE bytes
     * so that esp_msc_ota_get_img_desc() can safely parse esp_app_desc_t.
     */
    int data_read_size = IMAGE_HEADER_SIZE;
    int data_read = 0;

    ret = _msc_lock(msc_ota);
    ESP_RETURN_ON_ERROR(ret, TAG, "lock msc host failed");
    FILE *file = fopen(msc_ota->ota_bin_path, "rb");
    if (file == NULL) {
        _msc_unlock(msc_ota);
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, TAG, "Failed to open file for reading");
    }

    ESP_GOTO_ON_FALSE(fseek(file, 0, SEEK_END) == 0, ESP_FAIL, file_close, TAG, "Failed to seek OTA file end");
    long file_pos = ftell(file);
    ESP_GOTO_ON_FALSE(file_pos >= 0 && file_pos <= UINT32_MAX, ESP_ERR_INVALID_SIZE, file_close, TAG, "Invalid OTA file size: %ld", file_pos);
    uint32_t fileLength = (uint32_t)file_pos;
    msc_ota->binary_file_len = fileLength;
    ESP_GOTO_ON_FALSE(fseek(file, 0, SEEK_SET) == 0, ESP_FAIL, file_close, TAG, "Failed to seek OTA file start");

    ESP_GOTO_ON_FALSE(fileLength >= OTA_MIN_HEADER_SIZE, ESP_ERR_INVALID_SIZE, file_close, TAG,
                      "File %s too small (%" PRIu32 " B) for OTA header (need >= %u B)",
                      msc_ota->ota_bin_path, fileLength, (unsigned)OTA_MIN_HEADER_SIZE);

    if ((uint32_t)data_read_size > fileLength) {
        data_read_size = (int)fileLength;
    }
    ESP_LOGI(TAG, "Reading file %s, size: %d, total size: %"PRIu32"", msc_ota->ota_bin_path, data_read_size, fileLength);

    data_read = fread(msc_ota->ota_upgrade_buf, 1, data_read_size, file);

    if (data_read < (int)OTA_MIN_HEADER_SIZE) {
        if (ferror(file)) {
            ESP_LOGE(TAG, "Error reading from file");
        } else if (feof(file)) {
            ESP_LOGE(TAG, "End of file reached before header complete");
        } else {
            ESP_LOGE(TAG, "Short read: got %d B, need >= %u B for header",
                     data_read, (unsigned)OTA_MIN_HEADER_SIZE);
        }
        ret = ESP_ERR_INVALID_SIZE;
        goto file_close;
    }

    msc_ota->binary_file_read_len = data_read;
    ret = ESP_OK;

file_close:
    fclose(file);
    _msc_unlock(msc_ota);
    return ret;
}

static esp_err_t _ota_verify_chip_id(const void *arg)
{
    esp_image_header_t *data = (esp_image_header_t *)(arg);
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_VERIFY_CHIP_ID, (void *)(&data->chip_id), sizeof(esp_chip_id_t));

    if (data->chip_id != CONFIG_IDF_FIRMWARE_CHIP_ID) {
        ESP_LOGE(TAG, "Mismatch chip id, expected %d, found %d", CONFIG_IDF_FIRMWARE_CHIP_ID, data->chip_id);
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

esp_err_t esp_msc_ota_get_img_desc(esp_msc_ota_handle_t handle, esp_app_desc_t *new_app_info)
{
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_GET_IMG_DESC, NULL, 0);

    // TODO: Support decrypt image

    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    ESP_RETURN_ON_FALSE(msc_ota != NULL, ESP_ERR_INVALID_ARG, TAG, "msc_ota can't be NULL");
    ESP_RETURN_ON_FALSE(new_app_info != NULL, ESP_ERR_INVALID_ARG, TAG, "new_app_info can't be NULL");
    ESP_RETURN_ON_FALSE(msc_ota->status >= ESP_MSC_OTA_BEGIN, ESP_ERR_INVALID_STATE, TAG, "Invalid state");

    esp_err_t err = _read_header(msc_ota);
    ESP_RETURN_ON_FALSE(err == ESP_OK, ESP_FAIL, TAG, "Failed to read header");

    const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    esp_app_desc_t *app_info = (esp_app_desc_t *)&msc_ota->ota_upgrade_buf[app_desc_offset];
    if (app_info->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Incorrect app descriptor magic");
        return ESP_FAIL;
    }

    memcpy(new_app_info, app_info, sizeof(esp_app_desc_t));
    return ESP_OK;
}

esp_err_t esp_msc_ota_begin(const esp_msc_ota_config_t *config, esp_msc_ota_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    ESP_RETURN_ON_FALSE(config->host_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "host_handle can't be NULL");
    ESP_RETURN_ON_FALSE(config->ota_bin_path != NULL, ESP_ERR_INVALID_ARG, TAG, "ota_bin_path can't be NULL");
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_START, NULL, 0);

    esp_err_t ret = ESP_OK;
    esp_msc_ota_t *msc_ota = calloc(1, sizeof(esp_msc_ota_t));
    ESP_GOTO_ON_FALSE(msc_ota != NULL, ESP_ERR_NO_MEM, msc_cleanup, TAG, "Failed to allocate memory for esp_msc_ota_t");

    msc_ota->host_handle = config->host_handle;

    ESP_LOGI(TAG, "Waiting for MSC VFS to mount...");
    ESP_GOTO_ON_ERROR(_wait_for_msc_connect(msc_ota, config->wait_msc_connect),
                      msc_cleanup, TAG, "TIMEOUT: MSC VFS not mounted");

    ESP_LOGI(TAG, "Starting OTA...");
    msc_ota->update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_GOTO_ON_FALSE(msc_ota->update_partition != NULL, ESP_FAIL, msc_cleanup, TAG,
                      "Failed to get next update partition");
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32,
             msc_ota->update_partition->subtype, msc_ota->update_partition->address);

    ESP_GOTO_ON_FALSE(_file_exists(config->ota_bin_path), ESP_ERR_NOT_FOUND, msc_cleanup, TAG,
                      "File %s does not exist, make sure the file name doesn't exceed 11 bytes",
                      config->ota_bin_path);
    msc_ota->ota_bin_path = config->ota_bin_path;

    int alloc_size = MAX(config->buffer_size, DEFAULT_OTA_BUF_SIZE);
    msc_ota->ota_upgrade_buf = (char *)malloc(alloc_size);
    ESP_GOTO_ON_FALSE(msc_ota->ota_upgrade_buf != NULL, ESP_ERR_NO_MEM, msc_cleanup, TAG,
                      "Failed to allocate memory for OTA buffer");
    msc_ota->ota_upgrade_buf_size = alloc_size;
    msc_ota->bulk_flash_erase = config->bulk_flash_erase;

    *handle = (esp_msc_ota_handle_t)msc_ota;
    msc_ota->status = ESP_MSC_OTA_BEGIN;
    return ESP_OK;

msc_cleanup:
    esp_msc_ota_dispatch_failed(ret);
    if (msc_ota != NULL) {
        if (msc_ota->ota_upgrade_buf) {
            free(msc_ota->ota_upgrade_buf);
        }
        free(msc_ota);
    }
    return ret;
}

esp_err_t esp_msc_ota_perform(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    ESP_RETURN_ON_FALSE(msc_ota != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(msc_ota->status >= ESP_MSC_OTA_BEGIN, ESP_ERR_INVALID_STATE, TAG, "Invalid state");
    esp_err_t err;
    int data_read = 0;
    const int erase_size = msc_ota->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
    switch (msc_ota->status) {
    case ESP_MSC_OTA_BEGIN: {
        err = esp_ota_begin(msc_ota->update_partition, erase_size, &msc_ota->update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            esp_msc_ota_dispatch_failed(err);
            return err;
        }

        msc_ota->status = ESP_MSC_OTA_IN_PROGRESS;

        if (msc_ota->binary_file_len == 0) {
            err = _read_header(msc_ota);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read header");
                esp_msc_ota_dispatch_failed(err);
                return err;
            }
        }

        err = _ota_verify_chip_id(msc_ota->ota_upgrade_buf);
        if (err != ESP_OK) {
            esp_msc_ota_dispatch_failed(err);
            ESP_RETURN_ON_ERROR(err, TAG, "Failed to verify chip id");
        }

        esp_msc_ota_dispatch_event(ESP_MSC_OTA_READY_UPDATE, NULL, 0);
        err = esp_ota_write(msc_ota->update_handle, (const void *)msc_ota->ota_upgrade_buf, msc_ota->binary_file_read_len);
        if (err != ESP_OK) {
            esp_msc_ota_dispatch_failed(err);
            ESP_RETURN_ON_ERROR(err, TAG, "esp_ota_write failed");
        }
        return ESP_OK;

        break;
    }
    case ESP_MSC_OTA_IN_PROGRESS: {
        if (!_msc_is_connected(msc_ota)) {
            esp_msc_ota_dispatch_failed(ESP_ERR_INVALID_STATE);
            ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_STATE, TAG, "msc can't be disconnect");
        }
        if (msc_ota->file == NULL) {

            err = _msc_lock(msc_ota);
            if (err != ESP_OK) {
                esp_msc_ota_dispatch_failed(err);
                ESP_RETURN_ON_ERROR(err, TAG, "lock msc host failed");
            }
            msc_ota->file_locked = true;
            FILE *file = fopen(msc_ota->ota_bin_path, "rb");
            if (file == NULL) {
                _msc_unlock(msc_ota);
                msc_ota->file_locked = false;
                esp_msc_ota_dispatch_failed(ESP_ERR_NOT_FOUND);
                ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_FOUND, TAG, "Failed to open file for reading");
            }
            fseek(file, msc_ota->binary_file_read_len, SEEK_CUR);
            msc_ota->file = file;
        }
        uint32_t *fileLength = &msc_ota->binary_file_read_len;
        uint32_t *totalLength = &msc_ota->binary_file_len;
        if (*fileLength < *totalLength) {
            size_t remaining = *totalLength - *fileLength;
            size_t readLength = remaining > msc_ota->ota_upgrade_buf_size ? msc_ota->ota_upgrade_buf_size : remaining;

            data_read = fread(msc_ota->ota_upgrade_buf, 1, readLength, msc_ota->file);

            if (data_read <= 0) {
                esp_msc_ota_dispatch_failed(ESP_ERR_INVALID_SIZE);
                ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_SIZE, TAG, "Failed to read file");
            }
            err = esp_ota_write(msc_ota->update_handle, (const void *)msc_ota->ota_upgrade_buf, data_read);
            if (err != ESP_OK) {
                esp_msc_ota_dispatch_failed(err);
                ESP_RETURN_ON_ERROR(err, TAG, "esp_ota_write failed");
            }
            *fileLength += data_read;
            // report progress
            float progress = (float)(*fileLength) / (float)(*totalLength);
            progress = progress > 1.0 ? 1.0 : progress;
            esp_msc_ota_dispatch_event(ESP_MSC_OTA_WRITE_FLASH, &progress, sizeof(progress));
            ESP_LOGD(TAG, "Progress: %f %%", progress * 100);
        }
        if (*fileLength >= *totalLength) {
            msc_ota->status = ESP_MSC_OTA_SUCCESS;
            _close_ota_file(msc_ota);
            return ESP_OK;
        }
        return ESP_OK;
        break;
    }
    default:
        ESP_LOGE(TAG, "Invalid ota state");
        esp_msc_ota_dispatch_failed(ESP_FAIL);
        return ESP_FAIL;
        break;
    }
}

esp_err_t esp_msc_ota_end(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    ESP_RETURN_ON_FALSE(msc_ota != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(msc_ota->status >= ESP_MSC_OTA_BEGIN, ESP_ERR_INVALID_STATE, TAG, "Invalid state");

    if (msc_ota->status != ESP_MSC_OTA_SUCCESS) {
        ESP_LOGE(TAG, "Invalid ESP MSC OTA State");
        esp_msc_ota_dispatch_failed(ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }

    _close_ota_file(msc_ota);
    if (msc_ota->ota_upgrade_buf) {
        free(msc_ota->ota_upgrade_buf);
        msc_ota->ota_upgrade_buf = NULL;
    }

    /* Finalize the OTA session: esp_ota_end() validates the written image and
     * releases update_handle. It must be called before esp_ota_set_boot_partition(). */
    esp_err_t err = esp_ota_end(msc_ota->update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed! err=0x%x", err);
        esp_msc_ota_dispatch_failed(err);
        free(msc_ota);
        esp_msc_ota_dispatch_event(ESP_MSC_OTA_FINISH, NULL, 0);
        return err;
    }

    err = esp_ota_set_boot_partition(msc_ota->update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        esp_msc_ota_dispatch_failed(err);
    } else {
        esp_msc_ota_dispatch_event(ESP_MSC_OTA_UPDATE_BOOT_PARTITION,
                                   (void *)(&msc_ota->update_partition->subtype),
                                   sizeof(esp_partition_subtype_t));
    }

    free(msc_ota);
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_FINISH, NULL, 0);
    return err;
}

esp_err_t esp_msc_ota_abort(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    ESP_RETURN_ON_FALSE(msc_ota != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_FALSE(msc_ota->status >= ESP_MSC_OTA_BEGIN, ESP_ERR_INVALID_STATE, TAG, "Invalid state");
    esp_err_t err = ESP_OK;
    switch (msc_ota->status) {
    case ESP_MSC_OTA_SUCCESS:
        // SUCCESS before esp_msc_ota_end() still owns the OTA update handle and buffer.
        [[fallthrough]];
    case ESP_MSC_OTA_IN_PROGRESS:
        _close_ota_file(msc_ota);
        err = esp_ota_abort(msc_ota->update_handle);
        [[fallthrough]];
    case ESP_MSC_OTA_BEGIN:
        if (msc_ota->ota_upgrade_buf) {
            free(msc_ota->ota_upgrade_buf);
        }
        break;
    default:
        err = ESP_ERR_INVALID_STATE;
        ESP_LOGE(TAG, "Invalid ESP MSC OTA State");
        break;
    }
    free(msc_ota);
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_ABORT, NULL, 0);
    return err;
}

bool esp_msc_ota_is_complete_data_received(esp_msc_ota_handle_t handle)
{
    bool ret = false;
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    ESP_RETURN_ON_FALSE(msc_ota != NULL, false, TAG, "Invalid handle");
    ret = (msc_ota->binary_file_read_len == msc_ota->binary_file_len);
    return ret;
}

esp_err_t esp_msc_ota(const esp_msc_ota_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    esp_msc_ota_handle_t msc_ota_handle = NULL;

    esp_err_t err = esp_msc_ota_begin(config, &msc_ota_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "esp_msc_ota_begin_fail");

    do {
        err = esp_msc_ota_perform(msc_ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_msc_ota_perform: (%s)", esp_err_to_name(err));
            break;
        }
    } while (!esp_msc_ota_is_complete_data_received(msc_ota_handle));

    if (esp_msc_ota_is_complete_data_received(msc_ota_handle)) {
        err = esp_msc_ota_end(msc_ota_handle);
    } else {
        err |= esp_msc_ota_abort(msc_ota_handle);
    }

    return err;
}
