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
#include "cloud.h"
#include "virtual_device.h"


#if CONFIG_DEMO_ENABLE
static const char *TAG = "app_main";
SemaphoreHandle_t xSemWriteInfo = NULL;
virtual_device_t virtual_device = {
    0x01, 0x30, 0x50, 0, 0x01
};

void read_task_test(void *arg)
{
    for (;;) {
        cloud_read(&virtual_device, portMAX_DELAY);
        virtual_device_write(virtual_device);
        xSemaphoreGive(xSemWriteInfo);
    }
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    esp_err_t ret;
    for (;;) {
        xSemaphoreTake(xSemWriteInfo, portMAX_DELAY);
        virtual_device_read(&virtual_device);
        ret = cloud_write(virtual_device, 500 / portTICK_PERIOD_MS);
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "esp_write is err");
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t cloud_event_handler(cloud_event_t event)
{
    switch (event) {
        case CLOUD_EVENT_STA_GOT_IP:
            virtual_device_net_write(VIRTUAL_DEV_CONNECTING_CLOUD);
            break;
        case CLOUD_EVENT_STA_DISCONNECTED:
            virtual_device_net_write(VIRTUAL_DEV_STA_DISCONNECTED);
            break;
        case CLOUD_EVENT_CLOUD_CONNECTED:
            virtual_device_net_write(VIRTUAL_DEV_CLOUD_CONNECTED);
            if (xSemWriteInfo == NULL) xSemWriteInfo = xSemaphoreCreateBinary();
            xSemaphoreGive(xSemWriteInfo);
            break;
        case CLOUD_EVENT_CLOUD_DISCONNECTED:
            if (cloud_sys_net_is_ready()) {
                virtual_device_net_write(VIRTUAL_DEV_CONNECTING_CLOUD);
            }
            break;
        case CLOUD_EVENT_GET_DEVICE_DATA:
            break;
        case CLOUD_EVENT_SET_DEVICE_DATA:
            break;
        case CLOUD_EVENT_POST_CLOUD_DATA:
            break;
        default:
            break;
    }
    return ESP_OK;
}

void iot_demo_start()
{
    ESP_LOGI(TAG, "mode: json, free_heap: %u\n", esp_get_free_heap_size());
    nvs_flash_init();
    if (xSemWriteInfo == NULL) {
        xSemWriteInfo = xSemaphoreCreateBinary();
    }

    virtual_device_init(xSemWriteInfo);
    cloud_init(cloud_event_handler);
    xTaskCreate(read_task_test, "read_task_test", 1024 * 8, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 8, NULL, 4, NULL);
    ESP_LOGI(TAG, "free_heap3:%u\n", esp_get_free_heap_size());
}
#endif

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
#if CONFIG_DEMO_ENABLE
    iot_demo_start();
#else
    printf("in app_main...\n");
#endif
}
