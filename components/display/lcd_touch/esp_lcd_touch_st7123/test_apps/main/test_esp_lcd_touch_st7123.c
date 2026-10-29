/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "esp_lcd_st77922.h"
#include "esp_lcd_touch_st7123.h"

#define TEST_I2C_MASTER_NUM         (I2C_NUM_0)
#define TEST_I2C_CLK_SPEED          (380 * 1000)
#define TEST_I2C_SCL_GPIO_NUM       (GPIO_NUM_8)
#define TEST_I2C_SDA_GPIO_NUM       (GPIO_NUM_18)

#define TEST_PANEL_X_MAX            (532)
#define TEST_PANEL_Y_MAX            (300)
#define TEST_PANEL_INT_IO_NUM       (GPIO_NUM_1)
#define TEST_PANEL_RST_IO_NUM       (GPIO_NUM_0)

#define TEST_READ_TIME_MS           (3000)
#define TEST_READ_PERIOD_MS         (30)

#define TEST_WITH_LCD               (1)

#if TEST_WITH_LCD
#define TEST_USE_LCD_QSPI           (1)
#define TEST_USE_LCD_SPI            (0)

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (TEST_PANEL_X_MAX)
#define TEST_LCD_V_RES              (TEST_PANEL_Y_MAX)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_11)
#if TEST_USE_LCD_QSPI
#define TEST_PIN_NUM_LCD_DATA1      (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_DATA2      (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_DATA3      (GPIO_NUM_14)
#else
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_8)
#endif
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_3)
#endif

static char *TAG = "esp_lcd_touch_st7123";
static SemaphoreHandle_t touch_mux = NULL;

static void test_i2c_init(void)
{
    ESP_LOGI(TAG, "Initialize I2C start");

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

    ESP_LOGI(TAG, "Initialize I2C finish");
}

static void test_i2c_deinit(void)
{
    TEST_ESP_OK(i2c_driver_delete(TEST_I2C_MASTER_NUM));
}

static void test_touch_panel_init(esp_lcd_panel_io_handle_t *io_handle_p, esp_lcd_touch_handle_t *tp_handle_p, esp_lcd_touch_interrupt_callback_t callback)
{
    ESP_LOGI(TAG, "Initialize touch panel start");

    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = TEST_PANEL_X_MAX,
        .y_max = TEST_PANEL_Y_MAX,
        .rst_gpio_num = -1,
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
    TEST_ESP_OK(esp_lcd_touch_new_i2c_st7123(*io_handle_p, &tp_cfg, tp_handle_p));

    ESP_LOGI(TAG, "Initialize touch panel finish");
}

static void test_touch_panel_deinit(esp_lcd_panel_io_handle_t io_handle, esp_lcd_touch_handle_t tp_handle)
{
    TEST_ESP_OK(esp_lcd_touch_del(tp_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
}

#if TEST_WITH_LCD
static void test_lcd_panel_init(esp_lcd_panel_io_handle_t *io_handle_p, esp_lcd_panel_handle_t *lcd_handle_p)
{
    ESP_LOGI(TAG, "Initialize lcd panel start");

#if TEST_USE_LCD_QSPI
    const spi_bus_config_t buscfg = ST77922_PANEL_BUS_QSPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                  TEST_PIN_NUM_LCD_DATA1, TEST_PIN_NUM_LCD_DATA2,
                                                                  TEST_PIN_NUM_LCD_DATA3, TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
#elif TEST_USE_LCD_SPI
    const spi_bus_config_t buscfg = ST77922_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                 TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
#endif
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

#if TEST_USE_LCD_QSPI
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_QSPI_CONFIG(TEST_PIN_NUM_LCD_CS, NULL, NULL);
#elif TEST_USE_LCD_SPI
    const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
                                                                                NULL, NULL);
#endif
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, io_handle_p));

    const st77922_vendor_config_t vendor_config = {
        .flags = {
#if TEST_USE_LCD_QSPI
            .use_qspi_interface = 1,
#elif TEST_USE_LCD_SPI
            .use_qspi_interface = 0,
#endif
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77922(*io_handle_p, &panel_config, lcd_handle_p));
    esp_lcd_panel_reset(*lcd_handle_p);
    esp_lcd_panel_init(*lcd_handle_p);

    ESP_LOGI(TAG, "Initialize lcd panel finish");
}

static void test_lcd_panel_deinit(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t panel_handle)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));
}
#endif

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

TEST_CASE("test st7123 to read touch point with interruption", "[st7123][interrupt]")
{
    esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
    esp_lcd_panel_handle_t lcd_handle = NULL;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;

    // The RST pin of ST7123 should be pull-up before the LCD controller is initialized
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(TEST_PANEL_RST_IO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TEST_PANEL_RST_IO_NUM, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TEST_PANEL_RST_IO_NUM, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    touch_mux = xSemaphoreCreateBinary();
    TEST_ASSERT(touch_mux != NULL);

    test_i2c_init();
#if TEST_WITH_LCD
    // The ST7123 is a TDDI touch controller, which is need to be initialized after the LCD controller
    test_lcd_panel_init(&lcd_io_handle, &lcd_handle);
#endif
    test_touch_panel_init(&tp_io_handle, &tp_handle, touch_callback);

    esp_lcd_touch_point_data_t tp_data[1] = {0};
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
            TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));
        }

        /* Read data from touch controller */
        tp_cnt = 0;
        ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, 1));
        if (tp_cnt > 0) {
            ESP_LOGI(TAG, "Touch position: %" PRIu16 ",%" PRIu16, tp_data[0].x, tp_data[0].y);
        }

        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

#if TEST_WITH_LCD
    test_lcd_panel_deinit(lcd_io_handle, lcd_handle);
#endif
    test_touch_panel_deinit(tp_io_handle, tp_handle);
    test_i2c_deinit();
    vSemaphoreDelete(touch_mux);
    gpio_uninstall_isr_service();
}

TEST_CASE("test st7123 to read touch point with polling", "[st7123][poll]")
{
    esp_lcd_panel_io_handle_t lcd_io_handle = NULL;
    esp_lcd_panel_handle_t lcd_handle = NULL;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;

    // The RST pin of ST7123 should be pull-up before the LCD controller is initialized
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(TEST_PANEL_RST_IO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TEST_PANEL_RST_IO_NUM, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TEST_PANEL_RST_IO_NUM, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    test_i2c_init();
#if TEST_WITH_LCD
    // The ST7123 is a TDDI touch controller, which is need to be initialized after the LCD controller
    test_lcd_panel_init(&lcd_io_handle, &lcd_handle);
#endif
    test_touch_panel_init(&tp_io_handle, &tp_handle, NULL);

    esp_lcd_touch_point_data_t tp_data[1] = {0};
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle));

        /* Read data from touch controller */
        tp_cnt = 0;
        ESP_ERROR_CHECK(esp_lcd_touch_get_data(tp_handle, tp_data, &tp_cnt, 1));
        if (tp_cnt > 0) {
            ESP_LOGI(TAG, "Touch position: %" PRIu16 ",%" PRIu16, tp_data[0].x, tp_data[0].y);
        }

        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

#if TEST_WITH_LCD
    test_lcd_panel_deinit(lcd_io_handle, lcd_handle);
#endif
    test_touch_panel_deinit(tp_io_handle, tp_handle);
    test_i2c_deinit();
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
     *  __  _____ _____ _ ____  _____
     * / _\/__   \___  / |___ \|___ /
     * \ \   / /\/  / /| | __) | |_ \
     * _\ \ / /    / / | |/ __/ ___) |
     * \__/ \/    /_/  |_|_____|____/
     */
    printf(" __  _____ _____ _ ____  _____\r\n");
    printf("/ _\\/__   \\___  / |___ \\|___ /\r\n");
    printf("\\ \\   / /\\/  / /| | __) | |_ \\\r\n");
    printf("_\\ \\ / /    / / | |/ __/ ___) |\r\n");
    printf("\\__/ \\/    /_/  |_|_____|____/\r\n");
    unity_run_menu();
}
