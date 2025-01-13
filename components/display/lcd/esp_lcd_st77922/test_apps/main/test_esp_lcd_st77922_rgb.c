/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "soc/soc_caps.h"

#if SOC_LCD_RGB_SUPPORTED
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io_additions.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_st77922.h"

#define TEST_LCD_HOST                           SPI2_HOST
#define TEST_LCD_H_RES                          (480)
#define TEST_LCD_V_RES                          (480)
#define TEST_LCD_BIT_PER_PIXEL                  (24)
#define TEST_PIN_NUM_BK_LIGHT                   (-1)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL              (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL             !TEST_LCD_BK_LIGHT_ON_LEVEL
#define TEST_PIN_NUM_LCD_RST                    (GPIO_NUM_2)
#define TEST_PIN_NUM_LCD_RGB_VSYNC              (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_RGB_HSYNC              (GPIO_NUM_46)
#define TEST_PIN_NUM_LCD_RGB_DE                 (GPIO_NUM_17)
#define TEST_PIN_NUM_LCD_RGB_DISP               (-1)            // -1 if not used
#define TEST_PIN_NUM_LCD_RGB_PCLK               (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_RGB_DATA0              (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_RGB_DATA1              (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_RGB_DATA2              (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_RGB_DATA3              (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_RGB_DATA4              (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_RGB_DATA5              (GPIO_NUM_21)
#define TEST_PIN_NUM_LCD_RGB_DATA6              (GPIO_NUM_8)
#define TEST_PIN_NUM_LCD_RGB_DATA7              (GPIO_NUM_18)
#define TEST_PIN_NUM_LCD_SPI_CS                 (GPIO_NUM_45)
#define TEST_PIN_NUM_LCD_SPI_SCK                (GPIO_NUM_40)
#define TEST_PIN_NUM_LCD_SPI_SDO                (GPIO_NUM_42)

#define TEST_DELAY_TIME_MS                      (3000)

static char *TAG = "st77922_rgb_test";

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

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

static void test_init_lcd()
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
    esp_lcd_panel_io_3wire_spi_config_t io_config = ST77922_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    TEST_ESP_OK(esp_lcd_new_panel_io_3wire_spi(&io_config, &io_handle));

    ESP_LOGI(TAG, "Install st77922 panel driver");
    esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .psram_trans_align = 64,
        .data_width = 8,
        .bits_per_pixel = 24,
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
        .timings = ST77922_480_480_PANEL_60HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
        .bounce_buffer_size_px = TEST_LCD_H_RES * 10,
    };
    rgb_config.timings.h_res = TEST_LCD_H_RES;
    rgb_config.timings.v_res = TEST_LCD_V_RES;
    st77922_vendor_config_t vendor_config = {
        .rgb_config = &rgb_config,
        .flags = {
            .use_rgb_interface = 1,
            .enable_io_multiplex = 0,      /**
                                            * Set to 1 if panel IO is no longer needed after LCD initialization.
                                            * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                            * Please set it to 1 to release the pins.
                                            */
            .mirror_by_cmd = 0,             // Set to 0 if `enable_io_multiplex` is enabled
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

static void test_deinit_lcd()
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));

    panel_handle = NULL;
    io_handle = NULL;
#if TEST_PIN_NUM_BK_LIGHT >= 0
    TEST_ESP_OK(gpio_reset_pin(TEST_PIN_NUM_BK_LIGHT));
#endif
}

TEST_CASE("test st77922 to draw color bar with RGB interface", "[st77922][rgb]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd();
}

TEST_CASE("test st77922_rgb to rotate", "[st77922_rgb][rotate]")
{
    esp_err_t ret = ESP_OK;

    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd();

    ESP_LOGI(TAG, "Rotate the screen");
    for (size_t i = 0; i < 8; i++) {
        if (ret != ESP_ERR_NOT_SUPPORTED) {
            if (i & 4) {
                w = TEST_LCD_V_RES;
                h = TEST_LCD_H_RES;
            } else {
                w = TEST_LCD_H_RES;
                h = TEST_LCD_V_RES;
            }
        }

        TEST_ASSERT_NOT_EQUAL(esp_lcd_panel_mirror(panel_handle, i & 2, i & 1), ESP_FAIL);
        ret = esp_lcd_panel_swap_xy(panel_handle, i & 4);
        TEST_ASSERT_NOT_EQUAL(ret, ESP_FAIL);

        ESP_LOGI(TAG, "Rotation: %d", i);
        t = esp_timer_get_time();
        test_draw_color_bar(panel_handle, w, h);
        t = esp_timer_get_time() - t;
        ESP_LOGI(TAG, "@resolution %dx%d time per frame=%.2fMS\r\n", w, h, (float)t / 1000.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd();
}
#endif
