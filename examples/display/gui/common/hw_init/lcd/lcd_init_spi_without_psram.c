/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "esp_system.h"
#include "esp_lcd_gc9a01.h"
#include "hw_init.h"
#include "helpers/lcd_orientation_helper.h"

#if CONFIG_EXAMPLE_LCD_INTERFACE_SPI_WITHOUT_PSRAM

static const char *TAG = "hw_lcd_init";

#define HW_LCD_BASE_SWAP_XY                     (false)
#define HW_LCD_BASE_MIRROR_X                    (true)
#define HW_LCD_BASE_MIRROR_Y                    (false)

#define HW_LCD_SPI_HOST                         (SPI2_HOST)

#define HW_LCD_DATA0                            (GPIO_NUM_0)
#define HW_LCD_PCLK                             (GPIO_NUM_1)
#define HW_LCD_CS                               (GPIO_NUM_7)
#define HW_LCD_DC                               (GPIO_NUM_2)
#define HW_LCD_RST                              (GPIO_NUM_NC)

#define HW_LCD_PIXEL_CLOCK_HZ                   (80 * 1000 * 1000)
#define HW_LCD_COLOR_SPACE                      (LCD_RGB_ELEMENT_ORDER_BGR)
#define HW_LCD_BITS_PER_PIXEL                   (16)
#define HW_LCD_CMD_BITS                         (8)
#define HW_LCD_PARAM_BITS                       (8)
#define HW_LCD_MAX_TRANSFER_SZ                  (HW_LCD_H_RES * 80 * 2)

static esp_lcd_panel_io_handle_t s_panel_io_handle;
static esp_lcd_panel_handle_t s_panel_handle;
static bool s_spi_bus_initialized;

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
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)HW_LCD_SPI_HOST, &io_config, &s_panel_io_handle));

    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = HW_LCD_RST,
        .rgb_ele_order = HW_LCD_COLOR_SPACE,
        .bits_per_pixel = HW_LCD_BITS_PER_PIXEL,
    };

    ESP_LOGI(TAG, "Install gc9a01 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(s_panel_io_handle, &panel_config, &s_panel_handle));

    esp_lcd_panel_reset(s_panel_handle);
    esp_lcd_panel_init(s_panel_handle);

    bool swap_xy = false;
    bool mirror_x = true;
    bool mirror_y = false;
    lcd_get_orientation_flags(HW_LCD_BASE_SWAP_XY, HW_LCD_BASE_MIRROR_X, HW_LCD_BASE_MIRROR_Y,
                              rotation, &swap_xy, &mirror_x, &mirror_y);
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel_handle, swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel_handle, mirror_x, mirror_y));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true));
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    *io_handle = s_panel_io_handle;
    *panel_handle = s_panel_handle;

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
