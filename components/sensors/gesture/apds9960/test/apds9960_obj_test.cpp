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
#include "esp_log.h"
#include "unity.h"
#include "iot_i2c_bus.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "iot_apds9960.h"

#define APDS9960_I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock */
#define APDS9960_I2C_MASTER_SDA_IO           (gpio_num_t)22          /*!< gpio number for I2C master data  */
#define APDS9960_I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define APDS9960_I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

void periph_gpio_init()
{
    gpio_config_t cfg;
    cfg.pin_bit_mask = BIT19;
    cfg.intr_type = (gpio_int_type_t) 0;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_down_en = (gpio_pulldown_t) 0;
    cfg.pull_up_en = (gpio_pullup_t) 0;

    gpio_config(&cfg);
    gpio_set_level((gpio_num_t) 19, (uint8_t) 0);

    cfg.pin_bit_mask = BIT26;
    cfg.intr_type = (gpio_int_type_t) 0;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_down_en = (gpio_pulldown_t) 0;
    cfg.pull_up_en = (gpio_pullup_t) 0;
    gpio_config(&cfg);
}


extern "C" void apds9960_obj_test()
{
    CI2CBus i2c_bus(APDS9960_I2C_MASTER_NUM, APDS9960_I2C_MASTER_SCL_IO, APDS9960_I2C_MASTER_SDA_IO);
    CApds9960 apds(&i2c_bus);
    // Initialize interrupt service routine
    periph_gpio_init();
    // Initialize APDS-9960 (configure I2C and initial values)
    if (apds.gesture_init()) {
        printf("Device initialized!\n");
    } else {
        printf("Device false!\n");
    }

    int cnt = 0;
    while (cnt < 5) {
        uint8_t gesture = apds.read_gesture();
        if (gesture == APDS9960_DOWN) {
            printf("APDS9960_DOWN*******************************\n");
        }
        if (gesture == APDS9960_UP) {
            printf("APDS9960_UP********************************\n");
        }
        if (gesture == APDS9960_LEFT) {
            printf("APDS9960_LEFT**********************\n");
            cnt++;
        }
        if (gesture == APDS9960_RIGHT) {
            printf("APDS9960_RIGHT***************************\n");
            cnt++;
        }
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor apds9960 obj test", "[apds9960_cpp][iot][Sensor]")
{
    apds9960_obj_test();
}

