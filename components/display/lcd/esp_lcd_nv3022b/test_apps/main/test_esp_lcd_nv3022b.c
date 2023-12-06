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
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_nv3022b.h"

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (240)
#define TEST_LCD_V_RES              (284)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_4)
#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_18)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_19)
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_3)
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_5)
#define TEST_PIN_NUM_LCD_BL         (GPIO_NUM_0)

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "nv3022b_test";
static SemaphoreHandle_t refresh_finish = NULL;

static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

IRAM_ATTR static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
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

TEST_CASE("test nv3022b to draw color bar with SPI interface", "[nv3022b][spi]")
{
    ESP_LOGI(TAG, "Turn on the backlight");
    gpio_config_t io_conf = {
        .pin_bit_mask = BIT64(TEST_PIN_NUM_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(TEST_PIN_NUM_LCD_BL, 1);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = NV3022B_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                 TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = NV3022B_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
                                                                                test_notify_refresh_ready, NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install nv3022b panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST, // Shared with Touch reset
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
#else
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
#endif
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_nv3022b(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, false, false));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    TEST_ESP_OK(esp_lcd_panel_disp_off(panel_handle, false));
#else
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
#endif

    test_draw_bitmap(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    gpio_reset_pin(TEST_PIN_NUM_LCD_BL);
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));
}

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (-300)

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
    *      __        _____  ___ ____  ____    ___
    *   /\ \ \/\   /\___ / / _ \___ \|___ \  / __\
    *  /  \/ /\ \ / / |_ \| | | |__) | __) |/__\//
    * / /\  /  \ V / ___) | |_| / __/ / __// \/  \
    * \_\ \/    \_/ |____/ \___/_____|_____\_____/
    *
    */
    printf("      __        _____  ___ ____  ____    ___  \r\n");
    printf("   /\\ \\ \\/\\   /\\___ / / _ \\___ \\|___ \\  / __\\ \r\n");
    printf("  /  \\// /\\ \\ / / |_ \\| | | |__) | __) |/__\\// \r\n");
    printf(" / /\\  /  \\ V / ___) | |_| / __/ / __// \\/  \\ \r\n");
    printf(" \\_\\ \\/    \\_/ |____/ \\___/_____|_____\\_____/ \r\n");
    unity_run_menu();
}
