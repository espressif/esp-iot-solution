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
#include "unity.h"
#include "driver/i2c.h"
#include "iot_mcp23017.h"
#include "iot_i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           21          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           15          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mcp23017_handle_t device = NULL;

/**
 * @brief i2c master initialization
 */
static void i2c_bus_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(I2C_MASTER_NUM, &conf);
}

void mcp23017_init()
{
    i2c_bus_init();
    device = iot_mcp23017_create(i2c_bus, 0x20);
}

void mcp23017_test_task(void* pvParameters)
{
    int cnt = 5;
    iot_mcp23017_set_io_dir(device, 0x11, MCP23017_GPIOA); //config input:1; 0:output
    iot_mcp23017_set_io_dir(device, 0x11, MCP23017_GPIOB);
    while (cnt--) {

        /*****Interrupt Test******/
        iot_mcp23017_interrupt_en(device, MCP23017_PIN1, true, 0x00);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("Intf:%d\n", iot_mcp23017_get_int_flag(device));

        /*****Normal Test*****
         ret = iot_mcp23017_read_io(device, MCP23017_GPIOA);
         printf("GPIOA:%x\n", ret);
         ret = iot_mcp23017_read_io(device, MCP23017_GPIOB);
         printf("GPIOB:%x\n", ret);

//    	iot_mcp23017_write_io(device,0x11, MCP23017_GPIOA);
//    	iot_mcp23017_write_io(device,0x22, MCP23017_GPIOB);
//		vTaskDelay(1000 / portTICK_RATE_MS);

         ret = iot_mcp23017_read_io(device, MCP23017_GPIOA);
         printf("Become GPIOA:%x\n", ret);
         ret = iot_mcp23017_read_io(device, MCP23017_GPIOB);
         printf("Become GPIOB:%x\n", ret);
         vTaskDelay(1000 / portTICK_RATE_MS);*/
    }
    iot_mcp23017_delete(device, true);
    vTaskDelete(NULL);
}

void mcp23017_test()
{
    mcp23017_init();
    xTaskCreate(mcp23017_test_task, "mcp23017_test_task", 1024 * 2, NULL, 10,
            NULL);
}

TEST_CASE("Device mcp23017 test", "[mcp23017][iot][device]")
{
    mcp23017_test();
}
