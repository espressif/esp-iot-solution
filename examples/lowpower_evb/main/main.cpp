/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "lowpower_framework.h"
#include "lowpower_evb_callback.h"

#define LOWPOWER_EVB_TASK_STACK CONFIG_LOWPOWER_EVB_TASK_STACK

void lowpower_evb_register_cb(void) 
{
    lowpower_framework_register_callback(LPFCB_DEVICE_INIT, (void*)lowpower_evb_init_cb);
    lowpower_framework_register_callback(LPFCB_WIFI_CONNECT, (void*)wifi_connect_cb);

    #ifdef NOT_ULP_WAKE_UP
    lowpower_framework_register_callback(LPFCB_GET_DATA_BY_CPU, (void*)get_data_by_cpu_cb);
    #endif

    #ifdef ULP_WAKE_UP
    lowpower_framework_register_callback(LPFCB_ULP_PROGRAM_INIT, (void*)ulp_program_init_cb);
    lowpower_framework_register_callback(LPFCB_GET_DATA_FROM_ULP, (void*)get_data_from_ulp_cb);
    #endif
    
    lowpower_framework_register_callback(LPFCB_SEND_DATA_TO_SERVER, (void*)send_data_to_server_cb);
    lowpower_framework_register_callback(LPFCB_SEND_DATA_DONE, (void*)send_data_done_cb);
    lowpower_framework_register_callback(LPFCB_START_DEEP_SLEEP, (void*)start_deep_sleep_cb);
}

void lowpower_evb_task(void *arg)
{
    lowpower_framework_start();
}

extern "C" void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    lowpower_evb_register_cb();
    xTaskCreate(lowpower_evb_task, "lowpower_evb_task", LOWPOWER_EVB_TASK_STACK, NULL, 5, NULL);
}

