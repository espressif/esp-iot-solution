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

#include "esp_lcd_touch_ft6336u.h"

#define TEST_I2C_MASTER_NUM         (I2C_NUM_0)
#define TEST_I2C_SDA_GPIO_NUM       (GPIO_NUM_8)
#define TEST_I2C_SCL_GPIO_NUM       (GPIO_NUM_18)

#define TEST_PANEL_X_MAX            (480)
#define TEST_PANEL_Y_MAX            (480)
#define TEST_PANEL_RST_IO_NUM       (GPIO_NUM_NC)
#define TEST_PANEL_INT_IO_NUM       (GPIO_NUM_NC)

#define TEST_READ_TIME_MS           (3000)
#define TEST_READ_PERIOD_MS         (30)

static const char *TAG = "esp_lcd_touch_ft6336u";

static void test_i2c_init(i2c_master_bus_handle_t *bus_handle)
{
    const i2c_master_bus_config_t i2c_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = TEST_I2C_MASTER_NUM,
        .sda_io_num = TEST_I2C_SDA_GPIO_NUM,
        .scl_io_num = TEST_I2C_SCL_GPIO_NUM,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    TEST_ESP_OK(i2c_new_master_bus(&i2c_conf, bus_handle));
}

static void test_touch_panel_create(i2c_master_bus_handle_t bus_handle, esp_lcd_panel_io_handle_t *io_handle_p,
                                    esp_lcd_touch_handle_t *tp_handle_p)
{
    const esp_lcd_panel_io_i2c_config_t tp_io_config =
        ESP_LCD_TOUCH_IO_I2C_FT6336U_CONFIG_WITH_ADDR(ESP_LCD_TOUCH_IO_I2C_FT6336U_ADDRESS_ALT);
    const esp_lcd_touch_config_t tp_cfg = {
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

    TEST_ESP_OK(esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_config, io_handle_p));
    TEST_ESP_OK(esp_lcd_touch_new_i2c_ft6336u(*io_handle_p, &tp_cfg, tp_handle_p));
}

TEST_CASE("test ft6336u to read touch point with polling", "[ft6336u][poll]")
{
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    test_i2c_init(&i2c_bus_handle);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_create(i2c_bus_handle, &tp_io_handle, &tp_handle);

    esp_lcd_touch_point_data_t tp_data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));
        tp_cnt = 0;
        TEST_ESP_OK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS));
        for (int j = 0; j < tp_cnt; j++) {
            ESP_LOGI(TAG, "Touch point[%d]: %" PRIu16 ",%" PRIu16 ", strength=%" PRIu16,
                     j, tp_data[j].x, tp_data[j].y, tp_data[j].strength);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    TEST_ESP_OK(esp_lcd_touch_del(tp_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(tp_io_handle));
    TEST_ESP_OK(i2c_del_master_bus(i2c_bus_handle));
}

#define TEST_MEMORY_LEAK_THRESHOLD (-300)

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
    /**
     *  _____ _____ __  __________  __   _   _
     * |  ___|_   _/ /_|___ /___ / / /_ | | | |
     * | |_    | || '_ \ |_ \ |_ \| '_ \| | | |
     * |  _|   | || (_) |__) |__) | (_) | |_| |
     * |_|     |_| \___/____/____/ \___/ \___/
     */
    printf("  _____ _____ __  __________  __   _   _ \r\n");
    printf(" |  ___|_   _/ /_|___ /___ / / /_ | | | |\r\n");
    printf(" | |_    | || '_ \\ |_ \\ |_ \\| '_ \\| | | |\r\n");
    printf(" |  _|   | || (_) |__) |__) | (_) | |_| |\r\n");
    printf(" |_|     |_| \\___/____/____/ \\___/ \\___/ \r\n");
    unity_run_menu();
}
