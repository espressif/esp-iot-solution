/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_io_expander_tca9554.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_lcd_st7701.h"

#define TEST_LCD_H_RES              (480)
#define TEST_LCD_V_RES              (480)
#define TEST_LCD_BIT_PER_PIXEL      (18)
#define TEST_RGB_BIT_PER_PIXEL      (16)
#define TEST_RGB_DATA_WIDTH         (16)

#define TEST_LCD_IO_RGB_DISP        (GPIO_NUM_NC)
#define TEST_LCD_IO_RGB_VSYNC       (GPIO_NUM_3)
#define TEST_LCD_IO_RGB_HSYNC       (GPIO_NUM_46)
#define TEST_LCD_IO_RGB_DE          (GPIO_NUM_17)
#define TEST_LCD_IO_RGB_PCLK        (GPIO_NUM_9)
#define TEST_LCD_IO_RGB_DATA0       (GPIO_NUM_10)
#define TEST_LCD_IO_RGB_DATA1       (GPIO_NUM_11)
#define TEST_LCD_IO_RGB_DATA2       (GPIO_NUM_12)
#define TEST_LCD_IO_RGB_DATA3       (GPIO_NUM_13)
#define TEST_LCD_IO_RGB_DATA4       (GPIO_NUM_14)
#define TEST_LCD_IO_RGB_DATA5       (GPIO_NUM_21)
#define TEST_LCD_IO_RGB_DATA6       (GPIO_NUM_47)
#define TEST_LCD_IO_RGB_DATA7       (GPIO_NUM_48)
#define TEST_LCD_IO_RGB_DATA8       (GPIO_NUM_45)
#define TEST_LCD_IO_RGB_DATA9       (GPIO_NUM_38)
#define TEST_LCD_IO_RGB_DATA10      (GPIO_NUM_39)
#define TEST_LCD_IO_RGB_DATA11      (GPIO_NUM_40)
#define TEST_LCD_IO_RGB_DATA12      (GPIO_NUM_41)
#define TEST_LCD_IO_RGB_DATA13      (GPIO_NUM_42)
#define TEST_LCD_IO_RGB_DATA14      (GPIO_NUM_2)
#define TEST_LCD_IO_RGB_DATA15      (GPIO_NUM_1)
#define TEST_LCD_IO_SPI_CS_1        (GPIO_NUM_0)
#define TEST_LCD_IO_SPI_CS_2        (IO_EXPANDER_PIN_NUM_1)
#define TEST_LCD_IO_SPI_SCL         (TEST_LCD_IO_RGB_DATA14)
#define TEST_LCD_IO_SPI_SDA         (TEST_LCD_IO_RGB_DATA15)
#define TEST_LCD_IO_RST             (GPIO_NUM_NC)

#define TEST_EXPANDER_I2C_HOST      (0)
#define TEST_EXPANDER_I2C_ADDR      (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)
#define TEST_EXPANDER_IO_I2C_SCL    (GPIO_NUM_18)
#define TEST_EXPANDER_IO_I2C_SDA    (GPIO_NUM_8)

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "st7701_test";

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    uint16_t row_line = TEST_LCD_V_RES / TEST_RGB_BIT_PER_PIXEL;
    uint8_t byte_per_pixel = TEST_RGB_BIT_PER_PIXEL / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * TEST_LCD_H_RES * byte_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < TEST_RGB_BIT_PER_PIXEL; j++) {
        for (int i = 0; i < row_line * TEST_LCD_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (BIT(j) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, TEST_LCD_H_RES, (j + 1) * row_line, color));
    }
    free(color);
}

TEST_CASE("test st7701 to draw color bar with RGB interface", "[st7701][rgb]")
{
    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = TEST_LCD_IO_SPI_CS_1,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = TEST_LCD_IO_SPI_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = TEST_LCD_IO_SPI_SDA,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST7701 panel driver");
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = TEST_RGB_DATA_WIDTH,
        .bits_per_pixel = TEST_RGB_BIT_PER_PIXEL,
        .de_gpio_num = TEST_LCD_IO_RGB_DE,
        .pclk_gpio_num = TEST_LCD_IO_RGB_PCLK,
        .vsync_gpio_num = TEST_LCD_IO_RGB_VSYNC,
        .hsync_gpio_num = TEST_LCD_IO_RGB_HSYNC,
        .disp_gpio_num = TEST_LCD_IO_RGB_DISP,
        .data_gpio_nums = {
            TEST_LCD_IO_RGB_DATA0,
            TEST_LCD_IO_RGB_DATA1,
            TEST_LCD_IO_RGB_DATA2,
            TEST_LCD_IO_RGB_DATA3,
            TEST_LCD_IO_RGB_DATA4,
            TEST_LCD_IO_RGB_DATA5,
            TEST_LCD_IO_RGB_DATA6,
            TEST_LCD_IO_RGB_DATA7,
            TEST_LCD_IO_RGB_DATA8,
            TEST_LCD_IO_RGB_DATA9,
            TEST_LCD_IO_RGB_DATA10,
            TEST_LCD_IO_RGB_DATA11,
            TEST_LCD_IO_RGB_DATA12,
            TEST_LCD_IO_RGB_DATA13,
            TEST_LCD_IO_RGB_DATA14,
            TEST_LCD_IO_RGB_DATA15,
        },
        .timings = ST7701_480_480_PANEL_60HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            .auto_del_panel_io = 0,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_LCD_IO_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    test_draw_bitmap(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
}

TEST_CASE("test st7701 to draw color bar with RGB interface, using IO expander", "[st7701][rgb][expander]")
{
    ESP_LOGI(TAG, "Install I2C");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_EXPANDER_IO_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = TEST_EXPANDER_IO_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 400 * 1000
    };
    TEST_ESP_OK(i2c_param_config(TEST_EXPANDER_I2C_HOST, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_EXPANDER_I2C_HOST, i2c_conf.mode, 0, 0, 0));

    ESP_LOGI(TAG, "Create TCA9554 IO expander");
    esp_io_expander_handle_t expander_handle = NULL;
    TEST_ESP_OK(esp_io_expander_new_i2c_tca9554(TEST_EXPANDER_I2C_HOST, TEST_EXPANDER_I2C_ADDR, &expander_handle));

    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_EXPANDER,
        .cs_expander_pin = TEST_LCD_IO_SPI_CS_2,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = TEST_LCD_IO_SPI_SCL,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = TEST_LCD_IO_SPI_SDA,
        .io_expander = expander_handle,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST7701 panel driver");
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = TEST_RGB_DATA_WIDTH,
        .bits_per_pixel = TEST_RGB_BIT_PER_PIXEL,
        .de_gpio_num = TEST_LCD_IO_RGB_DE,
        .pclk_gpio_num = TEST_LCD_IO_RGB_PCLK,
        .vsync_gpio_num = TEST_LCD_IO_RGB_VSYNC,
        .hsync_gpio_num = TEST_LCD_IO_RGB_HSYNC,
        .disp_gpio_num = TEST_LCD_IO_RGB_DISP,
        .data_gpio_nums = {
            TEST_LCD_IO_RGB_DATA0,
            TEST_LCD_IO_RGB_DATA1,
            TEST_LCD_IO_RGB_DATA2,
            TEST_LCD_IO_RGB_DATA3,
            TEST_LCD_IO_RGB_DATA4,
            TEST_LCD_IO_RGB_DATA5,
            TEST_LCD_IO_RGB_DATA6,
            TEST_LCD_IO_RGB_DATA7,
            TEST_LCD_IO_RGB_DATA8,
            TEST_LCD_IO_RGB_DATA9,
            TEST_LCD_IO_RGB_DATA10,
            TEST_LCD_IO_RGB_DATA11,
            TEST_LCD_IO_RGB_DATA12,
            TEST_LCD_IO_RGB_DATA13,
            TEST_LCD_IO_RGB_DATA14,
            TEST_LCD_IO_RGB_DATA15,
        },
        .timings = ST7701_480_480_PANEL_60HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    st7701_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            .auto_del_panel_io = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_LCD_IO_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    test_draw_bitmap(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(i2c_driver_delete(TEST_EXPANDER_I2C_HOST));
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (300)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    /**
     *  __  _____ _____ _____ ___  _
     * / _\/__   \___  |___  / _ \/ |
     * \ \   / /\/  / /   / / | | | |
     * _\ \ / /    / /   / /| |_| | |
     * \__/ \/    /_/   /_/  \___/|_|
    */
    printf(" __  _____ _____ _____ ___  _ \r\n");
    printf("/ _\\/__   \\___  |___  / _ \\/ |\r\n");
    printf("\\ \\   / /\\/  / /   / / | | | |\r\n");
    printf("_\\ \\ / /    / /   / /| |_| | |\r\n");
    printf("\\__/ \\/    /_/   /_/  \\___/|_|\r\n");
    unity_run_menu();
}
