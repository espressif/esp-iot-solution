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

#define PM_CF_IO_NUM    GPIO_NUM_25
#define PM_CFI_IO_NUM   GPIO_NUM_26

#define PM_CF_PCNT_UNIT     PCNT_UNIT_0
#define PM_CFI_PCNT_UNIT    PCNT_UNIT_1

#define PM_POWER_PARAM  1293699
#define PM_VOLTAGE_PARAM    102961
#define PM_CURRENT_PARAM    13670

static const char* TAG = "powermeter_test";

extern "C" void power_meter_obj_test()
{
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
    CPowerMeter* my_pm = new CPowerMeter(pm_conf);
    while (1) {
        my_pm->change_mode(PM_SINGLE_VOLTAGE);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", my_pm->read(PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", my_pm->read(PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", my_pm->read(PM_CURRENT));

        my_pm->change_mode(PM_SINGLE_CURRENT);
        vTaskDelay(5000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "value of power:%d", my_pm->read(PM_POWER));
        ESP_LOGI(TAG, "value of voltage:%d", my_pm->read(PM_VOLTAGE));
        ESP_LOGI(TAG, "value of current:%d", my_pm->read(PM_CURRENT));
    }
    delete my_pm;
}

TEST_CASE("Power meter obj test", "[power_meter][iot]")
{
    power_meter_obj_test();
}
