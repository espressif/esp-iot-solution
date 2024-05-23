/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tusb_tasks.h"

const static char *TAG = "tusb_tsk";
static TaskHandle_t s_tusb_tskh;

#if CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
const static int INIT_OK = BIT0;
const static int INIT_FAILED = BIT1;
#endif

/**
 * @brief This top level thread processes all usb events and invokes callbacks
 */
static void tusb_device_task(void *arg)
{
    ESP_LOGD(TAG, "tinyusb task started");
#if CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
    EventGroupHandle_t *init_flags = arg;
    if (!tusb_init()) {
        ESP_LOGI(TAG, "Init TinyUSB stack failed");
        xEventGroupSetBits(*init_flags, INIT_FAILED);
        vTaskDelete(NULL);
    }
    ESP_LOGD(TAG, "tinyusb task has been initialized");
    xEventGroupSetBits(*init_flags, INIT_OK);
#endif // CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
    while (1) { // RTOS forever loop
        tud_task();
    }
}

esp_err_t tusb_run_task(void)
{
    // This function is not guaranteed to be thread safe, if invoked multiple times without calling `tusb_stop_task`, will cause memory leak
    // doing a sanity check anyway
    ESP_RETURN_ON_FALSE(!s_tusb_tskh, ESP_ERR_INVALID_STATE, TAG, "TinyUSB main task already started");

    void *task_arg = NULL;
#if CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
    // need to synchronize to potentially report issue if init failed
    EventGroupHandle_t init_flags = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(init_flags, ESP_ERR_NO_MEM, TAG, "Failed to allocate task sync flags");
    task_arg = &init_flags;
#endif
    // Create a task for tinyusb device stack:
    xTaskCreatePinnedToCore(tusb_device_task, "TinyUSB", CONFIG_TINYUSB_TASK_STACK_SIZE, task_arg, CONFIG_TINYUSB_TASK_PRIORITY, &s_tusb_tskh, CONFIG_TINYUSB_TASK_AFFINITY);
    ESP_RETURN_ON_FALSE(s_tusb_tskh, ESP_FAIL, TAG, "create TinyUSB main task failed");
#if CONFIG_TINYUSB_INIT_IN_DEFAULT_TASK
    // wait until tusb initialization has completed
    EventBits_t bits = xEventGroupWaitBits(init_flags, INIT_OK | INIT_FAILED, pdFALSE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(init_flags);
    ESP_RETURN_ON_FALSE(bits & INIT_OK, ESP_FAIL, TAG, "Init TinyUSB stack failed");
#endif

    return ESP_OK;
}

esp_err_t tusb_stop_task(void)
{
    ESP_RETURN_ON_FALSE(s_tusb_tskh, ESP_ERR_INVALID_STATE, TAG, "TinyUSB main task not started yet");
    vTaskDelete(s_tusb_tskh);
    s_tusb_tskh = NULL;
    return ESP_OK;
}
