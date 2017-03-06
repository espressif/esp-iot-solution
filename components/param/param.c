#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "nvs.h"
#include "param.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

static const char* TAG = "param";

esp_err_t param_save(const char* namespace, const char* key, void *param, uint16_t len)
{
    nvs_handle my_handle;
    ERR_ASSERT(TAG, nvs_open(namespace, NVS_READWRITE, &my_handle));
    ERR_ASSERT(TAG, nvs_set_blob(my_handle, key, param, len));
    ERR_ASSERT(TAG, nvs_commit(my_handle));
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t param_load(const char* namespace, const char* key,void* dest)
{
    nvs_handle my_handle;
    uint16_t required_size = 0;
    ERR_ASSERT(TAG, nvs_open(namespace, NVS_READWRITE, &my_handle));
    ERR_ASSERT(TAG, nvs_get_blob(my_handle, key, NULL, &required_size));
    IOT_CHECK(TAG, required_size != 0, ESP_FAIL);
    ERR_ASSERT(TAG, nvs_get_blob(my_handle, key, dest, &required_size));
    nvs_close(my_handle);
    return ESP_OK;
}