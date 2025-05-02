/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*-
 * @File: aht20_test_app.c
 *
 * @brief: AHT20 driver unity test app
 *
 * @Date: May 2, 2025
 *
 * @Author: Rohan Jeet <jeetrohan92@gmail.com>
 *
 */

#include <stdio.h>
#include "unity.h"
#include "esp_system.h"
#include "aht20.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

// I2C config
#define I2C_MASTER_SCL_IO       CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO       CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000

// Global handles
i2c_master_bus_handle_t my_i2c_bus_handle = NULL;
aht20_handle_t aht20_handle = NULL;

/*******************************Memory Leak Checks****************************/

static size_t before_free_8bit;
static size_t before_free_32bit;
static size_t after_free_8bit;
static size_t after_free_32bit;

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
    after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

/************************** Memory Leak Checks Completed********************/

/*******************************I2C Master Bus Initialization****************************/
void i2c_master_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    printf("Requesting I2C bus handle\n");
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &my_i2c_bus_handle));
    printf("I2C bus handle acquired\n");
}
/*******************************I2C Master Bus Initialization Over****************************/

/*******************************AHT20 Device Initialization****************************/
esp_err_t aht20_init_test()
{
    i2c_master_init();
    aht20_handle = aht20_create(my_i2c_bus_handle, AHT20_ADDRESS_LOW);

    printf("Initializing AHT20 sensor\n");
    while (aht20_init(aht20_handle) != ESP_OK) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("AHT20 initialized\n");

    return ESP_OK;
}
/*******************************AHT20 Device Initialization Over****************************/

/*******************************AHT20 Device Deinitializtion****************************/
void aht20_deinit_test(void)
{
    aht20_remove(&aht20_handle);
}
/*******************************AHT20 Device Deinitializtion Over****************************/

/*******************************AHT20 Read sensor****************************/
void aht20_read_test(void)
{
    vTaskDelay(400 / portTICK_PERIOD_MS);

    TEST_ASSERT(ESP_OK == aht20_read_humiture(aht20_handle));

    printf("Temperature = %.2f°C, Humidity = %.3f%%\n",
           aht20_handle->humiture.temperature,
           aht20_handle->humiture.humidity);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
/*******************************AHT20 AHT20 Read sensor Over*****************************/

/*************************************Test Case**************************/

TEST_CASE("AHT20 Sensor", "[aht20][sensor]")
{
    aht20_init_test();
    aht20_read_test();
    aht20_deinit_test();
}
/*************************************Test Case Over**************************/

void app_main(void)
{
    printf("\n=== AHT20 Sensor Test Menu ===\n");
    unity_run_menu();  // Run test selection menu in flash monitor
}
