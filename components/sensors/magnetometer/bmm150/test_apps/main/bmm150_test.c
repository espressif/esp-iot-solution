/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmm150.h"
#include "bmm150_common.h"
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

static const char *TAG = "BMM150_TEST";

static i2c_bus_handle_t i2c_bus = NULL;
static struct bmm150_dev bmm150_dev;

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

static int8_t bmm150_probe_and_init(void)
{
    int8_t rslt = BMM150_E_DEV_NOT_FOUND;
    const uint8_t addrs[] = {
        BMM150_DEFAULT_I2C_ADDRESS,
        BMM150_I2C_ADDRESS_CSB_LOW_SDO_HIGH,
        BMM150_I2C_ADDRESS_CSB_HIGH_SDO_LOW,
        BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH,
    };

    bmm150_set_i2c_bus_handle(i2c_bus);

    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        memset(&bmm150_dev, 0, sizeof(bmm150_dev));
        bmm150_set_i2c_address(addrs[i]);
        rslt = bmm150_interface_init(&bmm150_dev);
        if (rslt != BMM150_OK) {
            continue;
        }

        rslt = bmm150_init(&bmm150_dev);
        ESP_LOGI(TAG, "Probe 0x%02X -> rslt=%d chip_id=0x%02X", addrs[i], rslt, bmm150_dev.chip_id);
        if (rslt == BMM150_OK && bmm150_dev.chip_id == BMM150_CHIP_ID) {
            return BMM150_OK;
        }
    }

    return BMM150_E_DEV_NOT_FOUND;
}

TEST_CASE("bmm150 init-deinit test", "[i2c][sensor][bmm150]")
{
    int8_t rslt;
    esp_err_t ret;

    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    rslt = bmm150_probe_and_init();
    if (rslt == BMM150_OK) {
        ESP_LOGI(TAG, "BMM150 initialized successfully");
    }

    bmm150_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BMM150_OK, rslt);
}

TEST_CASE("bmm150 normal mode read test", "[i2c][sensor][bmm150]")
{
    int8_t rslt;
    struct bmm150_mag_data data;
    struct bmm150_settings settings = {0};
    esp_err_t ret;

    ret = i2c_master_init();
    if (ret != ESP_OK) {
        TEST_FAIL_MESSAGE("I2C master init failed");
    }

    rslt = bmm150_probe_and_init();
    if (rslt != BMM150_OK) {
        goto cleanup;
    }

    settings.preset_mode = BMM150_PRESETMODE_REGULAR;
    rslt = bmm150_set_presetmode(&settings, &bmm150_dev);
    if (rslt != BMM150_OK) {
        goto cleanup;
    }

    settings.pwr_mode = BMM150_POWERMODE_NORMAL;
    rslt = bmm150_set_op_mode(&settings, &bmm150_dev);
    if (rslt != BMM150_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "Sample, MagX(uT), MagY(uT), MagZ(uT)");
    for (int sample = 1; sample <= 10; sample++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        rslt = bmm150_read_mag_data(&data, &bmm150_dev);
        if (rslt != BMM150_OK) {
            goto cleanup;
        }
        ESP_LOGI(TAG, "%d, %d, %d, %d", sample, (int)data.x, (int)data.y, (int)data.z);
    }

    settings.pwr_mode = BMM150_POWERMODE_SUSPEND;
    (void)bmm150_set_op_mode(&settings, &bmm150_dev);

cleanup:
    bmm150_interface_deinit();
    i2c_master_deinit();

    TEST_ASSERT_EQUAL(BMM150_OK, rslt);
}

#if !CONFIG_I2C_BUS_BACKWARD_CONFIG
/* Adding a device does not probe the bus, so this runs without sensors attached. */
TEST_CASE("bmm150 native dual instance test", "[i2c][sensor][bmm150]")
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_IO,
        .scl_io_num = I2C_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    TEST_ESP_OK(i2c_new_master_bus(&bus_config, &bus));

    struct bmm150_dev first = {0};
    struct bmm150_dev second = {0};
    TEST_ESP_OK(bmm150_interface_init_from_master_bus(&first, bus, BMM150_DEFAULT_I2C_ADDRESS, 400000));
    TEST_ESP_OK(bmm150_interface_init_from_master_bus(&second, bus, BMM150_I2C_ADDRESS_CSB_LOW_SDO_HIGH, 400000));
    TEST_ASSERT_NOT_NULL(first.intf_ptr);
    TEST_ASSERT_NOT_NULL(second.intf_ptr);
    TEST_ASSERT_TRUE(first.intf_ptr != second.intf_ptr);

    struct bmm150_dev invalid = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bmm150_interface_init_from_master_bus(NULL, bus, BMM150_DEFAULT_I2C_ADDRESS, 400000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bmm150_interface_init_from_master_bus(&invalid, NULL, BMM150_DEFAULT_I2C_ADDRESS, 400000));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bmm150_interface_init_from_master_bus(&invalid, bus, BMM150_DEFAULT_I2C_ADDRESS, 0));
    TEST_ASSERT_NULL(invalid.intf_ptr);

    /* Deleting one instance leaves the other usable. */
    TEST_ESP_OK(bmm150_interface_deinit_device(&first));
    TEST_ASSERT_NULL(first.intf_ptr);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bmm150_interface_deinit_device(&first));
    TEST_ASSERT_NOT_NULL(second.intf_ptr);

    TEST_ESP_OK(bmm150_interface_deinit_device(&second));
    TEST_ESP_OK(i2c_del_master_bus(bus));
}
#endif

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
    printf("BMM150 TEST\n");
    unity_run_menu();
}
