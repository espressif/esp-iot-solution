/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esl_tag_storage.h"
#include "esl_nvs.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "tag_storage";

#define ESL_NVS_NAMESPACE       "esl_prod"
#define ESL_NVS_KEY_TAG_ASSOC   "tag_assoc"

esp_err_t esl_tag_storage_init(void)
{
    ESP_LOGI(TAG, "Tag storage backend: NVS metadata via esl_nvs");
    return esl_nvs_init();
}

esp_err_t esl_tag_storage_load_assoc(esl_tag_assoc_blob_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = sizeof(*out);
    ret = nvs_get_blob(handle, ESL_NVS_KEY_TAG_ASSOC, out, &len);
    nvs_close(handle);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    if (len != sizeof(*out)) {
        ESP_LOGW(TAG, "tag_assoc size mismatch (%u)", (unsigned)len);
        return ESP_ERR_INVALID_SIZE;
    }

    esl_tag_assoc_status_t st = esl_tag_assoc_validate(out);
    if (st != ESL_TAG_ASSOC_OK) {
        ESP_LOGW(TAG, "tag_assoc validate failed: %d", (int)st);
        if (st == ESL_TAG_ASSOC_ERR_CRC) {
            return ESP_ERR_INVALID_CRC;
        }
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t esl_tag_storage_save_assoc(esl_tag_assoc_blob_t *blob)
{
    if (blob == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esl_tag_assoc_prepare(blob);
    if (esl_tag_assoc_validate(blob) != ESL_TAG_ASSOC_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_blob(handle, ESL_NVS_KEY_TAG_ASSOC, blob, sizeof(*blob));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "tag_assoc saved (no secrets logged)");
    } else {
        ESP_LOGE(TAG, "tag_assoc save failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esl_tag_storage_erase_assoc(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(ESL_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_key(handle, ESL_NVS_KEY_TAG_ASSOC);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = ESP_OK;
    } else if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "tag_assoc erased");
    } else {
        ESP_LOGE(TAG, "tag_assoc erase failed: %s", esp_err_to_name(ret));
    }
    return ret;
}
