/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "esp_system.h"
#include "hdc2010.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define HDC2010_I2C_MASTER_SCL_IO           GPIO_NUM_2             /*!< gpio number for I2C master clock */
#define HDC2010_I2C_MASTER_SDA_IO           GPIO_NUM_1             /*!< gpio number for I2C master data  */
#define HDC2010_I2C_MASTER_NUM              I2C_NUM_0              /*!< I2C port number for master dev */
#define HDC2010_I2C_MASTER_TX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_RX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define HDC2010_I2C_MASTER_FREQ_HZ          100000                 /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static hdc2010_handle_t hdc2010 = NULL;
static const char *TAG = "hdc2010";

void hdc2010_init_test()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HDC2010_I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = HDC2010_I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = HDC2010_I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(HDC2010_I2C_MASTER_NUM, &conf);
    hdc2010 = hdc2010_create(i2c_bus, HDC2010_ADDR_PIN_SELECT_GND);
}

void hdc2010_deinit_test()
{
    hdc2010_delete(&hdc2010);
    i2c_bus_delete(&i2c_bus);
}

void hdc2010_get_data_test()
{
    int cnt = 10;
    while (cnt--) {
        ESP_LOGI(TAG, "temperature %f", hdc2010_get_temperature(hdc2010));
        ESP_LOGI(TAG, "humidity:%f", hdc2010_get_humidity(hdc2010));
        ESP_LOGI(TAG, "max temperature:%f", hdc2010_get_max_temperature(hdc2010));
        ESP_LOGI(TAG, "max humidity:%f \n", hdc2010_get_max_humidity(hdc2010));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor hdc2010 test", "[hdc2010][iot][sensor]")
{
    hdc2010_init_test();
    hdc2010_default_init(hdc2010);
    hdc2010_get_data_test();
    hdc2010_deinit_test();
}

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
    printf("HDC2010 TEST \n");
    unity_run_menu();
}
