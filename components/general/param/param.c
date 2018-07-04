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
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "nvs.h"
#include "iot_param.h"

#define PARAM_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        goto ret;                                                      \
        }

#define PARAM_ERR_ASSERT(tag, param, ret)  PARAM_CHECK(tag, (param) == ESP_OK, ret)
#define PARAM_POINT_ASSERT(tag, param, ret) PARAM_CHECK(tag, (param) != NULL, ret)

static const char* TAG = "param";

esp_err_t iot_param_save(const char* space_name, const char* key, void *param, uint16_t len)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_POINT_ASSERT(TAG, space_name, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, key, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, param, OPEN_FAIL);
    nvs_handle my_handle;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_ERR_ASSERT(TAG, ret, OPEN_FAIL);
    ret = nvs_set_blob(my_handle, key, param, len);
    PARAM_ERR_ASSERT(TAG, ret, SAVE_FINISH);
    ret = nvs_commit(my_handle);

SAVE_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}

esp_err_t iot_param_load(const char* space_name, const char* key, void* dest)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_POINT_ASSERT(TAG, space_name, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, key, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, dest, OPEN_FAIL);
    nvs_handle my_handle;
    size_t required_size = 0;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_ERR_ASSERT(TAG, ret, OPEN_FAIL);
    ret = nvs_get_blob(my_handle, key, NULL, &required_size);
    PARAM_ERR_ASSERT(TAG, ret, LOAD_FINISH)
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

esp_err_t iot_param_erase(const char* space_name, const char* key)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_POINT_ASSERT(TAG, space_name, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, key, OPEN_FAIL);
    nvs_handle my_handle;
    ret = nvs_open(space_name, NVS_READWRITE, &my_handle);
    PARAM_ERR_ASSERT(TAG, ret, OPEN_FAIL);
    ret = nvs_erase_key(my_handle, key);
    PARAM_ERR_ASSERT(TAG, ret, ERASE_FINISH)
    ret = nvs_commit(my_handle);

ERASE_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}

