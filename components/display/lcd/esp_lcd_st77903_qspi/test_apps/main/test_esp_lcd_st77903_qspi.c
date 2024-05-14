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
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_lcd_st77903_qspi.h"

#define TEST_LCD_HOST                   SPI2_HOST
#define TEST_LCD_H_RES                  (400)
#define TEST_LCD_V_RES                  (400)
#define TEST_LCD_BIT_PER_PIXEL          (16)
#define TEST_LCD_READ_ENABLE            (0)

#define TEST_PIN_NUM_LCD_QSPI_CS        (GPIO_NUM_12)
#define TEST_PIN_NUM_LCD_QSPI_PCLK      (GPIO_NUM_10)
#define TEST_PIN_NUM_LCD_QSPI_DATA0     (GPIO_NUM_13)
#define TEST_PIN_NUM_LCD_QSPI_DATA1     (GPIO_NUM_11)
#define TEST_PIN_NUM_LCD_QSPI_DATA2     (GPIO_NUM_14)
#define TEST_PIN_NUM_LCD_QSPI_DATA3     (GPIO_NUM_9)
#define TEST_PIN_NUM_LCD_RST            (GPIO_NUM_47)
#define TEST_PIN_NUM_BK_LIGHT           (-1)    // set to -1 if not used
#define TEST_LCD_BK_LIGHT_ON_LEVEL      (1)
#define TEST_LCD_BK_LIGHT_OFF_LEVEL     !TEST_LCD_BK_LIGHT_ON_LEVEL

#define TEST_LCD_REG_RDDST              (0x09)
#define TEST_LCD_REG_VALUE              (0x00265284ULL)
#define TEST_DELAY_TIME_MS              (3000)

static char *TAG = "st77903_qspi_test";

static esp_lcd_panel_handle_t test_init_lcd(bool enable_read)
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
                                                                    TEST_LCD_H_RES,
                                                                    TEST_LCD_V_RES);
    if (enable_read) {
        qspi_config.flags.enable_read_reg = 1;
    }
#if CONFIG_IDF_TARGET_ESP32C6
    qspi_config.flags.fb_in_psram = 0;
    qspi_config.trans_pool_num = 2;
#endif
    st77903_vendor_config_t vendor_config = {
        .qspi_config = &qspi_config,
        .flags = {
            .mirror_by_cmd = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_st77903_qspi(&panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));

    return panel_handle;
}

static void test_deinit_lcd(esp_lcd_panel_handle_t panel_handle)
{
    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
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
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), TEST_LCD_BIT_PER_PIXEL) >> (k * 8)) & 0xff;
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

TEST_CASE("test st77903_qspi to draw color bar", "[st77903_qspi][draw_color_bar]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(false);

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(panel_handle);
}

#if TEST_LCD_READ_ENABLE
TEST_CASE("test st77903_qspi to read register", "[st77903_qspi][read_reg]")
{
    ESP_LOGI(TAG, "Initialize LCD device");
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(true);

    ESP_LOGI(TAG, "Show color bar drawn by software");
    test_draw_color_bar(panel_handle, TEST_LCD_H_RES, TEST_LCD_V_RES);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    ESP_LOGI(TAG, "Read register 0x%02x from LCD device", TEST_LCD_REG_RDDST);
    int reg_value = 0;
    TEST_ESP_OK(esp_lcd_st77903_qspi_read_reg(panel_handle, TEST_LCD_REG_RDDST, (uint8_t *)&reg_value, sizeof(reg_value), portMAX_DELAY));
    ESP_LOGI(TAG, "[0x%02x]: 0x%08x", TEST_LCD_REG_RDDST, reg_value);
    TEST_ASSERT_TRUE(reg_value == TEST_LCD_REG_VALUE);

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(panel_handle);
}
#endif

TEST_CASE("test st77903_qspi to rotate", "[st77903_qspi][rotate]")
{
    uint16_t w = 0;
    uint16_t h = 0;
    int64_t t = 0;

    ESP_LOGI(TAG, "Initialize LCD device");
    esp_lcd_panel_handle_t panel_handle = test_init_lcd(false);

    ESP_LOGI(TAG, "Rotate the screen");
    for (size_t i = 0; i < 8; i++) {
        if (i & 4) {
            w = TEST_LCD_V_RES;
            h = TEST_LCD_H_RES;
        } else {
            w = TEST_LCD_H_RES;
            h = TEST_LCD_V_RES;
        }

        TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, false));
        TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, i & 2, i & 1));
        TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));

        TEST_ESP_OK(esp_lcd_panel_swap_xy(panel_handle, i & 4));

        ESP_LOGI(TAG, "Rotation: %d", i);
        t = esp_timer_get_time();
        test_draw_color_bar(panel_handle, w, h);
        t = esp_timer_get_time() - t;
        ESP_LOGI(TAG, "@resolution %dx%d time per frame=%.2fMS\r\n", w, h, (float)t / 1000.0f);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Deinitialize LCD device");
    test_deinit_lcd(panel_handle);
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
     *  __  _____ _____ _____ ___   ___ _____     ____  __    ___ _____
     * / _\/__   \___  |___  / _ \ / _ \___ /    /___ \/ _\  / _ \\_   \
     * \ \   / /\/  / /   / / (_) | | | ||_ \   //  / /\ \  / /_)/ / /\/
     * _\ \ / /    / /   / / \__, | |_| |__) | / \_/ / _\ \/ ___/\/ /_
     * \__/ \/    /_/   /_/    /_/ \___/____/  \___,_\ \__/\/   \____/
    */
    printf(" __  _____ _____ _____ ___   ___ _____     ____  __    ___ _____\r\n");
    printf("/ _\\/__   \\___  |___  / _ \\ / _ \\___ /    /___ \\/ _\\  / _ \\\\_   \\\r\n");
    printf("\\ \\   / /\\/  / /   / / (_) | | | ||_ \\   //  / /\\ \\  / /_)/ / /\\/\r\n");
    printf("_\\ \\ / /    / /   / / \\__, | |_| |__) | / \\_/ / _\\ \\/ ___/\\/ /_\r\n");
    printf("\\__/ \\/    /_/   /_/    /_/ \\___/____/  \\___,_\\ \\__/\\/   \\____/\r\n");
    unity_run_menu();
}
