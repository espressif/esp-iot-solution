/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
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
i2c_bus_handle_t my_i2c_bus_handle = NULL;
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
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

/************************** Memory Leak Checks Completed********************/

/*******************************I2C Master Bus Initialization****************************/
void i2c_master_init(void)
{
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    printf("Requesting I2C bus handle\n");
    my_i2c_bus_handle = i2c_bus_create(I2C_MASTER_NUM, &conf);
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
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    printf("AHT20 initialized\n");

    return ESP_OK;
}
/*******************************AHT20 Device Initialization Over****************************/

/*******************************AHT20 Device Deinitializtion****************************/
void aht20_deinit_test(void)
{
    aht20_remove(&aht20_handle);
    i2c_bus_delete(&my_i2c_bus_handle);
}
/*******************************AHT20 Device Deinitializtion Over****************************/

/*******************************AHT20 Read sensor****************************/
void aht20_check_humidity(void)
{
    float_t humidity;

    TEST_ASSERT(ESP_OK == aht20_read_humidity(aht20_handle, &humidity));

    printf("Humidity = %.2f%%\n", humidity);

}

void aht20_check_temprature(void)
{
    float_t temperature;

    TEST_ASSERT(ESP_OK == aht20_read_temperature(aht20_handle, &temperature));

    printf("Temperature = %.2f°C\n", temperature);

}
/*******************************AHT20 AHT20 Read sensor Over*****************************/

/*************************************Test Case**************************/

TEST_CASE("AHT20 Sensor", "[aht20][sensor]")
{
    aht20_init_test();
    aht20_check_humidity();
    vTaskDelay(pdMS_TO_TICKS(2000));
    aht20_check_temprature();
    aht20_deinit_test();
}
/*************************************Test Case Over**************************/

void app_main(void)
{
    printf("\n=== AHT20 Sensor Test Menu ===\n");
    unity_run_menu();  // Run test selection menu in flash monitor
}
