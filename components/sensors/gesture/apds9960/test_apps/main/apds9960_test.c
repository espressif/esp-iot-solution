/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "apds9960.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define APDS9960_VL_IO                       GPIO_NUM_3
#define APDS9960_I2C_MASTER_SCL_IO           GPIO_NUM_4             /*!< gpio number for I2C master clock */
#define APDS9960_I2C_MASTER_SDA_IO           GPIO_NUM_5             /*!< gpio number for I2C master data  */
#define APDS9960_I2C_MASTER_NUM              I2C_NUM_0              /*!< I2C port number for master dev */
#define APDS9960_I2C_MASTER_TX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_RX_BUF_DISABLE   0                      /*!< I2C master do not need buffer */
#define APDS9960_I2C_MASTER_FREQ_HZ          100000                 /*!< I2C master clock frequency */

i2c_bus_handle_t i2c_bus = NULL;
apds9960_handle_t apds9960 = NULL;

static void apds9960_gpio_vl_init(void)
{
    gpio_config_t cfg;
    cfg.pin_bit_mask = BIT(APDS9960_VL_IO);
    cfg.intr_type = 0;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_down_en = 0;
    cfg.pull_up_en = 0;
    gpio_config(&cfg);
    gpio_set_level(APDS9960_VL_IO, 0);
}

/**
 * @brief i2c master initialization
 */
static void apds9960_test_init()
{
    int i2c_master_port = APDS9960_I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = APDS9960_I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = APDS9960_I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = APDS9960_I2C_MASTER_FREQ_HZ,
    };
    i2c_bus = i2c_bus_create(i2c_master_port, &conf);
    apds9960 = apds9960_create(i2c_bus, APDS9960_I2C_ADDRESS);
}

static void apds9960_test_deinit()
{
    apds9960_delete(&apds9960);
    i2c_bus_delete(&i2c_bus);
}

static void apds9960_test_gesture()
{
    int cnt = 0;
    while (cnt < 10) {
        uint8_t gesture = apds9960_read_gesture(apds9960);
        if (gesture == APDS9960_DOWN) {
            printf("gesture APDS9960_DOWN*********************!\n");
        } else if (gesture == APDS9960_UP) {
            printf("gesture APDS9960_UP*********************!\n");
        } else if (gesture == APDS9960_LEFT) {
            printf("gesture APDS9960_LEFT*********************!\n");
        } else if (gesture == APDS9960_RIGHT) {
            printf("gesture APDS9960_RIGHT*********************!\n");
        }
        cnt++;
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor apds9960 init-deinit test", "[apds9960][iot][sensor]")
{
    apds9960_test_init();
    apds9960_test_deinit();
}

TEST_CASE("Sensor apds9960 test", "[apds9960][iot][sensor]")
{
    apds9960_gpio_vl_init();
    apds9960_test_init();
    apds9960_gesture_init(apds9960);
    vTaskDelay(1000 / portTICK_RATE_MS);
    apds9960_test_gesture();
    apds9960_test_deinit();
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
    printf("APDS9960 TEST \n");
    unity_run_menu();
}
