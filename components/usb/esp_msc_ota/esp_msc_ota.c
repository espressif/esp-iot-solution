/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_vfs.h"
#include "esp_msc_ota_help.h"
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

_Static_assert(DEFAULT_OTA_BUF_SIZE > (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) + 1), "OTA data buffer too small");

/* Setting event group */
#define MSC_CONNECT (1 << 0) // MSC device connect

typedef struct {
    EventGroupHandle_t mscEventGroup;
    bool bulk_flash_erase;
    const esp_partition_t *update_partition;
    esp_msc_ota_status_t status;
    char *ota_upgrade_buf;
    size_t ota_upgrade_buf_size;
    esp_ota_handle_t update_handle;
    const char *ota_bin_path;
    FILE *file;
    uint32_t binary_file_len;
    uint32_t binary_file_read_len;
} esp_msc_ota_t;

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
    if (esp_event_post(ESP_MSC_OTA_EVENT, event_id, event_data, event_data_size, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post msc_ota event: %s", ota_event_name_table[event_id]);
    }
}

static bool _file_exists(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

static void _esp_msc_host_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)arg;
    switch (event_id) {
    case ESP_MSC_HOST_CONNECT:
        ESP_LOGD(TAG, "MSC device connected");
        break;
    case ESP_MSC_HOST_DISCONNECT:
        xEventGroupClearBits(msc_ota->mscEventGroup, MSC_CONNECT);
        ESP_LOGD(TAG, "MSC device disconnected");
        break;
    case ESP_MSC_HOST_INSTALL:
        ESP_LOGD(TAG, "MSC device install");
        break;
    case ESP_MSC_HOST_UNINSTALL:
        ESP_LOGD(TAG, "MSC device uninstall");
        break;
    case ESP_MSC_HOST_VFS_REGISTER:
        xEventGroupSetBits(msc_ota->mscEventGroup, MSC_CONNECT);
        ESP_LOGD(TAG, "MSC VFS Register");
        break;
    case ESP_MSC_HOST_VFS_UNREGISTER:
        ESP_LOGD(TAG, "MSC device disconnected");
        break;
    default:
        break;
    }
}

esp_msc_ota_status_t esp_msc_ota_get_status(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    return msc_ota->status;
}

static esp_err_t _read_header(esp_msc_ota_t *msc_ota)
{
    EventBits_t bits = xEventGroupWaitBits(msc_ota->mscEventGroup, MSC_CONNECT, pdFALSE, pdFALSE, 0);
    MSC_OTA_CHECK(bits & MSC_CONNECT, "msc can't be disconnect", ESP_ERR_INVALID_STATE);
    /*
     * `data_read_size` holds number of bytes needed to read complete header.
     */
    int data_read_size = IMAGE_HEADER_SIZE;
    int data_read = 0;

    FILE *file = fopen(msc_ota->ota_bin_path, "rb");
    MSC_OTA_CHECK(file != NULL, "Failed to open file for reading", ESP_ERR_NOT_FOUND);

    fseek(file, 0, SEEK_END);
    uint32_t fileLength = ftell(file);
    msc_ota->binary_file_len = fileLength;
    rewind(file);
    data_read_size = data_read_size > fileLength ? fileLength : data_read_size;
    ESP_LOGI(TAG, "Reading file %s, size: %d, total size: %"PRIu32"", msc_ota->ota_bin_path, data_read_size, fileLength);

    BaseType_t ret = xSemaphoreTake(msc_read_semaphore, pdMS_TO_TICKS(10));
    MSC_OTA_CHECK(ret == pdTRUE, "take msc_read_semaphore failed", ESP_FAIL);
    data_read = fread(msc_ota->ota_upgrade_buf, 1, data_read_size, file);
    xSemaphoreGive(msc_read_semaphore);

    if (data_read == data_read_size) {
        msc_ota->binary_file_read_len = data_read;
        fclose(file);
        return ESP_OK;
    } else if (data_read > 0 && data_read < data_read_size) {
        msc_ota->binary_file_read_len = data_read;
        fclose(file);
        return ESP_OK;
    } else if (ferror(file)) {
        ESP_LOGI(TAG, "Error reading from file");
    } else if (feof(file)) {
        ESP_LOGI(TAG, "End of file reached.\n");
    }

    fclose(file);
    return ESP_FAIL;
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
    MSC_OTA_CHECK(msc_ota != NULL, "msc_ota can't be NULL", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(new_app_info != NULL, "new_app_info can't be NULL", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(msc_ota->status >= ESP_MSC_OTA_BEGIN, "Invalid state", ESP_ERR_INVALID_STATE);

    esp_err_t err = _read_header(msc_ota);
    MSC_OTA_CHECK(err == ESP_OK, "Failed to read header", ESP_FAIL);

    const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
    esp_app_desc_t *app_info = (esp_app_desc_t *)&msc_ota->ota_upgrade_buf[app_desc_offset];
    if (app_info->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Incorrect app descriptor magic");
        return ESP_FAIL;
    }

    memcpy(new_app_info, app_info, sizeof(esp_app_desc_t));
    return ESP_OK;
}

esp_err_t esp_msc_ota_begin(esp_msc_ota_config_t *config, esp_msc_ota_handle_t *handle)
{
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_START, NULL, 0);
    MSC_OTA_CHECK(config != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(handle != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);

    esp_msc_ota_t *msc_ota = calloc(1, sizeof(esp_msc_ota_t));
    MSC_OTA_CHECK(msc_ota != NULL, "Failed to allocate memory for esp_msc_ota_t", ESP_ERR_NO_MEM);

    msc_ota->mscEventGroup = xEventGroupCreate();

    esp_event_handler_register(ESP_MSC_HOST_EVENT, ESP_EVENT_ANY_ID, &_esp_msc_host_handler, msc_ota);

    ESP_LOGI(TAG, "Waiting for MSC device to connect...");
    EventBits_t bits = xEventGroupWaitBits(msc_ota->mscEventGroup, MSC_CONNECT, pdFALSE, pdFALSE, config->wait_msc_connect);
    MSC_OTA_CHECK_GOTO(bits & MSC_CONNECT, "TIMEOUT: MSC device not connected", msc_cleanup);

    msc_ota->update_partition = NULL;
    ESP_LOGI(TAG, "Starting OTA...");
    msc_ota->update_partition = esp_ota_get_next_update_partition(NULL);
    MSC_OTA_CHECK_GOTO(msc_ota->update_partition != NULL, "Failed to get next update partition", msc_cleanup);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32, msc_ota->update_partition->subtype, msc_ota->update_partition->address);

    // TODO: Support filename with *.bin
    if (!_file_exists(config->ota_bin_path)) {
        ESP_LOGE(TAG, "File %s does not exist, make sure the file name doesn't exceed 11 bytes", config->ota_bin_path);
        return ESP_ERR_NOT_FOUND;
    }
    msc_ota->ota_bin_path = config->ota_bin_path;

    int alloc_size = MAX(config->buffer_size, DEFAULT_OTA_BUF_SIZE);
    msc_ota->ota_upgrade_buf = (char *)malloc(alloc_size);
    MSC_OTA_CHECK_GOTO(msc_ota->ota_upgrade_buf != NULL, "Failed to allocate memory for OTA buffer", msc_cleanup);
    msc_ota->ota_upgrade_buf_size = alloc_size;
    msc_ota->bulk_flash_erase = config->bulk_flash_erase;

    *handle = (esp_msc_ota_handle_t)msc_ota;
    msc_ota->status = ESP_MSC_OTA_BEGIN;
    return ESP_OK;

msc_cleanup:
    esp_event_handler_unregister(ESP_MSC_HOST_EVENT, ESP_EVENT_ANY_ID, &_esp_msc_host_handler);
    vEventGroupDelete(msc_ota->mscEventGroup);
    free(msc_ota);
    return ESP_FAIL;
}

esp_err_t esp_msc_ota_perform(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(msc_ota->status >= ESP_MSC_OTA_BEGIN, "Invalid state", ESP_ERR_INVALID_STATE);
    esp_err_t err;
    int data_read = 0;
    const int erase_size = msc_ota->bulk_flash_erase ? OTA_SIZE_UNKNOWN : OTA_WITH_SEQUENTIAL_WRITES;
    switch (msc_ota->status) {
    case ESP_MSC_OTA_BEGIN: {
        err = esp_ota_begin(msc_ota->update_partition, erase_size, &msc_ota->update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            return err;
        }

        msc_ota->status = ESP_MSC_OTA_IN_PROGRESS;

        if (msc_ota->binary_file_len == 0) {
            if (_read_header(msc_ota) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read header");
                return ESP_FAIL;
            }
        }

        err = _ota_verify_chip_id(msc_ota->ota_upgrade_buf);
        MSC_OTA_CHECK(err == ESP_OK, "Failed to verify chip id", err);

        esp_msc_ota_dispatch_event(ESP_MSC_OTA_READY_UPDATE, NULL, 0);
        err = esp_ota_write(msc_ota->update_handle, (const void *)msc_ota->ota_upgrade_buf, msc_ota->binary_file_read_len);
        MSC_OTA_CHECK(err == ESP_OK, "esp_ota_write failed", err);
        return ESP_OK;

        break;
    }
    case ESP_MSC_OTA_IN_PROGRESS: {
        EventBits_t bits = xEventGroupWaitBits(msc_ota->mscEventGroup, MSC_CONNECT, pdFALSE, pdFALSE, 0);
        MSC_OTA_CHECK(bits & MSC_CONNECT, "msc can't be disconnect", ESP_ERR_INVALID_STATE);
        if (msc_ota->file == NULL) {

            FILE *file = fopen(msc_ota->ota_bin_path, "rb");
            MSC_OTA_CHECK(file != NULL, "Failed to open file for reading", ESP_ERR_NOT_FOUND);
            fseek(file, msc_ota->binary_file_read_len, SEEK_CUR);
            msc_ota->file = file;
        }
        uint32_t *fileLength = &msc_ota->binary_file_read_len;
        uint32_t *totalLength = &msc_ota->binary_file_len;
        if (*fileLength < *totalLength) {
            size_t readLength = *fileLength > msc_ota->ota_upgrade_buf_size ? msc_ota->ota_upgrade_buf_size : *fileLength;

            BaseType_t ret = xSemaphoreTake(msc_read_semaphore, pdMS_TO_TICKS(10));
            MSC_OTA_CHECK(ret == pdTRUE, "take msc_read_semaphore failed", ESP_FAIL);
            data_read = fread(msc_ota->ota_upgrade_buf, 1, readLength, msc_ota->file);
            xSemaphoreGive(msc_read_semaphore);

            MSC_OTA_CHECK(data_read > 0, "Failed to read file", ESP_ERR_INVALID_SIZE);
            err = esp_ota_write(msc_ota->update_handle, (const void *)msc_ota->ota_upgrade_buf, data_read);
            MSC_OTA_CHECK(err == ESP_OK, "esp_ota_write failed", err);
            *fileLength += data_read;
            // report progress
            float progress = (float)(*fileLength) / (float)(*totalLength);
            progress = progress > 1.0 ? 1.0 : progress;
            esp_msc_ota_dispatch_event(ESP_MSC_OTA_WRITE_FLASH, &progress, sizeof(progress));
            ESP_LOGD(TAG, "Progress: %f %%", progress * 100);
        }
        if (*fileLength >= *totalLength) {
            msc_ota->status = ESP_MSC_OTA_SUCCESS;
            fclose(msc_ota->file);
            return ESP_OK;
        }
        return ESP_OK;
        break;
    }
    default:
        ESP_LOGE(TAG, "Invalid ota state");
        return ESP_FAIL;
        break;
    }
}

esp_err_t esp_msc_ota_end(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(msc_ota->status >= ESP_MSC_OTA_BEGIN, "Invalid state", ESP_ERR_INVALID_STATE);
    esp_err_t err = ESP_OK;
    switch (msc_ota->status) {
    case ESP_MSC_OTA_SUCCESS:
    case ESP_MSC_OTA_IN_PROGRESS:
        if (msc_ota->ota_upgrade_buf) {
            free(msc_ota->ota_upgrade_buf);
        }
        break;
    default:
        ESP_LOGE(TAG, "Invalid ESP MSC OTA State");
        break;
    }
    if (msc_ota->status == ESP_MSC_OTA_SUCCESS) {
        err = esp_ota_set_boot_partition(msc_ota->update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        } else {
            esp_msc_ota_dispatch_event(ESP_MSC_OTA_UPDATE_BOOT_PARTITION, (void *)(&msc_ota->update_partition->subtype), sizeof(esp_partition_subtype_t));
        }
    }
    esp_event_handler_unregister(ESP_MSC_HOST_EVENT, ESP_EVENT_ANY_ID, &_esp_msc_host_handler);
    free(msc_ota);
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_FINISH, NULL, 0);
    return err;
}

esp_err_t esp_msc_ota_abort(esp_msc_ota_handle_t handle)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    MSC_OTA_CHECK(msc_ota->status >= ESP_MSC_OTA_BEGIN, "Invalid state", ESP_ERR_INVALID_STATE);
    esp_err_t err = ESP_OK;
    switch (msc_ota->status) {
    case ESP_MSC_OTA_SUCCESS:
    case ESP_MSC_OTA_IN_PROGRESS:
        if (msc_ota->file) {
            fclose(msc_ota->file);
        }
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
    esp_event_handler_unregister(ESP_MSC_HOST_EVENT, ESP_EVENT_ANY_ID, &_esp_msc_host_handler);
    free(msc_ota);
    esp_msc_ota_dispatch_event(ESP_MSC_OTA_ABORT, NULL, 0);
    return err;
}

bool esp_msc_ota_is_complete_data_received(esp_msc_ota_handle_t handle)
{
    bool ret = false;
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    ret = (msc_ota->binary_file_read_len == msc_ota->binary_file_len);
    return ret;
}

esp_err_t esp_msc_ota(esp_msc_ota_config_t *config)
{
    MSC_OTA_CHECK(config != NULL, "Invalid argument", ESP_ERR_INVALID_ARG);
    esp_msc_ota_handle_t msc_ota_handle = NULL;

    esp_err_t err = esp_msc_ota_begin(config, &msc_ota_handle);
    MSC_OTA_CHECK(err == ESP_OK, "esp_msc_ota_begin_fail", err);

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

esp_err_t esp_msc_ota_set_msc_connect_state(esp_msc_ota_handle_t handle, bool if_connect)
{
    esp_msc_ota_t *msc_ota = (esp_msc_ota_t *)handle;
    MSC_OTA_CHECK(msc_ota != NULL, "Invalid handle", ESP_ERR_INVALID_ARG);
    if (if_connect) {
        xEventGroupSetBits(msc_ota->mscEventGroup, MSC_CONNECT);
    } else {
        xEventGroupClearBits(msc_ota->mscEventGroup, MSC_CONNECT);
    }
    return ESP_OK;
}
