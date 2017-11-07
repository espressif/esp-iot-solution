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
#define PARAM_POINT_ASSERT(tag, param, ret)	PARAM_CHECK(tag, (param) != NULL, ret)

static const char* TAG = "param";

esp_err_t iot_param_save(const char* namespace, const char* key, void *param, uint16_t len)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_POINT_ASSERT(TAG, namespace, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, key, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, param, OPEN_FAIL);
    nvs_handle my_handle;
    ret = nvs_open(namespace, NVS_READWRITE, &my_handle);
    PARAM_ERR_ASSERT(TAG, ret, OPEN_FAIL);
    ret = nvs_set_blob(my_handle, key, param, len);
    PARAM_ERR_ASSERT(TAG, ret, SAVE_FINISH);
    ret = nvs_commit(my_handle);

SAVE_FINISH:
    nvs_close(my_handle);

OPEN_FAIL:
    return ret;
}

esp_err_t iot_param_load(const char* namespace, const char* key, void* dest)
{
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    PARAM_POINT_ASSERT(TAG, namespace, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, key, OPEN_FAIL);
    PARAM_POINT_ASSERT(TAG, dest, OPEN_FAIL);
    nvs_handle my_handle;
    size_t required_size = 0;
    ret = nvs_open(namespace, NVS_READWRITE, &my_handle);
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
