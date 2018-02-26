// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "iot_power_meter.h"
#include "esp_log.h"
#include "unity.h"

#define PM_CF_IO_NUM    25
#define PM_CFI_IO_NUM   26

#define PM_CF_PCNT_UNIT     PCNT_UNIT_0
#define PM_CFI_PCNT_UNIT    PCNT_UNIT_1

#define PM_POWER_PARAM  1293699
#define PM_VOLTAGE_PARAM    102961
#define PM_CURRENT_PARAM    13670

static const char* TAG = "powermeter_test";
pm_handle_t pm_handle;

static void read_value(void* arg)
{
    pm_handle_t pm_handle = arg;
    for (int i = 0; i < 2; i++) {
        iot_powermeter_change_mode(pm_handle, PM_SINGLE_VOLTAGE);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", iot_powermeter_read(pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", iot_powermeter_read(pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", iot_powermeter_read(pm_handle, PM_CURRENT));
        iot_powermeter_change_mode(pm_handle, PM_SINGLE_CURRENT);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", iot_powermeter_read(pm_handle, PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", iot_powermeter_read(pm_handle, PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", iot_powermeter_read(pm_handle, PM_CURRENT));
    }
    vTaskDelete(NULL);
}

void power_meter_test()
{
    printf("before power meter create, heap: %d\n", esp_get_free_heap_size());
    pm_config_t pm_conf = {
        .power_io_num = PM_CF_IO_NUM,
        .power_pcnt_unit = PCNT_UNIT_0,
        .power_ref_param = PM_POWER_PARAM,
        .voltage_io_num = PM_CFI_IO_NUM,
        .voltage_pcnt_unit = PCNT_UNIT_1,
        .voltage_ref_param = PM_VOLTAGE_PARAM,
        .current_io_num = PM_CFI_IO_NUM,
        .current_pcnt_unit = PCNT_UNIT_1,
        .current_ref_param = PM_CURRENT_PARAM,
        .sel_io_num = 17,
        .sel_level = 0,
        .pm_mode = PM_SINGLE_VOLTAGE
    };
    pm_handle = iot_powermeter_create(pm_conf);
    xTaskCreate(read_value, "read_value", 2048, pm_handle, 5, NULL);
    vTaskDelay(60000 / portTICK_RATE_MS);
    iot_powermeter_delete(pm_handle);
    printf("after power meter delete, heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("Power meter test", "[power_meter][iot]")
{
    power_meter_test();
}
