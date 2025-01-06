/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "mcp23017.h"
#include "i2c_bus.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           1           /*!< gpio number for I2C master clock IO1*/
#define I2C_MASTER_SDA_IO           2           /*!< gpio number for I2C master data  IO2*/
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static i2c_bus_handle_t i2c_bus = NULL;
static mcp23017_handle_t device = NULL;

static void mcp23017_test_init()
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
    device = mcp23017_create(i2c_bus, 0x20);
    TEST_ASSERT(i2c_bus != NULL);
    TEST_ASSERT(device != NULL);
    TEST_ASSERT(ESP_OK == mcp23017_check_present(device));
}

static void mcp23017_test_deinit()
{
    mcp23017_delete(&device);
    TEST_ASSERT(device == NULL);
    i2c_bus_delete(&i2c_bus);
    TEST_ASSERT(i2c_bus == NULL);
}

static void mcp23017_test_read_write()
{
    uint8_t cnt = 5;
    TEST_ASSERT(ESP_OK == mcp23017_set_io_dir(device, 0x00, MCP23017_GPIOA)); //A:OUTPUT, B:INPUT
    TEST_ASSERT(ESP_OK == mcp23017_set_io_dir(device, 0xff, MCP23017_GPIOB));
    while (cnt--) {

        /*****Interrupt Test******/
        // mcp23017_interrupt_en(device, MCP23017_PIN15, 1, 0x0000); //1: compared against DEFVAL register.
        // vTaskDelay(1000 / portTICK_RATE_MS);
        // printf("Intf:%d\n",  mcp23017_get_int_flag(device));

        /*****Normal Test*****/
        TEST_ASSERT(ESP_OK == mcp23017_write_io(device, cnt, MCP23017_GPIOA));
        printf("GPIOA write = :%x\n", cnt);
        uint8_t read = mcp23017_read_io(device, MCP23017_GPIOB);
        printf("GPIOB read = :%x\n", read);
        TEST_ASSERT_EQUAL_UINT8(cnt, read);
        vTaskDelay(300 / portTICK_RATE_MS);
    }
}

TEST_CASE("Device mcp23017 test, connect A-B port together", "[mcp23017][iot][device]")
{
    mcp23017_test_init();
    mcp23017_test_read_write();
    mcp23017_test_deinit();
}

static size_t before_free_8bit;
static size_t before_free_32bit;

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("MCP23017 TEST \n");
    unity_run_menu();
}
