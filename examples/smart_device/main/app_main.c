/******************************************************************************
 * Copyright (C) 2014 -2016  Espressif System
 *
 * FileName: app_main.c
 *
 * Description:
 *
 * Modification history:
 * 2016/11/16, v0.0.1 create this file.
*******************************************************************************/
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "esp_system.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "cloud.h"

static const char *TAG = "app_main";
SemaphoreHandle_t xSemWriteInfo = NULL;

static void read_task_test(void *arg)
{
    for (;;) {
        cloud_read(portMAX_DELAY);
        xSemaphoreGive(xSemWriteInfo);
    }
    
    vTaskDelete(NULL);
}

void write_task_test(void *arg)
{
    esp_err_t ret;

    for (;;) {
        xSemaphoreTake(xSemWriteInfo, portMAX_DELAY);
        ret = cloud_write(500 / portTICK_PERIOD_MS);

        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "esp_write is err");
        }
        
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief This function is only for detecting memory leaks
 */
static void free_heap_task(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "free heap size: %d", esp_get_free_heap_size());
        vTaskDelay(5000 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}

void cloud_start_task(void *arg)
{
    if (xSemWriteInfo == NULL) {
        xSemWriteInfo = xSemaphoreCreateBinary();
    }

    cloud_init();
    xTaskCreate(read_task_test, "read_task_test", 1024 * 4, NULL, 9, NULL);
    xTaskCreate(write_task_test, "write_task_test", 1024 * 4, NULL, 4, NULL);
    vTaskDelete(NULL);
}

void iot_demo_start()
{
    ESP_LOGI(TAG, "mode: json, free_heap: %u\n", esp_get_free_heap_size());
    xTaskCreate(cloud_start_task, "alink_start_task", 1024 * 6, NULL, 9, NULL);
    xTaskCreate(free_heap_task, "free_heap_task", 1024 * 2, NULL, 3, NULL);
}

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main()
{
    nvs_flash_init();
    iot_demo_start();
}

