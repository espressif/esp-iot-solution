/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "alink_obj.h"
#include "virtual_device.h"

static SemaphoreHandle_t xSemWriteInfo = NULL;
CAlink* p_alink;
static const char* TAG = "alink_test";

static virtual_device_t virtual_device = {
    0x01, 0x30, 0x50, 0, 0x01
};

static const char *main_dev_params =
    "{\"OnOff_Power\": { \"value\": \"%d\" }, \"Color_Temperature\": { \"value\": \"%d\" }, \"Light_Brightness\": { \"value\": \"%d\" }, \"TimeDelay_PowerOff\": { \"value\": \"%d\"}, \"WorkMode_MasterLight\": { \"value\": \"%d\"}}";

static void read_task_test(void *arg)
{
    for (;;) {
        char *down_cmd = (char *) malloc(ALINK_DATA_LEN);
        p_alink->read(down_cmd, ALINK_DATA_LEN, portMAX_DELAY);
        free(down_cmd);
        xSemaphoreGive(xSemWriteInfo);
    }
    vTaskDelete(NULL);
}

static void write_task_test(void *arg)
{
    for (;;) {
        xSemaphoreTake(xSemWriteInfo, portMAX_DELAY);
        char *up_cmd = (char *) malloc(ALINK_DATA_LEN);
        memset(up_cmd, 0, ALINK_DATA_LEN);
        sprintf(up_cmd, main_dev_params, virtual_device.power, virtual_device.temp_value, virtual_device.light_value,
            virtual_device.time_delay, virtual_device.work_mode);
        esp_err_t ret = p_alink->write(up_cmd, ALINK_DATA_LEN, 1000);
        free(up_cmd);
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "esp_write is err");
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

extern "C" void alink_test()
{
    if (xSemWriteInfo == NULL) {
        xSemWriteInfo = xSemaphoreCreateBinary();
    }
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(NULL, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    alink_product_t product_info;
    strcpy(product_info.sn, ALINK_INFO_SN);
    strcpy(product_info.name, ALINK_INFO_NAME);
    strcpy(product_info.version, ALINK_INFO_VERSION);
    strcpy(product_info.model, ALINK_INFO_MODEL);
    strcpy(product_info.key, ALINK_INFO_KEY);
    strcpy(product_info.secret, ALINK_INFO_SECRET);
    strcpy(product_info.key_sandbox, ALINK_INFO_SANDBOX_KEY);
    strcpy(product_info.secret_sandbox, ALINK_INFO_SANDBOX_SECRET);
    strcpy(product_info.type, ALINK_INFO_TYPE);
    strcpy(product_info.category, ALINK_INFO_CATEGORY);
    strcpy(product_info.manufacturer, ALINK_INFO_MANUFACTURER);
    strcpy(product_info.cid, ALINK_INFO_CID);
    ALINK_LOGI("*********************************");
    ALINK_LOGI("*         PRODUCT INFO          *");
    ALINK_LOGI("*********************************");
    ALINK_LOGI("name   : %s", product_info.name);
    ALINK_LOGI("type   : %s", product_info.type);
    ALINK_LOGI("version: %s", product_info.version);
    ALINK_LOGI("model  : %s", product_info.model);
    p_alink = CAlink::GetInstance(product_info);
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
}
