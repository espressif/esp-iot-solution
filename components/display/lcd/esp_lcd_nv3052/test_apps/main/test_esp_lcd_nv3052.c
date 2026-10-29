/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "esp_timer.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_lcd_nv3052.h"

#define TEST_LCD_H_RES                  (720)
#define TEST_LCD_V_RES                  (1280)
#define TEST_LCD_RGB_DATA_WIDTH         (16)
#define TEST_LCD_BIT_PER_PIXEL          (18)
#define TEST_RGB_BIT_PER_PIXEL          (16)

#define TEST_PIN_NUM_LCD_RGB_VSYNC      (GPIO_NUM_17)
#define TEST_PIN_NUM_LCD_RGB_HSYNC      (GPIO_NUM_16)
#define TEST_PIN_NUM_LCD_RGB_DE         (GPIO_NUM_18)
#define TEST_PIN_NUM_LCD_RGB_DISP       (GPIO_NUM_NC)
#define TEST_PIN_NUM_LCD_RGB_PCLK       (GPIO_NUM_21)
#define TEST_PIN_NUM_LCD_RGB_DATA0      (GPIO_NUM_4)
#define TEST_PIN_NUM_LCD_RGB_DATA1      (GPIO_NUM_5)
#define TEST_PIN_NUM_LCD_RGB_DATA2      (GPIO_NUM_6)
#define TEST_PIN_NUM_LCD_RGB_DATA3      (GPIO_NUM_7)
#define TEST_PIN_NUM_LCD_RGB_DATA4      (GPIO_NUM_15)

#define TEST_PIN_NUM_LCD_RGB_DATA5      (GPIO_NUM_8)
#define TEST_PIN_NUM_LCD_RGB_DATA6      (GPIO_NUM_20)
#define TEST_PIN_NUM_LCD_RGB_DATA7      (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_RGB_DATA8      (GPIO_NUM_46)
#define TEST_PIN_NUM_LCD_RGB_DATA9      (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_RGB_DATA10     (GPIO_NUM_10)

#define TEST_PIN_NUM_LCD_RGB_DATA11     (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_RGB_DATA12     (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_RGB_DATA13     (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_RGB_DATA14     (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_RGB_DATA15     (GPIO_NUM_0)

#define TEST_PIN_NUM_LCD_SPI_CS         (GPIO_NUM_39)
#define TEST_PIN_NUM_LCD_SPI_SCK        (GPIO_NUM_48)
#define TEST_PIN_NUM_LCD_SPI_SDO        (GPIO_NUM_47)

#define TEST_PIN_NUM_LCD_RST            (GPIO_NUM_19)
#define TEST_PIN_NUM_BK_LIGHT           (-1)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL      (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL     !TEST_LCD_BK_LIGHT_ON_LEVEL

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "nv3052_test";

static void test_init_lcd(esp_lcd_panel_handle_t *lcd, esp_lcd_panel_io_handle_t *io)
{
#if TEST_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TEST_PIN_NUM_BK_LIGHT
    };
    TEST_ESP_OK(gpio_config(&bk_gpio_config));
    TEST_ESP_OK(gpio_set_level(TEST_PIN_NUM_BK_LIGHT, TEST_LCD_BK_LIGHT_ON_LEVEL));
#endif

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
    esp_lcd_panel_io_3wire_spi_config_t io_config = NV3052_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install nv3052 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t rgb_config = {
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
            TEST_PIN_NUM_LCD_RGB_DATA8,
            TEST_PIN_NUM_LCD_RGB_DATA9,
            TEST_PIN_NUM_LCD_RGB_DATA10,
            TEST_PIN_NUM_LCD_RGB_DATA11,
            TEST_PIN_NUM_LCD_RGB_DATA12,
            TEST_PIN_NUM_LCD_RGB_DATA13,
            TEST_PIN_NUM_LCD_RGB_DATA14,
            TEST_PIN_NUM_LCD_RGB_DATA15,
        },
        .timings = NV3052_720_1280_PANEL_30HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
    };
    nv3052_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_nv3052(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));

    if (lcd) {
        *lcd = panel_handle;
    }
    if (io) {
        *io = io_handle;
    }
}

static void test_deinit_lcd(esp_lcd_panel_handle_t panel_handle, esp_lcd_panel_io_handle_t io_handle)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));

#if TEST_PIN_NUM_BK_LIGHT >= 0
    TEST_ESP_OK(gpio_reset_pin(TEST_PIN_NUM_BK_LIGHT));
#endif
}

static void test_draw_color_bar(esp_lcd_panel_handle_t panel_handle, uint16_t h_res, uint16_t v_res)
{
    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint16_t row_line = v_res / byte_per_pixel / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);

    for (int j = 0; j < byte_per_pixel * 8; j++) {
        for (int i = 0; i < row_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (BIT(j) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
    }

    uint16_t color_line = row_line * byte_per_pixel * 8;
    uint16_t res_line = v_res - color_line;
    if (res_line) {
        for (int i = 0; i < res_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, color_line, h_res, v_res, color));
    }

    free(color);
}

TEST_CASE("test nv3052 to draw color bar", "[nv3052][draw_color_bar]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    test_init_lcd(&panel_handle, &io_handle);

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(panel_handle, io_handle);
}

TEST_CASE("test nv3052 to rotate", "[nv3052][rotate]")
{
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd(&panel_handle, &io_handle);

    ESP_LOGI(TAG, "Rotate the screen");
    for (size_t i = 0; i < 8; i++) {
        if (i & 4) {
            w = TEST_LCD_V_RES;
            h = TEST_LCD_H_RES;
        } else {
            w = TEST_LCD_H_RES;
            h = TEST_LCD_V_RES;
        }

        TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, i & 2, i & 1));
        TEST_ESP_OK(esp_lcd_panel_swap_xy(panel_handle, i & 4));

        ESP_LOGI(TAG, "Rotation: %d", i);
        t = esp_timer_get_time();
        test_draw_color_bar(panel_handle, w, h);
        t = esp_timer_get_time() - t;
        ESP_LOGI(TAG, "@resolution %dx%d time per frame=%.2fMS\r\n", w, h, (float)t / 1000.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(panel_handle, io_handle);
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
     *  _   ___     _______  ___  ____ ____
     * | \ | \ \   / /___ / / _ \| ___|___ \
     * |  \| |\ \ / /  |_ \| | | |___ \ __) |
     * | |\  | \ V /  ___) | |_| |___) / __/
     * |_| \_|  \_/  |____/ \___/|____/_____|
    */
    printf(" _   ___     _______  ___  ____ ____  \r\n");
    printf("| \\ | \\ \\   / /___ / / _ \\| ___|___ \\ \r\n");
    printf("|  \\| |\\ \\ / /  |_ \\| | | |___ \\ __) |\r\n");
    printf("| |\\  | \\ V /  ___) | |_| |___) / __/ \r\n");
    printf("|_| \\_|  \\_/  |____/ \\___/|____/_____|\r\n");
    unity_run_menu();
}
