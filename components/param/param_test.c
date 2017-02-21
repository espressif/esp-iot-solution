#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "param.h"

#define PARAM_SEC   0x3d
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
    ESP_LOGI(TAG, "param write a:%d, b:%d", param.a, param.b);
    param_save(PARAM_SEC, &param, sizeof(param_t));
    param_load(PARAM_SEC, &param_read, sizeof(param_t));
    ESP_LOGI(TAG, "param read a:%d, b:%d", param_read.a, param_read.b);
    
    param.a = 77;
    param.b = 88;
    ESP_LOGI(TAG, "param write a:%d, b:%d", param.a, param.b);
    param_save(PARAM_SEC, &param, sizeof(param_t));
    param_load(PARAM_SEC, &param_read, sizeof(param_t));
    ESP_LOGI(TAG, "param read a:%d, b:%d", param_read.a, param_read.b);
}