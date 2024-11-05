/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "veml6040.h"
#include "unity.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define VEML6040_I2C_MASTER_SCL_IO               GPIO_NUM_2     /*!< gpio number for I2C master clock */
#define VEML6040_I2C_MASTER_SDA_IO               GPIO_NUM_1     /*!< gpio number for I2C master data  */

#define VEML6040_I2C_MASTER_NUM                  I2C_NUM_0      /*!< I2C port number for master dev */
#define VEML6040_I2C_MASTER_TX_BUF_DISABLE       0              /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_RX_BUF_DISABLE       0              /*!< I2C master do not need buffer */
#define VEML6040_I2C_MASTER_FREQ_HZ              100000         /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static veml6040_handle_t veml6040 = NULL;

static void veml6040_test_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = VEML6040_I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = VEML6040_I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = VEML6040_I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(VEML6040_I2C_MASTER_NUM, &conf);
    veml6040 = veml6040_create(i2c_bus, VEML6040_I2C_ADDRESS);
}

static void veml6040_test_deinit()
{
    veml6040_delete(&veml6040);
    i2c_bus_delete(&i2c_bus);
}

static void veml6040_test_set_mode()
{
    veml6040_config_t veml6040_info;
    memset(&veml6040_info, 0, sizeof(veml6040_info));
    veml6040_get_mode(veml6040, &veml6040_info);
    printf("integration_time:%d \n", veml6040_info.integration_time);
    printf("mode:%d \n", veml6040_info.mode);
    printf("trigger:%d \n", veml6040_info.trigger);
    printf("switch_en:%d \n", veml6040_info.switch_en);
    veml6040_info.integration_time = VEML6040_INTEGRATION_TIME_160MS;
    veml6040_info.mode = VEML6040_MODE_AUTO;
    veml6040_info.trigger = VEML6040_TRIGGER_DIS;
    veml6040_info.switch_en = VEML6040_SWITCH_EN;
    veml6040_set_mode(veml6040, &veml6040_info);
    vTaskDelay(500 / portTICK_RATE_MS);
    veml6040_get_mode(veml6040, &veml6040_info);
    printf("integration_time:%d \n", veml6040_info.integration_time);
    printf("mode:%d \n", veml6040_info.mode);
    printf("trigger:%d \n", veml6040_info.trigger);
    printf("switch_en:%d \n", veml6040_info.switch_en);

}

static void veml6040_test_get_date()
{
    int cnt = 10;
    while (cnt--) {
        printf("red:%d ", veml6040_get_red(veml6040));
        printf("green:%d ", veml6040_get_green(veml6040));
        printf("blue:%d ", veml6040_get_blue(veml6040));
        printf("lux:%f\n", veml6040_get_lux(veml6040));
        vTaskDelay(1000  / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor veml6040 test", "[veml6040][iot][sensor]")
{
    veml6040_test_init();
    veml6040_test_set_mode();
    veml6040_test_get_date();
    veml6040_test_deinit();
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
    printf("VEML6040 TEST \n");
    unity_run_menu();
}
