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
#include "is31fl3218.h"
#include "is31fl3736.h"
#include "i2c_bus.h"
#include "unity.h"

#define I2C_MASTER_SCL_IO    13        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    14        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM     I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000   /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static is31fl3218_handle_t fxled = NULL;


/**
 * @brief i2c master initialization
 */
void fxled_is31fl3218_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if(fxled ==NULL) {
        fxled = is31fl3218_create(i2c_bus);
    }
    TEST_ASSERT_NOT_NULL(fxled);
}

void is31f13218_test()
{
    fxled_is31fl3218_init();

    uint8_t i=0,j=255/10;
    ESP_ERROR_CHECK( is31fl3218_channel_set(fxled, IS31FL3218_CH_NUM_MAX_MASK, 0) );
    int cnt = 0;
    printf("enter loop\n");
    uint8_t duty_data[18] = {0};
    while(1){
        is31fl3218_write_pwm_regs(fxled, duty_data, sizeof(duty_data));
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
                    is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                } else {
                    int duty = 5 - (dcnt - i);
                    if (duty < 0) {
                        duty += 16;
                    }
                    is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                }
            } else {
                is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0);
            }
        }
        ch_mask = ch_mask << 1;
        ch_mask |= (ch_mask >> 18);
        dcnt++;
        if (dcnt > 17) {
            dcnt = 0;
            if (cnt++ > 4) {
                is31fl3218_channel_set(fxled, 0xfffff, 0xff * 0 / 5);
                break;
            }
        }
    }

    is31fl3218_delete(fxled);
    i2c_bus_delete(&i2c_bus);
    fxled = NULL;
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("I2C led is31f13218 test", "[is31f13218][iot][fxled]")
{
    is31f13218_test();
}

