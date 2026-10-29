/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ili2118.h"

#define TEST_I2C_MASTER_NUM        (I2C_NUM_0)
#define TEST_I2C_CLK_SPEED         (90 * 1000)
#define TEST_I2C_SCL_GPIO_NUM      (GPIO_NUM_18)
#define TEST_I2C_SDA_GPIO_NUM      (GPIO_NUM_8)

#define TEST_TOUCH_INT_GPIO        (GPIO_NUM_1)
#define TEST_TOUCH_RST_GPIO        (GPIO_NUM_0)

#define TEST_TOUCH_H_RES           (480)
#define TEST_TOUCH_V_RES           (480)

#define TEST_READ_TOUCH_TIME_MS    (3000)
#define TEST_READ_TOUCH_PERIOD_MS  (30)

static const char *TAG = "ILI2118A_TEST";
static SemaphoreHandle_t touch_mux = NULL;

static void test_i2c_init(void)
{
    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_I2C_SDA_GPIO_NUM,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = TEST_I2C_SCL_GPIO_NUM,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = TEST_I2C_CLK_SPEED
    };
    TEST_ESP_OK(i2c_param_config(TEST_I2C_MASTER_NUM, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0));
}

static void test_i2c_deinit(void)
{
    TEST_ESP_OK(i2c_driver_delete(TEST_I2C_MASTER_NUM));
}

static void test_touch_init(esp_lcd_panel_io_handle_t *io_handle, esp_lcd_touch_handle_t *tp_handle, esp_lcd_touch_interrupt_callback_t callback)
{
    ESP_LOGI(TAG, "Initialize touch controller");

    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ILI2118A_CONFIG();

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = TEST_TOUCH_H_RES,
        .y_max = TEST_TOUCH_V_RES,
        .rst_gpio_num = TEST_TOUCH_RST_GPIO,
        .int_gpio_num = TEST_TOUCH_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .interrupt_callback = callback,
    };

    TEST_ESP_OK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TEST_I2C_MASTER_NUM, &tp_io_config, io_handle));
    TEST_ESP_OK(esp_lcd_touch_new_i2c_ili2118(*io_handle, &tp_cfg, tp_handle));
}

static void test_touch_deinit(esp_lcd_panel_io_handle_t io_handle, esp_lcd_touch_handle_t tp_handle)
{
    TEST_ESP_OK(esp_lcd_touch_del(tp_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
}

static void touch_interrupt_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (touch_mux) {
        xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

TEST_CASE("test ili2118a touch read with interrupt", "[ili2118a][intr]")
{
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;

    touch_mux = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(touch_mux);

    test_i2c_init();
    test_touch_init(&tp_io_handle, &tp_handle, touch_interrupt_callback);

    esp_lcd_touch_point_data_t tp_data[1] = {0};
    uint8_t tp_cnt = 0;
    uint32_t timeout_ticks = TEST_READ_TOUCH_TIME_MS / portTICK_PERIOD_MS;
    uint32_t start_ticks = xTaskGetTickCount();

    while (xTaskGetTickCount() - start_ticks < timeout_ticks) {
        if (xSemaphoreTake(touch_mux, pdMS_TO_TICKS(TEST_READ_TOUCH_PERIOD_MS))) {
            TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));
            tp_cnt = 0;
            ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, 1));
            if (tp_cnt > 0) {
                ESP_LOGI(TAG, "Touch IRQ: (%" PRIu16 ", %" PRIu16 ")", tp_data[0].x, tp_data[0].y);
            }
        }
    }

    test_touch_deinit(tp_io_handle, tp_handle);
    test_i2c_deinit();
    vSemaphoreDelete(touch_mux);
}

TEST_CASE("test ili2118a touch read with polling", "[ili2118a][poll]")
{
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;

    test_i2c_init();
    test_touch_init(&tp_io_handle, &tp_handle, NULL);

    esp_lcd_touch_point_data_t tp_data[1] = {0};
    uint8_t tp_cnt = 0;

    for (int i = 0; i < TEST_READ_TOUCH_TIME_MS / TEST_READ_TOUCH_PERIOD_MS; i++) {
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));
        tp_cnt = 0;
        ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, 1));
        if (tp_cnt > 0) {
            ESP_LOGI(TAG, "Touch Poll: (%" PRIu16 ", %" PRIu16 ")", tp_data[0].x, tp_data[0].y);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_TOUCH_PERIOD_MS));
    }

    test_touch_deinit(tp_io_handle, tp_handle);
    test_i2c_deinit();
}

// Memory leak detection
#define TEST_MEMORY_LEAK_THRESHOLD (-300)

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    /**
     *  ___ _     ___ ____  _ _  ___
     * |_ _| |   |_ _|___ \/ / |( _ )
     *  | || |    | |  __) | | |/ _ \
     *  | || |___ | | / __/| | | (_) |
     * |___|_____|___|_____|_|_|\___/
     */
    printf(" ___ _     ___ ____  _ _  ___\r\n");
    printf("|_ _| |   |_ _|___ \\/ / |( _ )\r\n");
    printf(" | || |    | |  __) | | |/ _ \\\r\n");
    printf(" | || |___ | | / __/| | | (_) |\r\n");
    printf("|___|_____|___|_____|_|_|\\___/\r\n");
    unity_run_menu();
}
