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
#include "iot_lis2dh12.h"
#include "iot_i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */

extern "C" void lis2dh12_obj_test()
{
    CI2CBus i2c_bus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    CLis2dh12 lis2dh12(&i2c_bus);

    int cnt = 5;
    while(cnt--){
        printf("\n******************************************\n");
        printf("X-axis acceleration is: %08x\n", lis2dh12.ax());
        printf("Y-axis acceleration is: %08x\n", lis2dh12.ay());
        printf("Z-axis acceleration is: %08x\n", lis2dh12.az());
        printf("******************************************\n");
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("Sensor LIS2DH12 obj test", "[lis2dh12_cpp][iot][sensor]")
{
    lis2dh12_obj_test();
}
