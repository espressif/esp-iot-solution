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

#define MCP23017_TEST_CODE 1
#if MCP23017_TEST_CODE

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

#endif
