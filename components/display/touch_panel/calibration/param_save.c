// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "param_save.h"

static const char* TAG = "param save";

#define PARAM_CHECK(a, str, label) \
    if (!(a)) { \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        goto label; \
    }

esp_err_t param_save(const char* space_name, const char* key, void *param, uint16_t len)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_CHECK(NULL != space_name, "Pointer of space_name is invalid", OPEN_FAIL);
    PARAM_CHECK(NULL != key, "Pointer of key is invalid", OPEN_FAIL);
    PARAM_CHECK(NULL != param, "Pointer of param is invalid", OPEN_FAIL);
    nvs_handle_t my_handle;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_CHECK(ESP_OK == ret, "nvs open failed", OPEN_FAIL);
    ret = nvs_set_blob(my_handle, key, param, len);
    PARAM_CHECK(ESP_OK == ret, "nvs set blob failed", SAVE_FINISH);
    ret = nvs_commit(my_handle);

SAVE_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}

esp_err_t param_load(const char* space_name, const char* key, void* dest)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_CHECK(NULL != space_name, "Pointer of space_name is invalid", OPEN_FAIL);
    PARAM_CHECK(NULL != key, "Pointer of key is invalid", OPEN_FAIL);
    PARAM_CHECK(NULL != dest, "Pointer of dest is invalid", OPEN_FAIL);
    nvs_handle_t my_handle;
    size_t required_size = 0;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_CHECK(ESP_OK == ret, "nvs open failed", OPEN_FAIL);
    ret = nvs_get_blob(my_handle, key, NULL, &required_size);
    PARAM_CHECK(ESP_OK == ret, "nvs get blob failed", LOAD_FINISH);
    if (required_size == 0) {
        ESP_LOGW(TAG, "the target you want to load has never been saved");
        ret = ESP_FAIL;
        goto LOAD_FINISH;
    }
    ret = nvs_get_blob(my_handle, key, dest, &required_size);

LOAD_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}

esp_err_t param_erase(const char* space_name, const char* key)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_CHECK(NULL != space_name, "Pointer of space_name is invalid", OPEN_FAIL);
    PARAM_CHECK(NULL != key, "Pointer of key is invalid", OPEN_FAIL);
    nvs_handle_t my_handle;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_CHECK(ESP_OK == ret, "nvs open failed", OPEN_FAIL);
    ret = nvs_erase_key(my_handle, key);
    PARAM_CHECK(ESP_OK == ret, "nvs erase failed", ERASE_FINISH);
    ret = nvs_commit(my_handle);

ERASE_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}
