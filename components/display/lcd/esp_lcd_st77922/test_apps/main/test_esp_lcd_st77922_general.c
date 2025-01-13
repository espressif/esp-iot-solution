/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

typedef enum {
    TEST_LCD_SPI_INTERFACE = 0,
    TEST_LCD_QSPI_INTERFACE,
} lcd_interface_t;

#define TEST_LCD_HOST                   SPI2_HOST
#define TEST_LCD_H_RES                  (532)
#define TEST_LCD_V_RES                  (300)
#define TEST_LCD_BIT_PER_PIXEL          (16)
#define TEST_PIN_NUM_BK_LIGHT           (-1)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL      (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL     !TEST_LCD_BK_LIGHT_ON_LEVEL
#define TEST_PIN_NUM_LCD_RST            (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_CS             (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_PCLK           (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_DATA0          (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_DATA1          (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_DATA2          (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_DATA3          (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_DC             (GPIO_NUM_8)

#define TEST_DELAY_TIME_MS              (3000)

static char *TAG = "st77922_general_test";

static SemaphoreHandle_t refresh_finish = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

static void test_draw_color_bar(esp_lcd_panel_handle_t panel_handle, uint16_t h_res, uint16_t v_res)
{
    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);

    uint8_t byte_per_pixel = (TEST_LCD_BIT_PER_PIXEL + 7) / 8;
    uint16_t row_line = v_res / byte_per_pixel / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);

    for (int j = 0; j < byte_per_pixel * 8; j++) {
        for (int i = 0; i < row_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), TEST_LCD_BIT_PER_PIXEL) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
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
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }

    free(color);
    vSemaphoreDelete(refresh_finish);
}

static void test_init_lcd(lcd_interface_t interface)
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
    if (TEST_LCD_SPI_INTERFACE == interface) {
        ESP_LOGI(TAG, "Initialize SPI bus");
        const spi_bus_config_t buscfg = ST77922_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0, TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
        TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
                                                                                    test_notify_refresh_ready, NULL);
        // Attach the LCD to the SPI bus
        TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, &io_handle));

        ESP_LOGI(TAG, "Install LCD driver of st77922");
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        };
        TEST_ESP_OK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_disp_on_off(panel_handle, true);
    } else if (TEST_LCD_QSPI_INTERFACE == interface) {
        ESP_LOGI(TAG, "Initialize SPI bus");
        const spi_bus_config_t buscfg = ST77922_PANEL_BUS_QSPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                      TEST_PIN_NUM_LCD_DATA1, TEST_PIN_NUM_LCD_DATA2,
                                                                      TEST_PIN_NUM_LCD_DATA3, TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
        TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_QSPI_CONFIG(TEST_PIN_NUM_LCD_CS, test_notify_refresh_ready, NULL);
        // Attach the LCD to the SPI bus
        TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, &io_handle));

        ESP_LOGI(TAG, "Install LCD driver of st77922");
        const st77922_vendor_config_t vendor_config = {
            .flags = {
                .use_qspi_interface = 1,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
            .vendor_config = (void *) &vendor_config,
        };
        TEST_ESP_OK(esp_lcd_new_panel_st77922(io_handle, &panel_config, &panel_handle));
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_disp_on_off(panel_handle, true);
    }
}

static void test_deinit_lcd(lcd_interface_t interface)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));

    panel_handle = NULL;
    io_handle = NULL;
#if TEST_PIN_NUM_BK_LIGHT >= 0
    TEST_ESP_OK(gpio_reset_pin(TEST_PIN_NUM_BK_LIGHT));
#endif
}

TEST_CASE("test st77922 to draw color bar with SPI interface", "[st77922][spi]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd(TEST_LCD_SPI_INTERFACE);

    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(TEST_LCD_SPI_INTERFACE);
}

TEST_CASE("test st77922_spi to rotate", "[st77922_spi][rotate]")
{
    esp_err_t ret = ESP_OK;

    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd(TEST_LCD_SPI_INTERFACE);

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
    test_deinit_lcd(TEST_LCD_SPI_INTERFACE);
}

TEST_CASE("test st77922 to draw color bar with QSPI interface", "[st77922][qspi]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd(TEST_LCD_QSPI_INTERFACE);

    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(TEST_LCD_QSPI_INTERFACE);
}

TEST_CASE("test st77922_qspi to rotate", "[st77922_qspi][rotate]")
{
    esp_err_t ret = ESP_OK;

    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    test_init_lcd(TEST_LCD_QSPI_INTERFACE);

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
    test_deinit_lcd(TEST_LCD_QSPI_INTERFACE);
}
