#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "param.h"
#include "unity.h"

#define PARAM_NAMESPACE     "param_save"
#define PARAM_KEY      "struct"
#define TAG     "param_test"

typedef struct {
    uint32_t a;
    uint32_t b;
} param_t;

void param_test()
{
    param_t param = {
        .a = 11,
        .b = 22,
    };
    param_t param_read= {
        .a = 99,
        .b = 99,
    };
    esp_err_t ret;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    ESP_LOGI(TAG, "heap size before param: %d", esp_get_free_heap_size());
    ret = param_load(PARAM_NAMESPACE, PARAM_KEY, &param_read);
    ESP_LOGI(TAG, "param read a:%d, b:%d", param_read.a, param_read.b);
    ESP_LOGI(TAG, "param_load return : %d", ret);
    ESP_LOGI(TAG, "param write a:%d, b:%d", param.a, param.b);
    param_save(PARAM_NAMESPACE, PARAM_KEY, &param, sizeof(param_t));
    param_load(PARAM_NAMESPACE, PARAM_KEY, &param_read);
    ESP_LOGI(TAG, "param read a:%d, b:%d", param_read.a, param_read.b);
    
    param.a = 77;
    param.b = 88;
    ESP_LOGI(TAG, "param write a:%d, b:%d", param.a, param.b);
    param_save(PARAM_NAMESPACE, PARAM_KEY, &param, sizeof(param_t));
    param_load(PARAM_NAMESPACE, PARAM_KEY, &param_read);
    ESP_LOGI(TAG, "param read a:%d, b:%d", param_read.a, param_read.b);
    ESP_LOGI(TAG, "heap size after param: %d", esp_get_free_heap_size());
}

TEST_CASE("Param test", "[param][iot]")
{
    param_test();
}
