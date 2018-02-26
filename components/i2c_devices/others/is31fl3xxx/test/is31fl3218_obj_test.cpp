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
#include <stdio.h>
#include "driver/i2c.h"
#include "iot_is31fl3218.h"
#include "iot_is31fl3736.h"
#include "iot_i2c_bus.h"
#include "unity.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */

extern "C" void is31f13218_obj_test()
{
    CI2CBus i2c_bus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    CIs31fl3218 is31fl3218(&i2c_bus);
    uint8_t i = 0, j = 255 / 10;
    is31fl3218.set_duty(IS31FL3218_CH_NUM_MAX_MASK, 0);
    int cnt = 0;
    uint8_t duty_data[18] = { 0 };
    while (1) {
        is31fl3218.write_duty_regs(duty_data, sizeof(duty_data));
        vTaskDelay(20 / portTICK_RATE_MS);
        if (++i > IS31FL3218_CH_NUM_MAX - 1) {
            i = 0;
            if (j == 0) {
                j = 255 / 10;
                if (cnt++ > 1) {
                    break;
                }
            } else {
                j = 0;
            }
        }
        duty_data[i] = j;
    }
    cnt = 0;
    uint32_t ch_mask = 0x1f;
    int dcnt = 4;
    while (1) {
        vTaskDelay(10 / portTICK_RATE_MS);
        for (int i = 0; i < 18; i++) {
            if ((ch_mask >> i) & 0x1) {
                if (dcnt >= 4) {
                    int duty = 5 - (dcnt - i);
                    is31fl3218.set_duty(IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                } else {
                    int duty = 5 - (dcnt - i);
                    if (duty < 0) {
                        duty += 16;
                    }
                    is31fl3218.set_duty(IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                }
            } else {
                is31fl3218.set_duty(IS31FL3218_CH_BIT(i), 0);
            }
        }

        ch_mask = ch_mask << 1;
        ch_mask |= (ch_mask >> 18);
        dcnt++;
        if (dcnt > 17) {
            dcnt = 0;
            if (cnt++ > 4) {
                is31fl3218.set_duty(0xfffff, 0xff * 0 / 5);
                break;
            }
        }
    }
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("I2C led is31f13218 cpp test", "[is31f13218_cpp][iot][fxled]")
{
    is31f13218_obj_test();
}


