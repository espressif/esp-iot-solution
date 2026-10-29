/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_st77926.h"
#include "esp_lcd_touch_st77926.h"

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (320)
#define TEST_LCD_V_RES              (480)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_2)
#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_1)
#define TEST_PIN_NUM_LCD_DATA1      (GPIO_NUM_5)
#define TEST_PIN_NUM_LCD_DATA2      (GPIO_NUM_6)
#define TEST_PIN_NUM_LCD_DATA3      (GPIO_NUM_7)

#define TEST_I2C_MASTER_NUM         (I2C_NUM_0)
#define TEST_I2C_SDA_GPIO_NUM       (GPIO_NUM_9)
#define TEST_I2C_SCL_GPIO_NUM       (GPIO_NUM_8)

#define TEST_PANEL_X_MAX            (320)
#define TEST_PANEL_Y_MAX            (480)
#define TEST_PANEL_RST_IO_NUM       (GPIO_NUM_4)
#define TEST_PANEL_INT_IO_NUM       (GPIO_NUM_NC)

#define TEST_READ_TIME_MS           (3000)
#define TEST_READ_PERIOD_MS         (30)
#define TEST_TOUCH_CHECK_TIME_MS    (10000)

static const char *TAG = "esp_lcd_touch_st77926";

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

static void test_lcd_panel_create(esp_lcd_panel_io_handle_t *io_handle_p, esp_lcd_panel_handle_t *panel_handle_p)
{
    const spi_bus_config_t buscfg = ST77926_PANEL_BUS_QSPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                  TEST_PIN_NUM_LCD_DATA1, TEST_PIN_NUM_LCD_DATA2,
                                                                  TEST_PIN_NUM_LCD_DATA3,
                                                                  TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = ST77926_PANEL_IO_QSPI_CONFIG(TEST_PIN_NUM_LCD_CS, NULL, NULL);
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, io_handle_p));

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PANEL_RST_IO_NUM,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77926(*io_handle_p, &panel_config, panel_handle_p));
    TEST_ESP_OK(esp_lcd_panel_reset(*panel_handle_p));
    TEST_ESP_OK(esp_lcd_panel_init(*panel_handle_p));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(*panel_handle_p, true));
}

static void test_lcd_panel_delete(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));
}

static void test_touch_panel_create(i2c_master_bus_handle_t bus_handle, gpio_num_t rst_gpio_num, esp_lcd_panel_io_handle_t *io_handle_p,
                                    esp_lcd_touch_handle_t *tp_handle_p)
{
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST77926_CONFIG();
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = TEST_PANEL_X_MAX,
        .y_max = TEST_PANEL_Y_MAX,
        .rst_gpio_num = rst_gpio_num,
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
    TEST_ESP_OK(esp_lcd_touch_new_i2c_st77926(*io_handle_p, &tp_cfg, tp_handle_p));
}

TEST_CASE("test st77926 to read touch point with polling", "[st77926][poll]")
{
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    test_i2c_init(&i2c_bus_handle);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_create(i2c_bus_handle, TEST_PANEL_RST_IO_NUM, &tp_io_handle, &tp_handle);

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

TEST_CASE("test st77926 to check touch points within 10 seconds", "[st77926][touch]")
{
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    test_i2c_init(&i2c_bus_handle);

    esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
    esp_lcd_panel_handle_t lcd_handle = NULL;
    test_lcd_panel_create(&lcd_io_handle, &lcd_handle);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    // The shared TDDI reset line has already been toggled by the LCD init path.
    test_touch_panel_create(i2c_bus_handle, GPIO_NUM_NC, &tp_io_handle, &tp_handle);

    esp_lcd_touch_point_data_t tp_data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS] = {0};
    uint8_t tp_cnt = 0;
    uint32_t touch_sample_count = 0;
    for (int i = 0; i < TEST_TOUCH_CHECK_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));
        tp_cnt = 0;
        TEST_ESP_OK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS));
        if (tp_cnt > 0) {
            touch_sample_count++;
            for (int j = 0; j < tp_cnt; j++) {
                ESP_LOGI(TAG, "Touch sample %" PRIu32 ", point[%d]: %" PRIu16 ",%" PRIu16 ", strength=%" PRIu16,
                         touch_sample_count, j, tp_data[j].x, tp_data[j].y, tp_data[j].strength);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }
    ESP_LOGI(TAG, "Touch check finished, touched samples: %" PRIu32, touch_sample_count);

    TEST_ESP_OK(esp_lcd_touch_del(tp_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(tp_io_handle));
    test_lcd_panel_delete(lcd_io_handle, lcd_handle);
    TEST_ESP_OK(i2c_del_master_bus(i2c_bus_handle));
}

// Some resources are lazy allocated in the LCD driver, the threshold is left for that case
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
     *  __  _____ _____ _____ ___   ___   __
     * / _\/__   \___  |___  / _ \ / _ \ / /_
     * \ \   / /\/  / /   / / (_) | (_) | '_ \
     * _\ \ / /    / /   / / \__, |\__, | (_) |
     * \__/ \/    /_/   /_/    /_/   /_/|_.__/
     */
    printf("  __  _____ _____ _____ ___   ___   __\r\n");
    printf(" / _\\/__   \\___  |___  / _ \\ / _ \\ / /_\r\n");
    printf(" \\ \\   / /\\/  / /   / / (_) | (_) | '_ \\\r\n");
    printf(" _\\ \\ / /    / /   / / \\__, |\\__, | (_) |\r\n");
    printf(" \\__/ \\/    /_/   /_/    /_/   /_/|_.__/\r\n");
    unity_run_menu();
}
