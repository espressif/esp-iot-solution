/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmm350.h"
#include "bmm350_common.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
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

/* For ESP-SensairShuttle */
#define I2C_SCL_IO (GPIO_NUM_3)
#define I2C_SDA_IO (GPIO_NUM_2)
#define I2C_FREQ_HZ (100000)

static const char *TAG = "BMM350_TEST";

static i2c_bus_handle_t i2c_bus = NULL;
static struct bmm350_dev bmm350_dev;

static esp_err_t i2c_master_init(void)
{
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
    ESP_LOGI(TAG, "SDA: GPIO%d, SCL: GPIO%d, Freq: %d Hz", I2C_SDA_IO, I2C_SCL_IO, I2C_FREQ_HZ);
    vTaskDelay(pdMS_TO_TICKS(100));
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

static int8_t bmm350_probe_and_init(void)
{
    int8_t rslt = BMM350_E_DEV_NOT_FOUND;
    const uint8_t addrs[] = {
        BMM350_I2C_ADSEL_SET_LOW,
        BMM350_I2C_ADSEL_SET_HIGH,
    };

    bmm350_set_i2c_bus_handle(i2c_bus);

    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        memset(&bmm350_dev, 0, sizeof(bmm350_dev));
        bmm350_set_i2c_address(addrs[i]);
        rslt = bmm350_interface_init(&bmm350_dev);
        if (rslt != BMM350_OK) {
            continue;
        }

        rslt = bmm350_init(&bmm350_dev);
        ESP_LOGI(TAG, "Probe 0x%02X -> rslt=%d chip_id=0x%02X", addrs[i], rslt, bmm350_dev.chip_id);
        if (rslt == BMM350_OK && bmm350_dev.chip_id == BMM350_CHIP_ID) {
            return BMM350_OK;
        }
    }

    return BMM350_E_DEV_NOT_FOUND;
}

TEST_CASE("bmm350 init-deinit test", "[i2c][sensor][bmm350]")
{
    int8_t rslt;
    esp_err_t ret;

    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    rslt = bmm350_probe_and_init();
    if (rslt == BMM350_OK) {
        ESP_LOGI(TAG, "BMM350 initialized successfully");
    }

    bmm350_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BMM350_OK, rslt);
}

TEST_CASE("bmm350 normal mode read test", "[i2c][sensor][bmm350]")
{
    int8_t rslt;
    struct bmm350_mag_temp_data data;
    esp_err_t ret;

    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    rslt = bmm350_probe_and_init();
    if (rslt != BMM350_OK) {
        goto cleanup;
    }

    rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, &bmm350_dev);
    if (rslt != BMM350_OK) {
        goto cleanup;
    }

    rslt = bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, &bmm350_dev);
    if (rslt != BMM350_OK) {
        goto cleanup;
    }

    rslt = bmm350_set_powermode(BMM350_NORMAL_MODE, &bmm350_dev);
    if (rslt != BMM350_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sample, MagX(uT), MagY(uT), MagZ(uT), Temp(C)");
    for (int sample = 1; sample <= 10; sample++) {
        vTaskDelay(pdMS_TO_TICKS(20));
        rslt = bmm350_get_compensated_mag_xyz_temp_data(&data, &bmm350_dev);
        if (rslt != BMM350_OK) {
            goto cleanup;
        }
        ESP_LOGI(TAG, "%d, %.2f, %.2f, %.2f, %.2f", sample,
                 data.x, data.y, data.z, data.temperature);
    }

    (void)bmm350_set_powermode(BMM350_SUSPEND_MODE, &bmm350_dev);

cleanup:
    bmm350_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BMM350_OK, rslt);
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n",
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
    printf("BMM350 TEST\n");
    unity_run_menu();
}
