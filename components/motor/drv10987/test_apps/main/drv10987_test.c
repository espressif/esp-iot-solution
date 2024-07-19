/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "drv10987.h"
#include "esp_check.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

#define DIRECTION_IO        21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_SDA_IO   23
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

static size_t before_free_8bit;
static size_t before_free_32bit;

static i2c_bus_handle_t i2c_bus = NULL;
static drv10987_handle_t drv10987 = NULL;

static void drv10987_test_init(void)
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
    TEST_ASSERT_NOT_NULL(i2c_bus);

    drv10987_config_t drv10987_cfg = {
        .bus = i2c_bus,
        .dev_addr = DRV10987_I2C_ADDRESS_DEFAULT,
        .dir_pin = DIRECTION_IO,
    };

    esp_err_t err = drv10987_create(&drv10987, &drv10987_cfg);
    TEST_ASSERT(err == ESP_OK);
}

static void drv10987_test_deinit(void)
{
    drv10987_delete(drv10987);
    drv10987 = NULL;
    i2c_bus_delete(&i2c_bus);
}

TEST_CASE("drv10987 eeprom test", "[eeprom]")
{
    drv10987_test_init();
    drv10987_config1_t config1 = DEFAULT_DRV10987_CONFIG1;
    drv10987_config2_t config2 = DEFAULT_DRV10987_CONFIG2;
    TEST_ASSERT_EQUAL(ESP_OK, drv10987_eeprom_test(drv10987, config1, config2));
    drv10987_test_deinit();
}

TEST_CASE("drv10987 read motor phase resistance and kt", "[motor info][eeprom]")
{
    drv10987_test_init();
    float phase_resistance = 0.0f, kt = 0.0f;
    TEST_ASSERT_EQUAL(ESP_OK, drv10987_read_phase_resistance_and_kt(drv10987, &phase_resistance, &kt));
    printf("Phase Resistance: %.2f Ohm, kt: %.2f mV/Hz \n", phase_resistance, kt);
    drv10987_test_deinit();
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
    printf("DRV10987 TEST\n");
    unity_run_menu();
}
