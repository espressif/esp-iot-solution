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
#include "led_12_8_image.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */

CI2CBus *p_i2c_bus = NULL;
CIs31fl3736 *p_is31fl3736 = NULL;
extern "C" void initGame(void*);

esp_err_t example_build_buf(i2c_port_t i2c_port, uint8_t x, uint8_t y, char *c, uint8_t duty, uint8_t* buf)
{
    uint8_t i;
    if (x > 8 || y > 12) {
        return ESP_FAIL;
    }
    switch (*c) {
        case 'e':
            for (i = 0; i < 12; i++) {
                if (image_e[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_e[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        case 's':
            for (i = 0; i < 12; i++) {
                if (image_s[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_s[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        case 'p':
            for (i = 0; i < 12; i++) {
                if (image_p[i]) {
                    if (i + y < 12) {
                        buf[i + y] = image_p[i];
                        buf[i + y] = (buf[i + y] << x) & 0xff;
                    }
                }
            }
            break;
        default:
            return ESP_FAIL;
            break;
    }
    return ESP_OK;
}

void fill_pixel(int x, int y, uint8_t duty)
{
    if (x == -1 || y == -1) {
        return;
    }
    uint8_t reg = x * 2 + y * 0x10;
    p_is31fl3736->write_reg(reg, &duty, 1);
}

void is31f13736_obj_test_func()
{
    int i = 11;
    char c = 'e';
    static uint8_t dir = 0;
    p_is31fl3736->set_page(IS31FL3736_PAGE(1));
    initGame((void*) &fill_pixel);
    int cnt = 1;
    while (cnt) {
        if (dir == 0) {
            vTaskDelay(150 / portTICK_RATE_MS);
            uint8_t buf[12] = { 0 };
            c = 'e';
            example_build_buf(I2C_MASTER_NUM, 0, i, &c, 50, buf);
            c = 's';
            example_build_buf(I2C_MASTER_NUM, 0, i + 4, &c, 50, buf);
            c = 'p';
            example_build_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            p_is31fl3736->fill_matrix(50, buf);
            if (--i < 0 - 4) {
                dir = 1;
            }
        } else {
            vTaskDelay(50 / portTICK_RATE_MS);
            uint8_t buf[12] = { 0 };
            c = 'e';
            example_build_buf(I2C_MASTER_NUM, 0, i, &c, 50, buf);
            c = 's';
            example_build_buf(I2C_MASTER_NUM, 0, i + 4, &c, 50, buf);
            c = 'p';
            example_build_buf(I2C_MASTER_NUM, 0, i + 8, &c, 50, buf);
            p_is31fl3736->fill_matrix(50, buf);
            if (++i > 12) {
                dir = 0;
                cnt--;
            }
        }
    }
}

extern "C" void is31f13736_obj_test()
{
    p_i2c_bus = new CI2CBus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    p_is31fl3736 = new CIs31fl3736(p_i2c_bus, (gpio_num_t) GPIO_NUM_32, (is31fl3736_addr_pin_t) 0,
            (is31fl3736_addr_pin_t) 0, 0XCF);
    is31f13736_obj_test_func();
    delete (p_is31fl3736);
    delete (p_i2c_bus);
    p_is31fl3736 = NULL;
    p_i2c_bus = NULL;
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("I2C led is31f13736 cpp test", "[is31f13736_cpp][iot][fxled]")
{
    is31f13736_obj_test();
}
