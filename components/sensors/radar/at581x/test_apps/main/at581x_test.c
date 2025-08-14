/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "sdkconfig.h"
#include "i2c_bus.h"
#include "at581x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

#define I2C_MASTER_SCL_IO   CONFIG_I2C_MASTER_SCL       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO   CONFIG_I2C_MASTER_SDA       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ  100000                      /*!< I2C master clock frequency */
#define RADAR_OUTPUT_IO     CONFIG_RADAR_OUTPUT         /*!< Radar output IO */

static at581x_dev_handle_t at581x = NULL;
static i2c_bus_handle_t i2c_bus = NULL;

static void IRAM_ATTR at581x_isr_callback(void *arg)
{
    at581x_i2c_config_t *config = (at581x_i2c_config_t *)arg;
    esp_rom_printf("[%s],GPIO[%d] intr, val: %d\n", __FUNCTION__, config->int_gpio_num, gpio_get_level(config->int_gpio_num));
}

static void i2c_sensor_at581x_init(void)
{
    const at581x_default_cfg_t def_cfg = ATH581X_INITIALIZATION_CONFIG();

    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
    TEST_ASSERT_NOT_NULL_MESSAGE(i2c_bus, "i2c_bus create returned NULL");

    at581x_i2c_config_t i2c_conf = {
        .bus_inst = i2c_bus,
        .i2c_addr = AT581X_ADDRRES_0,

        .int_gpio_num = RADAR_OUTPUT_IO,
        .interrupt_level = 1,
        .interrupt_callback = at581x_isr_callback,

        .def_conf = &def_cfg,
    };

    at581x_new_sensor(&i2c_conf, &at581x);
    TEST_ASSERT_NOT_NULL_MESSAGE(at581x, "AT581X create returned NULL");
}

TEST_CASE("sensor at581x test", "[at581x][iot][sensor]")
{
    esp_err_t ret;

    i2c_sensor_at581x_init();

    vTaskDelay(pdMS_TO_TICKS(8000 * 1));

    at581x_del_sensor(at581x);
    ret = i2c_bus_delete(&i2c_bus);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    printf("AT581X TEST \n");
    unity_run_menu();
}
