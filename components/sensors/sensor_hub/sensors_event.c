// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensor_event.h"

#define TAG "SENSOR_LOOP"
#ifdef CONFIG_SENSOR_EVENT_LOOP_AUTO
volatile static int register_count = 0;
#endif
static esp_event_loop_handle_t h_sensors_loop = NULL;

esp_err_t sensors_event_loop_create(void)
{
    if (h_sensors_loop) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_event_loop_args_t loop_args = {
        .queue_size = CONFIG_SENSORS_EVENT_QUEUE_SIZE,
        .task_name = "sensors_evt",
        .task_stack_size = CONFIG_SENSORS_EVENT_STACK_SIZE,
#ifdef CONFIG_SENSORS_EVENT_TASK_PRIORITY
        .task_priority = CONFIG_SENSORS_EVENT_TASK_PRIORITY,
#else
        .task_priority = uxTaskPriorityGet(NULL),
#endif
        .task_core_id = 0
    };
    esp_err_t err = esp_event_loop_create(&loop_args, &h_sensors_loop);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "event loop created error");
        return err;
    }
    ESP_LOGI(TAG, "event loop created succeed");
    return ESP_OK;
}

esp_err_t sensors_event_loop_delete(void)
{
    if (!h_sensors_loop) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_event_loop_delete(h_sensors_loop);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "delete event loop failed");
        return err;
    }
    ESP_LOGI(TAG, "delete event loop succeed");
    h_sensors_loop = NULL;
    return ESP_OK;
}


esp_err_t sensors_event_handler_instance_register(esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_t event_handler,
        void *event_handler_arg,
        esp_event_handler_instance_t *context)
{
#ifdef CONFIG_SENSOR_EVENT_LOOP_AUTO
    if (h_sensors_loop == NULL) {
        /* creat event loop if not inited*/
        if (ESP_OK != sensors_event_loop_create()) {
            return ESP_FAIL;
        }
        register_count = 0;
    }
#else
    if (h_sensors_loop == NULL) {
        ESP_LOGE(TAG, "event loop must init first");
        return ESP_ERR_INVALID_STATE;
    }
#endif

    esp_err_t ret = esp_event_handler_instance_register_with(h_sensors_loop, event_base, event_id, event_handler, event_handler_arg, context);

#ifdef CONFIG_SENSOR_EVENT_LOOP_AUTO
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register a new handler to event loop failed");
        return ret;
    }
    register_count++;
    ESP_LOGI(TAG, "register a new handler to event loop succeed");
#endif

    return ret;
}

esp_err_t sensors_event_handler_instance_unregister(esp_event_base_t event_base,
        int32_t event_id,
        esp_event_handler_instance_t context)
{
    if (h_sensors_loop == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_event_handler_instance_unregister_with(h_sensors_loop, event_base, event_id, context);

#ifdef CONFIG_SENSOR_EVENT_LOOP_AUTO
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "unregister handler from event loop failed");
        return ret;
    }
    register_count--;
    ESP_LOGI(TAG, "unregister handler from event loop succeed");
    if (register_count == 0) {
        ret = sensors_event_loop_delete();
    }
#endif

    return ret;
}

esp_err_t sensors_event_post(esp_event_base_t event_base, int32_t event_id,
                             void *event_data, size_t event_data_size, TickType_t ticks_to_wait)
{
    if (h_sensors_loop == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_event_post_to(h_sensors_loop, event_base, event_id,
                             event_data, event_data_size, ticks_to_wait);
}


#if CONFIG_ESP_EVENT_POST_FROM_ISR
esp_err_t sensors_event_isr_post(esp_event_base_t event_base, int32_t event_id,
                                 void *event_data, size_t event_data_size, BaseType_t *task_unblocked)
{
    if (h_sensors_loop == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_event_isr_post_to(h_sensors_loop, event_base, event_id,
                                 event_data, event_data_size, task_unblocked);
}
#endif