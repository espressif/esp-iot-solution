/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_interface.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_touch_spd2010.h"

#define TEST_I2C_MASTER_NUM         (I2C_NUM_0)
#define TEST_I2C_CLK_SPEED          (400 * 1000)
#define TEST_I2C_SCL_GPIO_NUM       (GPIO_NUM_18)
#define TEST_I2C_SDA_GPIO_NUM       (GPIO_NUM_8)

#define TEST_PANEL_X_MAX            (412)
#define TEST_PANEL_Y_MAX            (412)
#define TEST_PANEL_INT_IO_NUM       (GPIO_NUM_1)
#define TEST_PANEL_RST_IO_NUM       (GPIO_NUM_0)

#define TEST_READ_TIME_MS           (3000)
#define TEST_READ_PERIOD_MS         (30)

static char *TAG = "esp_lcd_touch_spd2010";
static SemaphoreHandle_t touch_mux = NULL;

static void test_i2c_init(void)
{
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_I2C_SDA_GPIO_NUM,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = TEST_I2C_SCL_GPIO_NUM,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = TEST_I2C_CLK_SPEED
    };
    TEST_ESP_OK(i2c_param_config(TEST_I2C_MASTER_NUM, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0));
}

static void test_touch_panel_crate(esp_lcd_panel_io_handle_t *io_handle_p, esp_lcd_touch_handle_t *tp_handle_p, esp_lcd_touch_interrupt_callback_t callback)
{
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_SPD2010_CONFIG();
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
        .interrupt_callback = callback,
    };
    TEST_ESP_OK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TEST_I2C_MASTER_NUM, &tp_io_config, io_handle_p));
    TEST_ESP_OK(esp_lcd_touch_new_i2c_spd2010(*io_handle_p, &tp_cfg, tp_handle_p));
}

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

TEST_CASE("test spd2010 to read touch point with interruption", "[spd2010][interrupt]")
{
    // Todo: Need to initialize LCD panel with specific initialization sequence, will be done in the future

    test_i2c_init();

    touch_mux = xSemaphoreCreateBinary();
    TEST_ASSERT(touch_mux != NULL);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_crate(&tp_io_handle, &tp_handle, touch_callback);

    bool tp_pressed = false;
    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
            TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle)); // read only when ISR was triggled
        }
        /* Read data from touch controller */
        tp_pressed = esp_lcd_touch_get_coordinates(tp_handle, &tp_x, &tp_y, NULL, &tp_cnt, 1);
        if (tp_pressed && (tp_cnt > 0)) {
            ESP_LOGI(TAG, "Touch position: %d,%d", tp_x, tp_y);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    i2c_driver_delete(TEST_I2C_MASTER_NUM);
    esp_lcd_touch_del(tp_handle);
    esp_lcd_panel_io_del(tp_io_handle);
    vSemaphoreDelete(touch_mux);
    gpio_uninstall_isr_service();
}

TEST_CASE("test spd2010 to read touch point with polling", "[spd2010][poll]")
{
    // Todo: Need to initialize LCD panel with specific initialization sequence, will be done in the future

    test_i2c_init();

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_crate(&tp_io_handle, &tp_handle, NULL);

    bool tp_pressed = false;
    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle)); // read only when ISR was triggled
        /* Read data from touch controller */
        tp_pressed = esp_lcd_touch_get_coordinates(tp_handle, &tp_x, &tp_y, NULL, &tp_cnt, 1);
        if (tp_pressed && (tp_cnt > 0)) {
            ESP_LOGI(TAG, "Touch position: %d,%d", tp_x, tp_y);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    i2c_driver_delete(TEST_I2C_MASTER_NUM);
    esp_lcd_touch_del(tp_handle);
    esp_lcd_panel_io_del(tp_io_handle);
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
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
     *  __    ___  ___ ____   ___  _  ___
     * / _\  / _ \/   \___ \ / _ \/ |/ _ \
     * \ \  / /_)/ /\ / __) | | | | | | | |
     * _\ \/ ___/ /_// / __/| |_| | | |_| |
     * \__/\/  /___,' |_____|\___/|_|\___/
     */
    printf(" __    ___  ___ ____   ___  _  ___\r\n");
    printf("/ _\\  / _ \\/   \\___ \\ / _ \\/ |/ _ \\\r\n");
    printf("\\ \\  / /_)/ /\\ / __) | | | | | | | |\r\n");
    printf("_\\ \\/ ___/ /_// / __/| |_| | | |_| |\r\n");
    printf("\\__/\\/  /___,' |_____|\\___/|_|\\___/\r\n");
    unity_run_menu();
}
