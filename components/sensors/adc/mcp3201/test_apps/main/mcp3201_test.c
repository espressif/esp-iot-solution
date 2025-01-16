/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "mcp3201.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-500)

static size_t before_free_8bit;
static size_t before_free_32bit;

#define SPI_MISO_IO    (gpio_num_t)(2)
#define SPI_SCLK_IO    (gpio_num_t)(3)
#define SPI_CS_IO      (gpio_num_t)(4)
#define SPI_FREQ_HZ    (10 * 1000 * 1000)

static const char *TAG = "MCP3201";
static spi_bus_handle_t spi_bus = NULL;
static mcp3201_handle_t mcp3201 = NULL;

TEST_CASE("mcp3201 init-deinit test", "[spi][sensor][mcp3201]")
{
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = -1,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus != NULL);

    spi_device_config_t dev_cfg = {
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
        .cs_io_num = SPI_CS_IO,
    };

    mcp3201 = mcp3201_create(spi_bus, &dev_cfg);
    mcp3201_delete(&mcp3201);
    spi_bus_delete(&spi_bus);
}

TEST_CASE("mcp3201 get data test", "[spi][sensor][mcp3201]")
{
    int16_t data;
    spi_config_t bus_conf = {
        .miso_io_num = SPI_MISO_IO,
        .mosi_io_num = -1,
        .sclk_io_num = SPI_SCLK_IO,
    };
    spi_bus = spi_bus_create(SPI2_HOST, &bus_conf);
    TEST_ASSERT(spi_bus != NULL);

    spi_device_config_t dev_cfg = {
        .mode = 0,
        .clock_speed_hz = SPI_FREQ_HZ,
        .cs_io_num = SPI_CS_IO,
    };

    mcp3201 = mcp3201_create(spi_bus, &dev_cfg);

    for (int i = 0; i < 50; i++) {
        mcp3201_get_data(mcp3201, &data);
        ESP_LOGI(TAG, "MCP3201:%d,Convert:%.2f", data, 1.0f * data * (3.3f / 4096));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    mcp3201_delete(&mcp3201);
    spi_bus_delete(&spi_bus);
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
    printf(" ____    ____   ______  _______   ______    _____     ____    __    \n");
    printf("|_   \\  /   _|.' ___  ||_   __ \\ / ____ `. / ___ `. .'    '. /  |   \n");
    printf("  |   \\/   | / .'   \\_|  | |__) |`'  __) ||_/___) ||  .--.  |`| |   \n");
    printf("  | |\\  /| | | |         |  ___/ _  |__ '. .'____.'| |    | | | |   \n");
    printf(" _| |_\\/_| | \\ `.___.'\\ _| |_   | \\____) |/ /_____ |  `--'  |_| |_  \n");
    printf("|_____||_____| `.____ .'|_____|   \\______.'|_______| '.____.'|_____| \n");
    unity_run_menu();
}
