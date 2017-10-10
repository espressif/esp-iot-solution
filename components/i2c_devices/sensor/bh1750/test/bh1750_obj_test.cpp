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
#include "esp_system.h"
#include "unity.h"
#include "bh1750.h"
#include "i2c_bus.h"
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
