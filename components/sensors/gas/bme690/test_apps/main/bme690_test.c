/* SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bme69x.h"
#include "bme690_common.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MEMORY_LEAK_THRESHOLD (-500)

static size_t before_free_8bit;
static size_t before_free_32bit;

// For ESP-SensairShuttle
#define I2C_SCL_IO (GPIO_NUM_3)
#define I2C_SDA_IO (GPIO_NUM_2)
#define I2C_FREQ_HZ (100000)

#define SPI_CS_IO (GPIO_NUM_10)
#define SPI_MISO_IO (GPIO_NUM_9)
#define SPI_MOSI_IO (GPIO_NUM_2)
#define SPI_CLK_IO (GPIO_NUM_3)

// BME690 SDO pin - controls I2C address
// SDO = 0 -> I2C address 0x76
// SDO = 1 -> I2C address 0x77
#define BME690_SDO_PIN (GPIO_NUM_9)
#define BME690_SDO_LEVEL (0)

static const char *TAG = "BME690_TEST";

static i2c_bus_handle_t i2c_bus = NULL;
static spi_device_handle_t spi_dev = NULL;
static struct bme69x_dev bme690_dev;

static esp_err_t i2c_master_init(void)
{
    esp_err_t ret;

    if (BME690_SDO_PIN != GPIO_NUM_NC) {
        gpio_config_t sdo_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << BME690_SDO_PIN),
            .pull_down_en = 0,
            .pull_up_en = 0,
        };
        ret = gpio_config(&sdo_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO config failed");
            return ret;
        }
        gpio_set_level(BME690_SDO_PIN, BME690_SDO_LEVEL);
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "I2C master bus initialized on port %d", I2C_NUM_0);
    ESP_LOGI(TAG, "SDA: GPIO%d, SCL: GPIO%d, Freq: %d Hz", I2C_SDA_IO, I2C_SCL_IO,
             I2C_FREQ_HZ);
    ESP_LOGI(TAG, "SDO: GPIO%d (set to 0 for I2C addr 0x76)", BME690_SDO_PIN);

    // Add delay after I2C initialization
    vTaskDelay(pdMS_TO_TICKS(500));

    return ESP_OK;
}

static void i2c_master_deinit(void)
{
    if (i2c_bus) {
        i2c_bus_delete(&i2c_bus);
        i2c_bus = NULL;
        ESP_LOGI(TAG, "I2C master bus deinitialized");
    }
}

static esp_err_t spi_master_init(void)
{
    esp_err_t ret;

    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SPI_MOSI_IO,
        .miso_io_num = SPI_MISO_IO,
        .sclk_io_num = SPI_CLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed");
        return ret;
    }

    // Configure SPI device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,  // 1 MHz
        .mode = 0,                           // SPI mode 0
        .spics_io_num = SPI_CS_IO,
        .queue_size = 1,
        .flags = 0,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed");
        spi_bus_free(SPI2_HOST);
        return ret;
    }

    ESP_LOGI(TAG, "SPI bus initialized on SPI2_HOST");
    ESP_LOGI(TAG, "MOSI: GPIO%d, MISO: GPIO%d, CLK: GPIO%d, CS: GPIO%d",
             SPI_MOSI_IO, SPI_MISO_IO, SPI_CLK_IO, SPI_CS_IO);

    // Add delay after SPI initialization
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

static void spi_master_deinit(void)
{
    if (spi_dev) {
        spi_bus_remove_device(spi_dev);
        spi_dev = NULL;
    }
    spi_bus_free(SPI2_HOST);
    ESP_LOGI(TAG, "SPI bus deinitialized");
}

TEST_CASE("bme690 init-deinit test", "[i2c][sensor][bme690]")
{
    int8_t rslt;
    esp_err_t ret;

    // Initialize I2C bus
    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    // Set I2C bus handle for BME690
    bme69x_set_i2c_bus_handle(i2c_bus);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_I2C_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 initialized successfully");

cleanup:
    bme69x_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

TEST_CASE("bme690 forced mode test", "[i2c][sensor][bme690]")
{
    int8_t rslt = BME69X_OK;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data;
    uint32_t del_period;
    uint8_t n_fields;
    esp_err_t ret;

    // Initialize I2C bus
    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    // Set I2C bus handle for BME690
    bme69x_set_i2c_bus_handle(i2c_bus);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_I2C_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Configure sensor
    conf.filter = BME69X_FILTER_OFF;
    conf.odr = BME69X_ODR_NONE;
    conf.os_hum = BME69X_OS_16X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_2X;
    rslt = bme69x_set_conf(&conf, &bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Configure heater
    heatr_conf.enable = BME69X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, &bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 configured, starting measurements...");
    ESP_LOGI(
        TAG,
        "Sample, Temperature(°C), Pressure(Pa), Humidity(%%), Gas(Ω), Status");

    // Take 10 measurements
    for (int sample = 1; sample <= 10; sample++) {
        // Trigger measurement
        rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &bme690_dev);
        if (rslt != BME69X_OK) {
            goto cleanup;
        }

        // Calculate delay period
        del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, &conf, &bme690_dev) +
                     (heatr_conf.heatr_dur * 1000);
        bme690_dev.delay_us(del_period, bme690_dev.intf_ptr);

        // Read data
        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &bme690_dev);
        if (rslt != BME69X_OK) {
            goto cleanup;
        }

        if (n_fields) {
#ifdef BME69X_USE_FPU
            ESP_LOGI(TAG, "%d, %.2f, %.2f, %.2f, %.2f, 0x%x", sample,
                     data.temperature, data.pressure, data.humidity,
                     data.gas_resistance, data.status);
#else
            ESP_LOGI(TAG, "%d, %d, %ld, %ld, %ld, 0x%x", sample, data.temperature,
                     (long)data.pressure, (long)data.humidity,
                     (long)data.gas_resistance, data.status);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

cleanup:
    bme69x_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

TEST_CASE("bme690 self test", "[i2c][sensor][bme690]")
{
    int8_t rslt;
    esp_err_t ret;

    // Initialize I2C bus
    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    // Set I2C bus handle for BME690
    bme69x_set_i2c_bus_handle(i2c_bus);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_I2C_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Run self-test
    ESP_LOGI(TAG, "Running BME690 self-test...");
    rslt = bme69x_selftest_check(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 self-test passed");

cleanup:
    bme69x_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

TEST_CASE("bme690 spi init-deinit test", "[spi][sensor][bme690]")
{
    int8_t rslt;
    esp_err_t ret;

    // Initialize SPI bus
    ret = spi_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("SPI master init failed");
    }

    // Set SPI device handle for BME690
    bme69x_set_spi_device_handle(spi_dev);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_SPI_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 initialized successfully via SPI");

cleanup:
    bme69x_interface_deinit();
    spi_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

TEST_CASE("bme690 spi forced mode test", "[spi][sensor][bme690]")
{
    int8_t rslt = BME69X_OK;
    struct bme69x_conf conf;
    struct bme69x_heatr_conf heatr_conf;
    struct bme69x_data data;
    uint32_t del_period;
    uint8_t n_fields;
    esp_err_t ret;

    // Initialize SPI bus
    ret = spi_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("SPI master init failed");
    }

    // Set SPI device handle for BME690
    bme69x_set_spi_device_handle(spi_dev);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_SPI_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Configure sensor
    conf.filter = BME69X_FILTER_OFF;
    conf.odr = BME69X_ODR_NONE;
    conf.os_hum = BME69X_OS_16X;
    conf.os_pres = BME69X_OS_1X;
    conf.os_temp = BME69X_OS_2X;
    rslt = bme69x_set_conf(&conf, &bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Configure heater
    heatr_conf.enable = BME69X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    rslt = bme69x_set_heatr_conf(BME69X_FORCED_MODE, &heatr_conf, &bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 configured via SPI, starting measurements...");
    ESP_LOGI(
        TAG,
        "Sample, Temperature(°C), Pressure(Pa), Humidity(%%), Gas(Ω), Status");

    // Take 10 measurements
    for (int sample = 1; sample <= 10; sample++) {
        // Trigger measurement
        rslt = bme69x_set_op_mode(BME69X_FORCED_MODE, &bme690_dev);
        if (rslt != BME69X_OK) {
            goto cleanup;
        }

        // Calculate delay period
        del_period = bme69x_get_meas_dur(BME69X_FORCED_MODE, &conf, &bme690_dev) +
                     (heatr_conf.heatr_dur * 1000);
        bme690_dev.delay_us(del_period, bme690_dev.intf_ptr);

        // Read data
        rslt = bme69x_get_data(BME69X_FORCED_MODE, &data, &n_fields, &bme690_dev);
        if (rslt != BME69X_OK) {
            goto cleanup;
        }

        if (n_fields) {
#ifdef BME69X_USE_FPU
            ESP_LOGI(TAG, "%d, %.2f, %.2f, %.2f, %.2f, 0x%x", sample,
                     data.temperature, data.pressure, data.humidity,
                     data.gas_resistance, data.status);
#else
            ESP_LOGI(TAG, "%d, %d, %ld, %ld, %ld, 0x%x", sample, data.temperature,
                     (long)data.pressure, (long)data.humidity,
                     (long)data.gas_resistance, data.status);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

cleanup:
    bme69x_interface_deinit();
    spi_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

TEST_CASE("bme690 spi self test", "[spi][sensor][bme690]")
{
    int8_t rslt;
    esp_err_t ret;

    // Initialize SPI bus
    ret = spi_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("SPI master init failed");
    }

    // Set SPI device handle for BME690
    bme69x_set_spi_device_handle(spi_dev);

    // Initialize BME690 interface
    rslt = bme69x_interface_init(&bme690_dev, BME69X_SPI_INTF);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Initialize BME690
    rslt = bme69x_init(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    // Run self-test
    ESP_LOGI(TAG, "Running BME690 self-test via SPI...");
    rslt = bme69x_selftest_check(&bme690_dev);
    if (rslt != BME69X_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "BME690 SPI self-test passed");

cleanup:
    bme69x_interface_deinit();
    spi_master_deinit();

    TEST_ASSERT_EQUAL(BME69X_OK, rslt);
}

static void check_leak(size_t before_free, size_t after_free,
                       const char *type)
{
    ssize_t delta = after_free - before_free;
    printf(
        "MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n",
        type, before_free, after_free, delta);
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
    printf(" ____    __  __   _____    __     ___     ___  \n");
    printf("| __ )  |  \\/  | | ____|  / /_   / _ \\   / _ \\ \n");
    printf("|  _ \\  | |\\/| | |  _|   | '_ \\ | (_) | | | | |\n");
    printf("| |_) | | |  | | | |___  | (_) | \\__, | | |_| |\n");
    printf("|____/  |_|  |_| |_____|  \\___/    /_/   \\___/ \n");
    printf("\n");
    printf("BME690 Gas Sensor Test Application\n");
    printf("===================================\n");
    unity_run_menu();
}
