/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_config.h"
#include "i2c_bus.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define I2C_MASTER_SCL_IO          (gpio_num_t)22         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          (gpio_num_t)21         /*!< gpio number for I2C master data  */

#define DATA_LENGTH          512  /*!<Data buffer length for test buffer*/
#define RW_TEST_LENGTH       129  /*!<Data length for r/w test, any value from 0-DATA_LENGTH*/
#define DELAY_TIME_BETWEEN_ITEMS_MS   1234 /*!< delay time between different test items */

#define I2C_SLAVE_SCL_IO     17    /*!<gpio number for i2c slave clock  */
#define I2C_SLAVE_SDA_IO     16    /*!<gpio number for i2c slave data */
#define I2C_MASTER_NUM I2C_NUM_0    /*!<I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */
#define I2C_SLAVE_NUM I2C_NUM_0    /*!<I2C port number for slave dev */
#define I2C_SLAVE_TX_BUF_LEN  (2*DATA_LENGTH) /*!<I2C slave tx buffer size */
#define I2C_SLAVE_RX_BUF_LEN  (2*DATA_LENGTH) /*!<I2C slave rx buffer size */

#define ESP_SLAVE_ADDR 0x28         /*!< ESP32 slave address, you can set any 7bit value */

void i2c_bus_init_deinit_test()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus_handle_t i2c0_bus_1 = i2c_bus_create(I2C_NUM_0, &conf);
    TEST_ASSERT(i2c0_bus_1 != NULL);
    /** configs not change**/
    i2c0_bus_1 = i2c_bus_create(I2C_NUM_0, &conf);
    TEST_ASSERT(i2c0_bus_1 != NULL);
    /** configs not change**/
    conf.master.clk_speed *= 2;
    i2c0_bus_1 = i2c_bus_create(I2C_NUM_0, &conf);
    TEST_ASSERT(i2c0_bus_1 != NULL);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c0_bus_1));
    TEST_ASSERT(i2c0_bus_1 == NULL);
}

void i2c_bus_device_add_test()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus_handle_t i2c0_bus_1 = i2c_bus_create(I2C_NUM_0, &conf);
    TEST_ASSERT(i2c0_bus_1 != NULL);
    i2c_bus_device_handle_t i2c_device1 = i2c_bus_device_create(i2c0_bus_1, 0x01, 400000);
    TEST_ASSERT(i2c_device1 != NULL);
    i2c_bus_device_handle_t i2c_device2 = i2c_bus_device_create(i2c0_bus_1, 0x01, 100000);
    TEST_ASSERT(i2c_device2 != NULL);
    i2c_bus_device_delete(&i2c_device1);
    TEST_ASSERT(i2c_device1 == NULL);
    i2c_bus_device_delete(&i2c_device2);
    TEST_ASSERT(i2c_device2 == NULL);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c0_bus_1));
    TEST_ASSERT(i2c0_bus_1 == NULL);
}

// print the reading buffer
static void disp_buf(uint8_t *buf, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        printf("%02x ", buf[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

static void i2c_master_write_test(void)
{
    uint8_t *data_wr = (uint8_t *) malloc(DATA_LENGTH);
    int i;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus_handle_t i2c0_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    TEST_ASSERT(i2c0_bus != NULL);
    i2c_bus_device_handle_t i2c_device1 = i2c_bus_device_create(i2c0_bus, ESP_SLAVE_ADDR, 0);
    TEST_ASSERT(i2c_device1 != NULL);

    for (i = 0; i < DATA_LENGTH / 2; i++) {
        data_wr[i] = i;
    }

    i2c_bus_write_bytes(i2c_device1, NULL_I2C_MEM_ADDR, DATA_LENGTH / 2, data_wr);
    disp_buf(data_wr, i + 1);
    free(data_wr);
    i2c_bus_device_delete(&i2c_device1);
    TEST_ASSERT(i2c_device1 == NULL);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c0_bus));
    TEST_ASSERT(i2c0_bus == NULL);
}

static void i2c_slave_read_test(void)
{
    uint8_t *data_rd = (uint8_t *) malloc(DATA_LENGTH);
    int size_rd = 0;
    int len = 0;

    i2c_config_t conf_slave = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = ESP_SLAVE_ADDR,
    };

    TEST_ESP_OK(i2c_param_config(I2C_SLAVE_NUM, &conf_slave));
    TEST_ESP_OK(i2c_driver_install(I2C_SLAVE_NUM, I2C_MODE_SLAVE,
                                   I2C_SLAVE_RX_BUF_LEN,
                                   I2C_SLAVE_TX_BUF_LEN, 0));

    while (1) {
        len = i2c_slave_read_buffer(I2C_SLAVE_NUM, data_rd + size_rd, DATA_LENGTH, 10000 / portTICK_RATE_MS);

        if (len == 0) {
            break;
        }

        size_rd += len;
    }

    disp_buf(data_rd, size_rd);

    for (int i = 0; i < size_rd; i++) {
        TEST_ASSERT(data_rd[i] == i);
    }

    free(data_rd);
    TEST_ESP_OK(i2c_driver_delete(I2C_SLAVE_NUM));
}

TEST_CASE_MULTIPLE_DEVICES("I2C master write slave test", "[i2c_bus]", i2c_master_write_test, i2c_slave_read_test);

static void master_read_slave_test(void)
{
    uint8_t *data_rd = (uint8_t *) malloc(DATA_LENGTH);
    memset(data_rd, 0, DATA_LENGTH);
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_bus_handle_t i2c0_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    TEST_ASSERT(i2c0_bus != NULL);
    i2c_bus_device_handle_t i2c_device1 = i2c_bus_device_create(i2c0_bus, ESP_SLAVE_ADDR, 0);
    TEST_ASSERT(i2c_device1 != NULL);

    i2c_bus_read_bytes(i2c_device1, NULL_I2C_MEM_ADDR, RW_TEST_LENGTH, data_rd);

    vTaskDelay(100 / portTICK_RATE_MS);

    for (int i = 0; i < RW_TEST_LENGTH; i++) {
        printf("%d\n", data_rd[i]);
    }

    i2c_bus_device_delete(&i2c_device1);
    TEST_ASSERT(i2c_device1 == NULL);
    TEST_ASSERT(ESP_OK == i2c_bus_delete(&i2c0_bus));
    TEST_ASSERT(i2c0_bus == NULL);
    free(data_rd);
}

static void slave_write_buffer_test(void)
{
    uint8_t *data_wr = (uint8_t *) malloc(DATA_LENGTH);
    int size_rd;

    i2c_config_t conf_slave = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave.addr_10bit_en = 0,
        .slave.slave_addr = ESP_SLAVE_ADDR,
    };

    TEST_ESP_OK(i2c_param_config(I2C_SLAVE_NUM, &conf_slave));
    TEST_ESP_OK(i2c_driver_install(I2C_SLAVE_NUM, I2C_MODE_SLAVE,
                                   I2C_SLAVE_RX_BUF_LEN,
                                   I2C_SLAVE_TX_BUF_LEN, 0));

    for (int i = 0; i < DATA_LENGTH / 2; i++) {
        data_wr[i] = i;
    }

    size_rd = i2c_slave_write_buffer(I2C_SLAVE_NUM, data_wr, RW_TEST_LENGTH, 2000 / portTICK_RATE_MS);
    disp_buf(data_wr, size_rd);
    free(data_wr);
    i2c_driver_delete(I2C_SLAVE_NUM);
}

TEST_CASE_MULTIPLE_DEVICES("I2C master read slave test", "[i2c_bus]", master_read_slave_test, slave_write_buffer_test);

TEST_CASE("i2c bus init-deinit test", "[bus][i2c_bus]")
{
    i2c_bus_init_deinit_test();
    i2c_bus_device_add_test();
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
    printf("I2C BUS TEST \n");
    unity_run_menu();
}
