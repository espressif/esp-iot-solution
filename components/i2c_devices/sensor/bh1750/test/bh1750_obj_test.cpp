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
#include "iot_bh1750.h"
#include "iot_i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */

extern "C" void bh1750_obj_test()
{
    CI2CBus i2c_bus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    CBh1750 bh1750(&i2c_bus);

    bh1750.on();
    int cnt = 5;
    while(cnt--){
        bh1750.set_mode(BH1750_ONETIME_4LX_RES);
        vTaskDelay(30 / portTICK_RATE_MS);
        printf("bh1750 val(one time mode): %f\n", bh1750.read());
        bh1750.set_mode(BH1750_CONTINUE_4LX_RES);
        vTaskDelay(30 / portTICK_RATE_MS);
        printf("bh1750 val(continuously mode): %f\n", bh1750.read());
        vTaskDelay(200 / portTICK_RATE_MS);
    }
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("Sensor BH1750 obj test", "[bh1750_cpp][iot][sensor]")
{
    bh1750_obj_test();
}
