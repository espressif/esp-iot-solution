/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_system.h"
#include "esp_lcd_ili9341.h"
#include "driver/gpio.h"
#include "hw_init.h"
#include "helpers/lcd_orientation_helper.h"

#if CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITH_PSRAM

static const char *TAG = "hw_lcd_init";

#define HW_PIN_NUM_BK_LIGHT                     (GPIO_NUM_47)
#define HW_LCD_BK_LIGHT_ON_LEVEL                (1)
#define HW_LCD_BASE_SWAP_XY                     (false)
#define HW_LCD_BASE_MIRROR_X                    (true)
#define HW_LCD_BASE_MIRROR_Y                    (true)

#define HW_LCD_SPI_HOST                         (SPI2_HOST)

#define HW_LCD_DATA0                            (GPIO_NUM_6)
#define HW_LCD_PCLK                             (GPIO_NUM_7)
#define HW_LCD_CS                               (GPIO_NUM_5)
#define HW_LCD_DC                               (GPIO_NUM_4)
#define HW_LCD_RST                              (GPIO_NUM_48)

#define HW_LCD_PIXEL_CLOCK_HZ                   (40 * 1000 * 1000)
#define HW_LCD_COLOR_SPACE                      (LCD_RGB_ELEMENT_ORDER_RGB)
#define HW_LCD_BITS_PER_PIXEL                   (16)
#define HW_LCD_CMD_BITS                         (8)
#define HW_LCD_PARAM_BITS                       (8)
#define HW_LCD_MAX_TRANSFER_SZ                  (HW_LCD_H_RES * HW_LCD_V_RES * 2)

static esp_lcd_panel_io_handle_t s_panel_io_handle;
static esp_lcd_panel_handle_t s_panel_handle;
static bool s_spi_bus_initialized;

static const ili9341_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xC8, (uint8_t []){0xFF, 0x93, 0x42}, 3, 0},
    {0xC0, (uint8_t []){0x0E, 0x0E}, 2, 0},
    {0xC5, (uint8_t []){0xD0}, 1, 0},
    {0xC1, (uint8_t []){0x02}, 1, 0},
    {0xB4, (uint8_t []){0x02}, 1, 0},
    {0xE0, (uint8_t []){0x00, 0x03, 0x08, 0x06, 0x13, 0x09, 0x39, 0x39, 0x48, 0x02, 0x0a, 0x08, 0x17, 0x17, 0x0F}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x28, 0x29, 0x01, 0x0d, 0x03, 0x3f, 0x33, 0x52, 0x04, 0x0f, 0x0e, 0x37, 0x38, 0x0F}, 15, 0},
    {0xB1, (uint8_t []){00, 0x1B}, 2, 0},
    {0x36, (uint8_t []){0x08}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xB7, (uint8_t []){0x06}, 1, 0},
    {0x11, (uint8_t []){0}, 0x80, 0},
    {0x29, (uint8_t []){0}, 0x80, 0},
    {0, (uint8_t []){0}, 0xff, 0},
};

esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle, esp_lcd_panel_io_handle_t *io_handle, esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode, esp_lv_adapter_rotation_t rotation)
{
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = HW_LCD_PCLK,
        .mosi_io_num = HW_LCD_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = HW_LCD_MAX_TRANSFER_SZ,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HW_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    s_spi_bus_initialized = true;

    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = HW_LCD_DC,
        .cs_gpio_num = HW_LCD_CS,
        .pclk_hz = HW_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = HW_LCD_CMD_BITS,
        .lcd_param_bits = HW_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)HW_LCD_SPI_HOST, &io_config, &s_panel_io_handle));

    ESP_LOGD(TAG, "Install LCD driver");
    const ili9341_vendor_config_t vendor_config = {
        .init_cmds = vendor_specific_init_default,
        .init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(vendor_specific_init_default[0]),
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = HW_LCD_RST,
        .flags.reset_active_high = true,
        .rgb_ele_order = HW_LCD_COLOR_SPACE,
        .bits_per_pixel = HW_LCD_BITS_PER_PIXEL,
        .vendor_config = (void *) &vendor_config,
    };

    ESP_LOGI(TAG, "Install ili9341 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_panel_io_handle, (const esp_lcd_panel_dev_config_t *)&panel_config, &s_panel_handle));

    esp_lcd_panel_reset(s_panel_handle);
    esp_lcd_panel_init(s_panel_handle);

    bool swap_xy = false;
    bool mirror_x = true;
    bool mirror_y = true;
    lcd_get_orientation_flags(HW_LCD_BASE_SWAP_XY, HW_LCD_BASE_MIRROR_X, HW_LCD_BASE_MIRROR_Y,
                              rotation, &swap_xy, &mirror_x, &mirror_y);
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel_handle, swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, mirror_x, mirror_y));
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    *io_handle = s_panel_io_handle;
    *panel_handle = s_panel_handle;

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << HW_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    ESP_LOGI(TAG, "Turning on LCD backlight");
    gpio_set_level(HW_PIN_NUM_BK_LIGHT, HW_LCD_BK_LIGHT_ON_LEVEL);

    return ESP_OK;
}

esp_err_t hw_lcd_deinit(void)
{
    if (s_panel_handle) {
        esp_lcd_panel_disp_on_off(s_panel_handle, false);
        esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }

    if (s_panel_io_handle) {
        esp_lcd_panel_io_del(s_panel_io_handle);
        s_panel_io_handle = NULL;
    }

    if (s_spi_bus_initialized) {
        spi_bus_free(HW_LCD_SPI_HOST);
        s_spi_bus_initialized = false;
    }

    gpio_set_level(HW_PIN_NUM_BK_LIGHT, !HW_LCD_BK_LIGHT_ON_LEVEL);

    return ESP_OK;
}

int hw_lcd_get_te_gpio(void)
{
    return GPIO_NUM_NC;
}

uint32_t hw_lcd_get_bus_freq_hz(void)
{
    return HW_LCD_PIXEL_CLOCK_HZ;
}

uint8_t hw_lcd_get_bus_data_lines(void)
{
    return 1;
}

uint8_t hw_lcd_get_bits_per_pixel(void)
{
    return HW_LCD_BITS_PER_PIXEL;
}

#endif
