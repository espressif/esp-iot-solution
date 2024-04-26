/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_lcd_axs15231b.h"

#define TEST_LCD_BIT_PER_PIXEL          (16)

/* I80 */
#define TEST_LCD_I80_H_RES              (170)
#define TEST_LCD_I80_V_RES              (560)
#define TEST_LCD_I80_CLK_HZ             (20 * 1000 * 1000)
#define TEST_PIN_NUM_I80_DATA0          (GPIO_NUM_7)
#define TEST_PIN_NUM_I80_DATA1          (GPIO_NUM_8)
#define TEST_PIN_NUM_I80_DATA2          (GPIO_NUM_9)
#define TEST_PIN_NUM_I80_DATA3          (GPIO_NUM_10)
#define TEST_PIN_NUM_I80_DATA4          (GPIO_NUM_11)
#define TEST_PIN_NUM_I80_DATA5          (GPIO_NUM_12)
#define TEST_PIN_NUM_I80_DATA6          (GPIO_NUM_13)
#define TEST_PIN_NUM_I80_DATA7          (GPIO_NUM_14)
#define TEST_PIN_NUM_I80_PCLK           (GPIO_NUM_3)
#define TEST_PIN_NUM_I80_CS             (GPIO_NUM_44)
#define TEST_PIN_NUM_I80_DC             (GPIO_NUM_4)
#define TEST_PIN_NUM_I80_RD             (GPIO_NUM_2)
#define TEST_PIN_NUM_I80_RST            (GPIO_NUM_5)
#define TEST_PIN_NUM_I80_BL             (GPIO_NUM_38)

/* SPI & QSPI */
#define TEST_LCD_SPI_H_RES              (320)
#define TEST_LCD_SPI_V_RES              (480)
#define TEST_LCD_SPI_HOST               (SPI2_HOST)
#define TEST_PIN_NUM_SPI_CS             (GPIO_NUM_45)
#define TEST_PIN_NUM_SPI_PCLK           (GPIO_NUM_47)
#define TEST_PIN_NUM_SPI_DATA0          (GPIO_NUM_21)
#define TEST_PIN_NUM_SPI_DATA1          (GPIO_NUM_48)
#define TEST_PIN_NUM_SPI_DATA2          (GPIO_NUM_40)
#define TEST_PIN_NUM_SPI_DATA3          (GPIO_NUM_39)
#define TEST_PIN_NUM_SPI_RST            (GPIO_NUM_NC)
#define TEST_PIN_NUM_SPI_DC             (GPIO_NUM_8)
#define TEST_PIN_NUM_SPI_BL             (GPIO_NUM_1)

/* Touch */
#define TEST_I2C_MASTER_NUM             (I2C_NUM_0)
#define TEST_I2C_CLK_SPEED              (400000)

#define TEST_PIN_NUM_TOUCH_SCL          (GPIO_NUM_18)
#define TEST_PIN_NUM_TOUCH_SDA          (GPIO_NUM_17)
#define TEST_PIN_NUM_TOUCH_RST          (GPIO_NUM_5)
#define TEST_PIN_NUM_TOUCH_INT          (GPIO_NUM_21)

#define TEST_DELAY_TIME_MS              (3000)
#define TEST_READ_TIME_MS               (3000)
#define TEST_READ_PERIOD_MS             (30)

static char *TAG = "axs15231b_test";

static SemaphoreHandle_t refresh_finish = NULL;
static SemaphoreHandle_t touch_mux = NULL;

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
    {0xA0, (uint8_t []){0xC0, 0x10, 0x00, 0x02, 0x00, 0x00, 0x04, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 17, 0},
    {0xA2, (uint8_t []){0x30, 0x3C, 0x24, 0x14, 0xD0, 0x20, 0xFF, 0xE0, 0x40, 0x19, 0x80, 0x80, 0x80, 0x20, 0xf9, 0x10, 0x02, 0xff, 0xff, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xE0, 0x20, 0x7F, 0xFF, 0x00, 0x5A}, 31, 0},
    {0xD0, (uint8_t []){0xE0, 0x40, 0x51, 0x24, 0x08, 0x05, 0x10, 0x01, 0x20, 0x15, 0x42, 0xC2, 0x22, 0x22, 0xAA, 0x03, 0x10, 0x12, 0x60, 0x14, 0x1E, 0x51, 0x15, 0x00, 0x8A, 0x20, 0x00, 0x03, 0x3A, 0x12}, 30, 0},
    {0xA3, (uint8_t []){0xA0, 0x06, 0xAa, 0x00, 0x08, 0x02, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 22, 0},
    {0xC1, (uint8_t []){0x31, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x00, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0d, 0x00, 0xFF, 0x40}, 30, 0},
    {0xC3, (uint8_t []){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
    {0xC4, (uint8_t []){0x00, 0x24, 0x33, 0x80, 0x00, 0xea, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
    {0xC5, (uint8_t []){0x18, 0x00, 0x00, 0x03, 0xFE, 0x3A, 0x4A, 0x20, 0x30, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x3A, 0x4A, 0x20, 0x10, 0x10, 0x00}, 23, 0},
    {0xC6, (uint8_t []){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x03, 0x00, 0x3F, 0x6A, 0x18, 0xC8, 0x22}, 20, 0},
    {0xC7, (uint8_t []){0x50, 0x32, 0x28, 0x00, 0xa2, 0x80, 0x8f, 0x00, 0x80, 0xff, 0x07, 0x11, 0x9c, 0x67, 0xff, 0x24, 0x0c, 0x0d, 0x0e, 0x0f}, 20, 0},
    {0xC9, (uint8_t []){0x33, 0x44, 0x44, 0x01}, 4, 0},
    {0xCF, (uint8_t []){0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x68, 0x88, 0x00, 0x65, 0x09, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x08, 0x08, 0x12, 0xA0, 0x08}, 27, 0},
    {0xD5, (uint8_t []){0x40, 0x8E, 0x8D, 0x01, 0x35, 0x04, 0x92, 0x74, 0x04, 0x92, 0x74, 0x04, 0x08, 0x6A, 0x04, 0x46, 0x03, 0x03, 0x03, 0x03, 0x82, 0x01, 0x03, 0x00, 0xE0, 0x51, 0xA1, 0x00, 0x00, 0x00}, 30, 0},
    {0xD6, (uint8_t []){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x93, 0x00, 0x01, 0x83, 0x07, 0x07, 0x00, 0x07, 0x07, 0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x84, 0x00, 0x20, 0x01, 0x00}, 30, 0},
    {0xD7, (uint8_t []){0x03, 0x01, 0x0b, 0x09, 0x0f, 0x0d, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19, 0x40, 0x8E, 0x04, 0x00, 0x20, 0xA0, 0x1F}, 19, 0},
    {0xD8, (uint8_t []){0x02, 0x00, 0x0a, 0x08, 0x0e, 0x0c, 0x1E, 0x1F, 0x18, 0x1d, 0x1f, 0x19}, 12, 0},
    {0xD9, (uint8_t []){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDD, (uint8_t []){0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDF, (uint8_t []){0x44, 0x73, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8,  0},
    {0xE0, (uint8_t []){0x3B, 0x28, 0x10, 0x16, 0x0c, 0x06, 0x11, 0x28, 0x5c, 0x21, 0x0D, 0x35, 0x13, 0x2C, 0x33, 0x28, 0x0D}, 17, 0},
    {0xE1, (uint8_t []){0x37, 0x28, 0x10, 0x16, 0x0b, 0x06, 0x11, 0x28, 0x5C, 0x21, 0x0D, 0x35, 0x14, 0x2C, 0x33, 0x28, 0x0F}, 17, 0},
    {0xE2, (uint8_t []){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE3, (uint8_t []){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x35, 0x44, 0x32, 0x0C, 0x14, 0x14, 0x36, 0x32, 0x2F, 0x0F}, 17, 0},
    {0xE4, (uint8_t []){0x3B, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0D}, 17, 0},
    {0xE5, (uint8_t []){0x37, 0x07, 0x12, 0x18, 0x0E, 0x0D, 0x17, 0x39, 0x44, 0x2E, 0x0C, 0x14, 0x14, 0x36, 0x3A, 0x2F, 0x0F}, 17, 0},
    {0xA4, (uint8_t []){0x85, 0x85, 0x95, 0x82, 0xAF, 0xAA, 0xAA, 0x80, 0x10, 0x30, 0x40, 0x40, 0x20, 0xFF, 0x60, 0x30}, 16, 0},
    {0xA4, (uint8_t []){0x85, 0x85, 0x95, 0x85}, 4, 0},
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {0x13, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x2C, (uint8_t []){0x00, 0x00, 0x00, 0x00}, 4, 0},
};

static void test_i2c_init(void)
{
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TEST_PIN_NUM_TOUCH_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = TEST_PIN_NUM_TOUCH_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = TEST_I2C_CLK_SPEED
    };
    TEST_ESP_OK(i2c_param_config(TEST_I2C_MASTER_NUM, &i2c_conf));
    TEST_ESP_OK(i2c_driver_install(TEST_I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0));
}

static void test_touch_panel_crate(esp_lcd_panel_io_handle_t *io_handle_p, esp_lcd_touch_handle_t *tp_handle_p, esp_lcd_touch_interrupt_callback_t callback)
{
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_AXS15231B_CONFIG();
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = TEST_LCD_I80_H_RES,
        .y_max = TEST_LCD_I80_V_RES,
        .rst_gpio_num = TEST_PIN_NUM_TOUCH_RST,
        .int_gpio_num = TEST_PIN_NUM_TOUCH_INT,
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
    TEST_ESP_OK(esp_lcd_touch_new_i2c_axs15231b(*io_handle_p, &tp_cfg, tp_handle_p));
}

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

IRAM_ATTR static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle, uint32_t h_res, uint32_t v_res)
{
    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);

    uint16_t row_line = ((v_res / TEST_LCD_BIT_PER_PIXEL) << 1) >> 1;
    uint8_t byte_per_pixel = TEST_LCD_BIT_PER_PIXEL / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < TEST_LCD_BIT_PER_PIXEL; j++) {
        for (int i = 0; i < row_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), TEST_LCD_BIT_PER_PIXEL) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }
    free(color);
    vSemaphoreDelete(refresh_finish);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));
}

TEST_CASE("test axs15231b to draw color bar with I80 interface", "[axs15231b][I80]")
{
    gpio_config_t init_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TEST_PIN_NUM_I80_RD) | (1ULL << TEST_PIN_NUM_I80_PCLK) | (1ULL << TEST_PIN_NUM_I80_BL),
    };
    ESP_ERROR_CHECK(gpio_config(&init_gpio_config));
    gpio_set_level(TEST_PIN_NUM_I80_RD, 1);
    gpio_set_level(TEST_PIN_NUM_I80_PCLK, 0);
    gpio_set_level(TEST_PIN_NUM_I80_BL, 1);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = AXS15231B_PANEL_BUS_I80_CONFIG(
                                              TEST_PIN_NUM_I80_DC,
                                              TEST_PIN_NUM_I80_PCLK,
                                              LCD_CLK_SRC_DEFAULT,
                                              TEST_PIN_NUM_I80_DATA0,
                                              TEST_PIN_NUM_I80_DATA1,
                                              TEST_PIN_NUM_I80_DATA2,
                                              TEST_PIN_NUM_I80_DATA3,
                                              TEST_PIN_NUM_I80_DATA4,
                                              TEST_PIN_NUM_I80_DATA5,
                                              TEST_PIN_NUM_I80_DATA6,
                                              TEST_PIN_NUM_I80_DATA7,
                                              8,
                                              TEST_LCD_I80_H_RES * TEST_LCD_I80_V_RES * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = AXS15231B_PANEL_IO_I80_CONFIG(TEST_PIN_NUM_I80_CS,
                                                                            TEST_LCD_I80_CLK_HZ,
                                                                            test_notify_refresh_ready,
                                                                            NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install LCD driver of axs15231b");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_I80_RST,
        .flags.reset_active_high = 0,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_axs15231b(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);

    test_draw_bitmap(panel_handle, TEST_LCD_I80_H_RES, TEST_LCD_I80_V_RES);

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(esp_lcd_del_i80_bus(i80_bus));
}

TEST_CASE("test axs15231b to draw color bar with SPI interface", "[axs15231b][spi]")
{
    ESP_LOGI(TAG, "Initialize BL");
    gpio_config_t init_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TEST_PIN_NUM_SPI_BL),
    };
    ESP_ERROR_CHECK(gpio_config(&init_gpio_config));
    gpio_set_level(TEST_PIN_NUM_SPI_BL, 1);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_SPI_CONFIG(TEST_PIN_NUM_SPI_PCLK,
                                                                   TEST_PIN_NUM_SPI_DATA0,
                                                                   TEST_LCD_SPI_H_RES * TEST_LCD_SPI_V_RES * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = AXS15231B_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_SPI_CS,
                                                                                  TEST_PIN_NUM_SPI_DC,
                                                                                  test_notify_refresh_ready,
                                                                                  NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_SPI_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install AXS15231B panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_SPI_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_axs15231b(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    test_draw_bitmap(panel_handle, TEST_LCD_SPI_H_RES, TEST_LCD_SPI_V_RES);

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_SPI_HOST));
}

TEST_CASE("test axs15231b to draw color bar with QSPI interface", "[axs15231b][qspi]")
{
    ESP_LOGI(TAG, "Initialize BL");
    gpio_config_t init_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TEST_PIN_NUM_SPI_BL),
    };
    ESP_ERROR_CHECK(gpio_config(&init_gpio_config));
    gpio_set_level(TEST_PIN_NUM_SPI_BL, 1);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = AXS15231B_PANEL_BUS_QSPI_CONFIG(TEST_PIN_NUM_SPI_PCLK,
                                                                    TEST_PIN_NUM_SPI_DATA0,
                                                                    TEST_PIN_NUM_SPI_DATA1,
                                                                    TEST_PIN_NUM_SPI_DATA2,
                                                                    TEST_PIN_NUM_SPI_DATA3,
                                                                    TEST_LCD_SPI_H_RES * TEST_LCD_SPI_V_RES * TEST_LCD_BIT_PER_PIXEL / 8);
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = AXS15231B_PANEL_IO_QSPI_CONFIG(TEST_PIN_NUM_SPI_CS, test_notify_refresh_ready, NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_SPI_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install AXS15231B panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const axs15231b_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_SPI_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };
    TEST_ESP_OK(esp_lcd_new_panel_axs15231b(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    test_draw_bitmap(panel_handle, TEST_LCD_SPI_H_RES, TEST_LCD_SPI_V_RES);

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_SPI_HOST));
}

TEST_CASE("test axs15231b to read touch point with interruption", "[axs15231b][interrupt]")
{
    // Todo: Need to initialize LCD panel with specific initialization sequence, will be done in the future

    test_i2c_init();

    touch_mux = xSemaphoreCreateBinary();
    TEST_ASSERT(touch_mux != NULL);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_crate(&tp_io_handle, &tp_handle, touch_callback);

    bool tp_pressed = false;
    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
            TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle)); // read only when ISR was triggled
        }
        /* Read data from touch controller */
        tp_pressed = esp_lcd_touch_get_coordinates(tp_handle, &tp_x, &tp_y, NULL, &tp_cnt, 1);
        if (tp_pressed && (tp_cnt > 0)) {
            ESP_LOGI(TAG, "Touch position: %d,%d", tp_x, tp_y);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    i2c_driver_delete(TEST_I2C_MASTER_NUM);
    esp_lcd_touch_del(tp_handle);
    esp_lcd_panel_io_del(tp_io_handle);
    vSemaphoreDelete(touch_mux);
    gpio_uninstall_isr_service();
}

TEST_CASE("test axs15231b to read touch point with polling", "[axs15231b][poll]")
{
    // Todo: Need to initialize LCD panel with specific initialization sequence, will be done in the future

    test_i2c_init();

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    test_touch_panel_crate(&tp_io_handle, &tp_handle, NULL);

    bool tp_pressed = false;
    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;
    for (int i = 0; i < TEST_READ_TIME_MS / TEST_READ_PERIOD_MS; i++) {
        /* Read data from touch controller into memory */
        TEST_ESP_OK(esp_lcd_touch_read_data(tp_handle)); // read only when ISR was triggled
        /* Read data from touch controller */
        tp_pressed = esp_lcd_touch_get_coordinates(tp_handle, &tp_x, &tp_y, NULL, &tp_cnt, 1);
        if (tp_pressed && (tp_cnt > 0)) {
            ESP_LOGI(TAG, "Touch position: %d,%d", tp_x, tp_y);
        }
        vTaskDelay(pdMS_TO_TICKS(TEST_READ_PERIOD_MS));
    }

    i2c_driver_delete(TEST_I2C_MASTER_NUM);
    esp_lcd_touch_del(tp_handle);
    esp_lcd_panel_io_del(tp_io_handle);
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
    printf("esp_lcd_axs15231b TEST \n");
    unity_run_menu();
}
