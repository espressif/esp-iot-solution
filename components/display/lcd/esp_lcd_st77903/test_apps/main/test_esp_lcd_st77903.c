/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_st77903.h"

#define TEST_LCD_HOST                   SPI2_HOST
#define TEST_LCD_QSPI_H_RES             (400)
#define TEST_LCD_QSPI_V_RES             (400)
#define TEST_LCD_QSPI_BIT_PER_PIXEL     (16)

#define TEST_PIN_NUM_LCD_QSPI_CS        (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_QSPI_PCLK      (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_QSPI_DATA0     (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_QSPI_DATA1     (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_QSPI_DATA2     (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_QSPI_DATA3     (GPIO_NUM_14)

#if SOC_LCD_RGB_SUPPORTED
#define TEST_LCD_RGB_H_RES              (320)
#define TEST_LCD_RGB_V_RES              (480)
#define TEST_LCD_RGB_DATA_WIDTH         (8)
#define TEST_LCD_BIT_PER_PIXEL          (24)
#define TEST_RGB_BIT_PER_PIXEL          (24)

#define TEST_PIN_NUM_LCD_RGB_VSYNC      (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_RGB_HSYNC      (GPIO_NUM_46)
#define TEST_PIN_NUM_LCD_RGB_DE         (GPIO_NUM_17)
#define TEST_PIN_NUM_LCD_RGB_DISP       (GPIO_NUM_NC)
#define TEST_PIN_NUM_LCD_RGB_PCLK       (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_RGB_DATA0      (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_RGB_DATA1      (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_RGB_DATA2      (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_RGB_DATA3      (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_RGB_DATA4      (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_RGB_DATA5      (GPIO_NUM_21)
#define TEST_PIN_NUM_LCD_RGB_DATA6      (GPIO_NUM_47)
#define TEST_PIN_NUM_LCD_RGB_DATA7      (GPIO_NUM_48)

#define TEST_PIN_NUM_LCD_SPI_CS         (GPIO_NUM_45)
#define TEST_PIN_NUM_LCD_SPI_SCK        (GPIO_NUM_40)
#define TEST_PIN_NUM_LCD_SPI_SDO        (GPIO_NUM_42)
#endif

#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_2)

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "st77903_test";

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle, uint16_t h_res, uint16_t v_res,
                             uint8_t bits_per_pixel, bool swap_byte)
{
    uint16_t row_line = v_res / bits_per_pixel;
    uint8_t byte_per_pixel = bits_per_pixel / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);
    for (int j = 0; j < bits_per_pixel; j++) {
        for (int i = 0; i < row_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                if (swap_byte) {
                    color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), bits_per_pixel) >> (k * 8)) & 0xff;
                } else {
                    color[i * byte_per_pixel + k] = (BIT(j) >> (k * 8)) & 0xff;
                }
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
    }
    free(color);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
}

TEST_CASE("test st77903 to draw color bar with QSPI interface", "[st77903][qspi]")
{
    ESP_LOGI(TAG, "Install st77903 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    st77903_qspi_config_t qspi_config = ST77903_QSPI_CONFIG_DEFAULT(TEST_LCD_HOST,
                                                                    TEST_PIN_NUM_LCD_QSPI_CS,
                                                                    TEST_PIN_NUM_LCD_QSPI_PCLK,
                                                                    TEST_PIN_NUM_LCD_QSPI_DATA0,
                                                                    TEST_PIN_NUM_LCD_QSPI_DATA1,
                                                                    TEST_PIN_NUM_LCD_QSPI_DATA2,
                                                                    TEST_PIN_NUM_LCD_QSPI_DATA3,
                                                                    1,
                                                                    TEST_LCD_QSPI_H_RES,
                                                                    TEST_LCD_QSPI_V_RES);
#if CONFIG_IDF_TARGET_ESP32C6
    qspi_config.flags.fb_in_psram = 0;
    qspi_config.trans_pool_num = 2;
#endif
    st77903_vendor_config_t vendor_config = {
        .qspi_config = &qspi_config,
        .flags = {
            .use_qspi_interface = 1,
            .mirror_by_cmd = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = TEST_LCD_QSPI_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77903(NULL, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, true, false));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));

    test_draw_bitmap(panel_handle, TEST_LCD_QSPI_H_RES, TEST_LCD_QSPI_V_RES, TEST_LCD_QSPI_BIT_PER_PIXEL, true);

    // Stop refresh task before send the command, then set display off
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, false));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, true, true));
    // Set display on and start refresh task
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
}

#if SOC_LCD_RGB_SUPPORTED
TEST_CASE("test st77903 to draw color bar with RGB interface", "[st77903][rgb]")
{
    ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
    spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_expander_pin = TEST_PIN_NUM_LCD_SPI_CS,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_expander_pin = TEST_PIN_NUM_LCD_SPI_SCK,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_expander_pin = TEST_PIN_NUM_LCD_SPI_SDO,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST77903_RGB_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install st77903 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = TEST_LCD_RGB_DATA_WIDTH,
        .bits_per_pixel = TEST_RGB_BIT_PER_PIXEL,
        .de_gpio_num = TEST_PIN_NUM_LCD_RGB_DE,
        .pclk_gpio_num = TEST_PIN_NUM_LCD_RGB_PCLK,
        .vsync_gpio_num = TEST_PIN_NUM_LCD_RGB_VSYNC,
        .hsync_gpio_num = TEST_PIN_NUM_LCD_RGB_HSYNC,
        .disp_gpio_num = TEST_PIN_NUM_LCD_RGB_DISP,
        .data_gpio_nums = {
            TEST_PIN_NUM_LCD_RGB_DATA0,
            TEST_PIN_NUM_LCD_RGB_DATA1,
            TEST_PIN_NUM_LCD_RGB_DATA2,
            TEST_PIN_NUM_LCD_RGB_DATA3,
            TEST_PIN_NUM_LCD_RGB_DATA4,
            TEST_PIN_NUM_LCD_RGB_DATA5,
            TEST_PIN_NUM_LCD_RGB_DATA6,
            TEST_PIN_NUM_LCD_RGB_DATA7,
        },
        .timings = ST77903_RGB_320_480_PANEL_48HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    st77903_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            .use_rgb_interface = 1,
            .auto_del_panel_io = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77903(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));

    test_draw_bitmap(panel_handle, TEST_LCD_RGB_H_RES, TEST_LCD_RGB_V_RES, TEST_LCD_BIT_PER_PIXEL, false);

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
}
#endif

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
     *   __  _____ _____ _____ ___   ___ _____
     *  / _\/__   \___  |___  / _ \ / _ \___ /
     *  \ \   / /\/  / /   / / (_) | | | ||_ \
     *  _\ \ / /    / /   / / \__, | |_| |__) |
     *  \__/ \/    /_/   /_/    /_/ \___/____/
     */
    printf(" __  _____ _____ _____ ___   ___ _____\r\n");
    printf("/ _\\/__   \\___  |___  / _ \\ / _ \\___ /\r\n");
    printf("\\ \\   / /\\/  / /   / / (_) | | | ||_ \\\r\n");
    printf("_\\ \\ / /    / /   / / \\__, | |_| |__) |\r\n");
    printf("\\__/ \\/    /_/   /_/    /_/ \\___/____/\r\n");
    unity_run_menu();
}
