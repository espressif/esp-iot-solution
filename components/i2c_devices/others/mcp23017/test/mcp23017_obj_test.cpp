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
#include "mcp23017.h"
#include "i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           (gpio_num_t)21          /*!< gpio number for I2C master clock IO21*/
#define I2C_MASTER_SDA_IO           (gpio_num_t)15          /*!< gpio number for I2C master data  IO15*/
#define I2C_MASTER_NUM              I2C_NUM_1               /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           			/*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           			/*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      			/*!< I2C master clock frequency */

extern "C" void mcp23017_obj_test()
{
    int cnt = 2;
    CI2CBus i2c_bus(I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);
    CMCP23017 mcp23017(&i2c_bus);
    mcp23017.set_iodirection(0x00, MCP23017_GPIOA);
    mcp23017.set_iodirection(0x00, MCP23017_GPIOB);

    while (cnt--) {
        printf("GPIOA:%x\n", mcp23017.read_ioport(MCP23017_GPIOA));
        printf("GPIOB:%x\n", mcp23017.read_ioport(MCP23017_GPIOB));
//		mcp23017.write_ioport(0x11, MCP23017_GPIOA);
//		mcp23017.write_ioport(0x22, MCP23017_GPIOB);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("Become GPIOA:%x\n", mcp23017.read_ioport(MCP23017_GPIOA));
        printf("Become GPIOB:%x\n", mcp23017.read_ioport(MCP23017_GPIOB));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("Device MCP23017 obj test", "[MCP23017_cpp][iot][device]")
{
    mcp23017_obj_test();
}
