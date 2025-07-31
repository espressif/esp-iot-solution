/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"

#include "esp_lcd_gc9107.h"

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (128)
#define TEST_LCD_V_RES              (115)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_13)
#define TEST_PIN_NUM_BK_LIGHT       (GPIO_NUM_14)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL      (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL     !TEST_LCD_BK_LIGHT_ON_LEVEL

#define TEST_DELAY_TIME_MS          (3000)

static char *TAG = "gc9107_test";
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

    uint16_t row_line = TEST_LCD_V_RES / TEST_LCD_BIT_PER_PIXEL;
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
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    (ledc_set_duty(LEDC_LOW_SPEED_MODE, 1, duty_cycle));
    (ledc_update_duty(LEDC_LOW_SPEED_MODE, 1));

    return ESP_OK;
}

TEST_CASE("test gc9107 to draw color bar with SPI interface", "[gc9107][spi]")
{
#if TEST_PIN_NUM_BK_LIGHT >= 0
    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << TEST_PIN_NUM_BK_LIGHT
    };
    TEST_ESP_OK(gpio_config(&bk_gpio_config));
#endif

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = GC9107_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_LCD_PCLK, TEST_PIN_NUM_LCD_DATA0,
                                                                TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = GC9107_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
                                                                               test_notify_refresh_ready, NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install gc9107 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
#else
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
#endif
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_gc9107(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_invert_color(panel_handle, true));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    TEST_ESP_OK(esp_lcd_panel_disp_off(panel_handle, false));
#else
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
#endif

    // esp_lcd_panel_gc9107_set_dual_screen_display(false);
    test_draw_bitmap(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));

#if TEST_PIN_NUM_BK_LIGHT >= 0
    TEST_ESP_OK(gpio_reset_pin(TEST_PIN_NUM_BK_LIGHT));
#endif
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
     *   ____  ____ ___  _  ___ _____
     *  / ___|/ ___/ _ \/ |/ _ \___  |
     * | |  _| |  | (_) | | | | | / /
     * | |_| | |___\__, | | |_| |/ /
     *  \____|\____| /_/|_|\___//_/
     */
    printf("   ____  ____ ___  _  ___ _____ \r\n");
    printf("  / ___|/ ___/ _ \\/ |/ _ \\___  |\r\n");
    printf(" | |  _| |  | (_) | | | | | / / \r\n");
    printf(" | |_| | |___\\__, | | |_| |/ /  \r\n");
    printf("  \\____|\\____| /_/|_|\\___//_/   \r\n");
    unity_run_menu();
}
