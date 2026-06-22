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
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_lcd_co5300.h"

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (466)
#define TEST_LCD_V_RES              (466)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_DATA1      (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_DATA2      (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_DATA3      (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_17)
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_8)
#define TEST_PIN_NUM_BK_LIGHT       (GPIO_NUM_0)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL  (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL !TEST_LCD_BK_LIGHT_ON_LEVEL

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "co5300_spi_test";
static SemaphoreHandle_t refresh_finish = NULL;

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);

    uint16_t row_line = ((TEST_LCD_V_RES / TEST_LCD_BIT_PER_PIXEL) << 1) >> 1;
    uint8_t byte_per_pixel = TEST_LCD_BIT_PER_PIXEL / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * TEST_LCD_H_RES * byte_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < TEST_LCD_BIT_PER_PIXEL; j++) {
        for (int i = 0; i < row_line * TEST_LCD_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), TEST_LCD_BIT_PER_PIXEL) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, TEST_LCD_H_RES, (j + 1) * row_line, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }
    free(color);
    vSemaphoreDelete(refresh_finish);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
}

static void test_set_backlight(bool on)
{
#if TEST_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TEST_PIN_NUM_BK_LIGHT
    };
    TEST_ESP_OK(gpio_config(&bk_gpio_config));
    TEST_ESP_OK(gpio_set_level(TEST_PIN_NUM_BK_LIGHT, on ? TEST_LCD_BK_LIGHT_ON_LEVEL : TEST_LCD_BK_LIGHT_OFF_LEVEL));
#else
    (void)on;
#endif
}

static esp_lcd_panel_handle_t test_init_lcd(bool use_qspi, esp_lcd_panel_io_handle_t *io_handle)
{
    TEST_ASSERT_NOT_NULL(io_handle);

    ESP_LOGI(TAG, "Turn on LCD backlight");
    test_set_backlight(true);

    ESP_LOGI(TAG, "Initialize %s bus", use_qspi ? "QSPI" : "SPI");
    if (use_qspi) {
        const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(TEST_PIN_NUM_LCD_PCLK,
                                                                     TEST_PIN_NUM_LCD_DATA0,
                                                                     TEST_PIN_NUM_LCD_DATA1,
                                                                     TEST_PIN_NUM_LCD_DATA2,
                                                                     TEST_PIN_NUM_LCD_DATA3,
                                                                     TEST_LCD_H_RES * TEST_LCD_V_RES * TEST_LCD_BIT_PER_PIXEL / 8);
        TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(TEST_PIN_NUM_LCD_CS, test_notify_refresh_ready, NULL);
        TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, io_handle));
    } else {
        const spi_bus_config_t buscfg = CO5300_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_LCD_PCLK,
                                                                    TEST_PIN_NUM_LCD_DATA0,
                                                                    TEST_LCD_H_RES * TEST_LCD_V_RES * TEST_LCD_BIT_PER_PIXEL / 8);
        TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
                                                                                   test_notify_refresh_ready, NULL);
        TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, io_handle));
    }

    ESP_LOGI(TAG, "Install LCD driver of co5300");
    esp_lcd_panel_handle_t panel_handle = NULL;
    co5300_vendor_config_t vendor_config = {0};
    vendor_config.flags.use_qspi_interface = use_qspi ? 1 : 0;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_co5300(*io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return panel_handle;
}

static void test_deinit_lcd(esp_lcd_panel_handle_t panel_handle, esp_lcd_panel_io_handle_t io_handle)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));

#if TEST_PIN_NUM_BK_LIGHT >= 0
    TEST_ESP_OK(gpio_reset_pin(TEST_PIN_NUM_BK_LIGHT));
#endif
}

static void test_rotate_panel(esp_lcd_panel_handle_t panel_handle)
{
    for (size_t i = 0; i < 8; i++) {
        TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, i & 2, i & 1));
        TEST_ESP_OK(esp_lcd_panel_swap_xy(panel_handle, i & 4));
    }
}

TEST_CASE("test co5300 to draw color bar with SPI interface", "[co5300][spi]")
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(false, &io_handle);

    test_draw_bitmap(panel_handle);

    test_deinit_lcd(panel_handle, io_handle);
}

TEST_CASE("test co5300 to draw color bar with QSPI interface", "[co5300][qspi]")
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(true, &io_handle);

    test_draw_bitmap(panel_handle);

    test_deinit_lcd(panel_handle, io_handle);
}

TEST_CASE("test co5300 to rotate with SPI interface", "[co5300][spi-rotate]")
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(false, &io_handle);

    test_rotate_panel(panel_handle);

    test_deinit_lcd(panel_handle, io_handle);
}

TEST_CASE("test co5300 to rotate with QSPI interface", "[co5300][qspi-rotate]")
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(true, &io_handle);

    test_rotate_panel(panel_handle);

    test_deinit_lcd(panel_handle, io_handle);
}
