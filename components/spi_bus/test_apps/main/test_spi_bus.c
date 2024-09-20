/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_config.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_bus.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-500)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define SPI_MISO_IO  (gpio_num_t)(15)
#define SPI_MOSI_IO  (gpio_num_t)(16)
#define SPI_SCLK_IO  (gpio_num_t)(17)
#define SPI_CS_IO    (gpio_num_t)(18)

#define DATA_LENGTH          200
#define SPI_FREQ_HZ    (10 * 1000 * 1000)

TEST_CASE("spi bus init-deinit test", "[bus][spi_bus]")
{
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = SPI_MOSI_IO,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus_handle_t spi_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus_handle != NULL);

    spi_device_config_t device_conf = {
        .cs_io_num = SPI_CS_IO,
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
    };
    spi_bus_device_handle_t spi_device_handle = spi_bus_device_create(spi_bus_handle, &device_conf);
    TEST_ASSERT(spi_device_handle != NULL);

    spi_bus_device_delete(&spi_device_handle);
    TEST_ASSERT(spi_device_handle == NULL);
    TEST_ASSERT(ESP_OK == spi_bus_delete(&spi_bus_handle));
    TEST_ASSERT(spi_bus_handle == NULL);
}

TEST_CASE("spi bus transfer byte test", "[bus][spi_bus][byte]")
{
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = SPI_MOSI_IO,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus_handle_t spi_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus_handle != NULL);

    spi_device_config_t device_conf = {
        .cs_io_num = SPI_CS_IO,
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
    };
    spi_bus_device_handle_t spi_device_handle = spi_bus_device_create(spi_bus_handle, &device_conf);
    TEST_ASSERT(spi_device_handle != NULL);

    for (int i = 0; i < DATA_LENGTH; i++) {
        uint8_t in = 0;
        spi_bus_transfer_byte(spi_device_handle, i, &in);
        TEST_ASSERT_EQUAL_UINT8(i, in);
    }

    spi_bus_device_delete(&spi_device_handle);
    TEST_ASSERT(spi_device_handle == NULL);
    TEST_ASSERT(ESP_OK == spi_bus_delete(&spi_bus_handle));
    TEST_ASSERT(spi_bus_handle == NULL);
}

TEST_CASE("spi bus transfer bytes test", "[bus][spi_bus][bytes]")
{
    uint8_t *data = (uint8_t *) malloc(DATA_LENGTH * sizeof(uint8_t));
    uint8_t *data_in = (uint8_t *) malloc(DATA_LENGTH * sizeof(uint8_t));
    TEST_ASSERT(data != NULL);
    TEST_ASSERT(data_in != NULL);

    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = SPI_MOSI_IO,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus_handle_t spi_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus_handle != NULL);

    spi_device_config_t device_conf = {
        .cs_io_num = SPI_CS_IO,
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
    };
    spi_bus_device_handle_t spi_device_handle = spi_bus_device_create(spi_bus_handle, &device_conf);
    TEST_ASSERT(spi_device_handle != NULL);

    for (int i = 0; i < DATA_LENGTH; i++) {
        data[i] = i;
    }

    TEST_ASSERT(ESP_OK == spi_bus_transfer_bytes(spi_device_handle, data, data_in, DATA_LENGTH));
    for (int i = 0; i < DATA_LENGTH; i++) {
        TEST_ASSERT_EQUAL_UINT8(data[i], data_in[i]);
    }

    spi_bus_device_delete(&spi_device_handle);
    TEST_ASSERT(spi_device_handle == NULL);
    TEST_ASSERT(ESP_OK == spi_bus_delete(&spi_bus_handle));
    TEST_ASSERT(spi_bus_handle == NULL);

    free(data);
    data = NULL;
    free(data_in);
    data_in = NULL;
}

TEST_CASE("spi bus transfer reg16 test", "[bus][spi_bus][reg16]")
{
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = SPI_MOSI_IO,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus_handle_t spi_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus_handle != NULL);

    spi_device_config_t device_conf = {
        .cs_io_num = SPI_CS_IO,
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
    };
    spi_bus_device_handle_t spi_device_handle = spi_bus_device_create(spi_bus_handle, &device_conf);
    TEST_ASSERT(spi_device_handle != NULL);

    for (uint16_t i = (0xffff - 200); i < 0xffff; i++) {
        uint16_t in = 0;
        TEST_ASSERT(ESP_OK == spi_bus_transfer_reg16(spi_device_handle, i, &in));
        TEST_ASSERT_EQUAL_UINT16(i, in);
    }

    spi_bus_device_delete(&spi_device_handle);
    TEST_ASSERT(spi_device_handle == NULL);
    TEST_ASSERT(ESP_OK == spi_bus_delete(&spi_bus_handle));
    TEST_ASSERT(spi_bus_handle == NULL);
}

TEST_CASE("spi bus transfer reg32 test", "[bus][spi_bus][reg32]")
{
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = SPI_MOSI_IO,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus_handle_t spi_bus_handle = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus_handle != NULL);

    spi_device_config_t device_conf = {
        .cs_io_num = SPI_CS_IO,
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
    };
    spi_bus_device_handle_t spi_device_handle = spi_bus_device_create(spi_bus_handle, &device_conf);
    TEST_ASSERT(spi_device_handle != NULL);

    for (uint32_t i = (0xffffffff - 200); i < 0xffffffff; i++) {
        uint32_t in = 0;
        TEST_ASSERT(ESP_OK == spi_bus_transfer_reg32(spi_device_handle, i, &in));
        TEST_ASSERT_EQUAL_UINT32(i, in);
    }

    spi_bus_device_delete(&spi_device_handle);
    TEST_ASSERT(spi_device_handle == NULL);
    TEST_ASSERT(ESP_OK == spi_bus_delete(&spi_bus_handle));
    TEST_ASSERT(spi_bus_handle == NULL);
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
    printf("SPI BUS TEST \n");
    unity_run_menu();
}
