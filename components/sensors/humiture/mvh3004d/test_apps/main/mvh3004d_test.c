/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "mvh3004d.h"
#include "i2c_bus.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define I2C_MASTER_SCL_IO           GPIO_NUM_2           /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_1           /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0            /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000               /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static mvh3004d_handle_t mvh3004d = NULL;

void mvh3004d_init_test()
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
    mvh3004d = mvh3004d_create(i2c_bus, MVH3004D_SLAVE_ADDR);
}

void mvh3004d_deinit_test()
{
    i2c_bus_delete(&i2c_bus);
    mvh3004d_delete(&mvh3004d);
}

void mvh3004d_get_data_test()
{
    int cnt = 10;
    float tp = 0.0, rh = 0.0;
    while (cnt--) {
        mvh3004d_get_data(mvh3004d, &tp, &rh);
        printf("mvh3004d[%d]: tp: %.02f; rh: %.02f %%\n", cnt, tp, rh);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

TEST_CASE("Sensor MVH3004D test", "[mvh3004d][iot][sensor]")
{
    mvh3004d_init_test();
    mvh3004d_get_data_test();
    mvh3004d_deinit_test();
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
    printf("MVH3004D TEST \n");
    unity_run_menu();
}
