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
#include "esp_system.h"
#include "unity.h"
#include "iot_adc.h"
#include "esp_log.h"

static const char* TAG = "ADC_OBJ_TEST";

extern "C" void adc_obj_test()
{
    ADConverter adc(GPIO_NUM_36);
    ESP_LOGI(TAG, "read vol: %d mv", adc.read());
}

TEST_CASE("ADC obj cpp test", "[adc_obj_cpp][iot]")
{
    adc_obj_test();
}
