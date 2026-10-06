/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_touch_cst9220.h"

#define TEST_I2C_MASTER_NUM       (I2C_NUM_0)
#define TEST_I2C_SDA_GPIO_NUM     (GPIO_NUM_8)
#define TEST_I2C_SCL_GPIO_NUM     (GPIO_NUM_18)

#define TEST_PANEL_X_MAX          (480)
#define TEST_PANEL_Y_MAX          (480)
#define TEST_PANEL_RST_IO_NUM     (GPIO_NUM_0)
#define TEST_PANEL_INT_IO_NUM     (GPIO_NUM_1)

#define TEST_READ_TIME_MS         (3000)
#define TEST_READ_PERIOD_MS       (30)
#define TEST_SLEEP_TIME_MS        (100)
#define TEST_MEMORY_LEAK_THRESHOLD (-300)

static const char *TAG = "test_cst9220";
static size_t before_free_8bit;
static size_t before_free_32bit;

static void test_i2c_init(i2c_master_bus_handle_t *bus_handle)
{
    const i2c_master_bus_config_t i2c_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TEST_I2C_MASTER_NUM,
        .sda_io_num = TEST_I2C_SDA_GPIO_NUM,
        .scl_io_num = TEST_I2C_SCL_GPIO_NUM,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    TEST_ESP_OK(i2c_new_master_bus(&i2c_config, bus_handle));
}

static void test_touch_create(i2c_master_bus_handle_t bus_handle, esp_lcd_panel_io_handle_t *io_handle,
                              esp_lcd_touch_handle_t *touch_handle)
{
    const esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST9220_CONFIG();
    const esp_lcd_touch_config_t touch_config = {
        .x_max = TEST_PANEL_X_MAX,
        .y_max = TEST_PANEL_Y_MAX,
        .rst_gpio_num = TEST_PANEL_RST_IO_NUM,
        .int_gpio_num = TEST_PANEL_INT_IO_NUM,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    TEST_ESP_OK(esp_lcd_new_panel_io_i2c(bus_handle, &io_config, io_handle));
    TEST_ESP_OK(esp_lcd_touch_new_i2c_cst9220(*io_handle, &touch_config, touch_handle));
}

static void test_touch_delete(i2c_master_bus_handle_t bus_handle, esp_lcd_panel_io_handle_t io_handle,
                              esp_lcd_touch_handle_t touch_handle)
{
    TEST_ESP_OK(esp_lcd_touch_del(touch_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(i2c_del_master_bus(bus_handle));
}

TEST_CASE("test cst9220 touch data polling", "[cst9220][poll]")
{
    i2c_master_bus_handle_t bus_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;
    test_i2c_init(&bus_handle);
    test_touch_create(bus_handle, &io_handle, &touch_handle);

    esp_lcd_touch_point_data_t points[ESP_LCD_TOUCH_CST9220_MAX_POINTS] = {0};
    esp_err_t test_ret = ESP_OK;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        uint8_t point_count = 0;
        test_ret = esp_lcd_touch_read_data(touch_handle);
        if (test_ret != ESP_OK) {
            break;
        }
        test_ret = esp_lcd_touch_get_data(touch_handle, points, &point_count,
                                          ESP_LCD_TOUCH_CST9220_MAX_POINTS);
        if (test_ret != ESP_OK) {
            break;
        }
        for (uint8_t point = 0; point < point_count; point++) {
            ESP_LOGI(TAG, "Touch point[%u]: x=%" PRIu16 " y=%" PRIu16 " track=%u",
                     point, points[point].x, points[point].y, points[point].track_id);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    test_touch_delete(bus_handle, io_handle, touch_handle);
    TEST_ESP_OK(test_ret);
}

TEST_CASE("test cst9220 sleep and reset wakeup", "[cst9220][sleep]")
{
    i2c_master_bus_handle_t bus_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_touch_handle_t touch_handle = NULL;
    test_i2c_init(&bus_handle);
    test_touch_create(bus_handle, &io_handle, &touch_handle);

    TEST_ESP_OK(esp_lcd_touch_enter_sleep(touch_handle));
    TEST_ESP_OK(esp_lcd_touch_enter_sleep(touch_handle));
    TEST_ESP_ERR(ESP_ERR_INVALID_STATE, esp_lcd_touch_read_data(touch_handle));
    vTaskDelay(pdMS_TO_TICKS(TEST_SLEEP_TIME_MS));
    TEST_ESP_OK(esp_lcd_touch_exit_sleep(touch_handle));
    TEST_ESP_OK(esp_lcd_touch_exit_sleep(touch_handle));
    TEST_ESP_OK(esp_lcd_touch_read_data(touch_handle));

    test_touch_delete(bus_handle, io_handle, touch_handle);
}

static void check_leak(size_t before_free, size_t after_free, const char *memory_type)
{
    ssize_t delta = after_free - before_free;
    ESP_LOGI(TAG, "MALLOC_CAP_%s: before=%u after=%u delta=%d", memory_type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    check_leak(before_free_8bit, heap_caps_get_free_size(MALLOC_CAP_8BIT), "8BIT");
    check_leak(before_free_32bit, heap_caps_get_free_size(MALLOC_CAP_32BIT), "32BIT");
}

void app_main(void)
{
    unity_run_menu();
}
