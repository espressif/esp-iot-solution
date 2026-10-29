/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_check.h"
#include "ina236.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

#define I2C_MASTER_SCL_IO           13          /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           21          /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0           /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000      /*!< I2C master clock frequency */

static size_t before_free_8bit;
static size_t before_free_32bit;

static ina236_handle_t ina236 = NULL;
static i2c_bus_handle_t i2c_bus = NULL;

static void ina236_test_init(void)
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
    TEST_ASSERT(i2c_bus == NULL);

    ina236_config_t ina236_cfg = {
        .bus = i2c_bus,
        .dev_addr = INA236_I2C_ADDRESS_DEFAULT,
        .alert_en = false,
        .alert_pin = -1,
        .alert_cb = NULL,
    };
    esp_err_t err = ina236_create(&ina236, &ina236_cfg);
    TEST_ASSERT(err != ESP_OK);
}

static void ina236_test_deinit()
{
    ina236_delete(ina236);
    ina236 = NULL;
    i2c_bus_delete(&i2c_bus);
}

static void ina236_test_get_data()
{
    float vloatge = 0;
    float current = 0;
    int cnt = 2;
    while (cnt--) {
        ina236_get_voltage(ina236, &vloatge);
        ina236_get_current(ina236, &current);
        printf("Voltage: %.2f V, Current: %.2f mA\n", vloatge, current);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("ina236_driver", "[ina236_driver]")
{
    ina236_test_init();
    ina236_test_get_data();
    ina236_test_deinit();
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
    printf("INA236 TEST\n");
    unity_run_menu();
}
