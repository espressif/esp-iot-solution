/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "hw_multi.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_log.h"
#include "lcd_orientation_helper.h"

static const char *TAG = "hw_multi";

#define HW_LCD_BASE_SWAP_XY              (false)
#define HW_LCD_BASE_MIRROR_X             (true)
#define HW_LCD_BASE_MIRROR_Y             (false)

#define HW_LCD_PIXEL_CLOCK_HZ            (80 * 1000 * 1000)
#define HW_LCD_COLOR_SPACE               (LCD_RGB_ELEMENT_ORDER_BGR)
#define HW_LCD_BITS_PER_PIXEL            (16)
#define HW_LCD_CMD_BITS                  (8)
#define HW_LCD_PARAM_BITS                (8)
#define HW_LCD_MAX_TRANSFER_SZ           (BSP_LCD_H_RES * 80 * 2)

static bool s_bus_initialized;

esp_err_t hw_multi_lcd_init(hw_multi_panel_t *out_panels,
                            size_t panel_slots,
                            esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                            esp_lv_adapter_rotation_t rotation,
                            size_t *out_count)
{
    (void)tear_avoid_mode;
    ESP_RETURN_ON_FALSE(out_panels, ESP_ERR_INVALID_ARG, TAG, "out_panels is NULL");
    ESP_RETURN_ON_FALSE(panel_slots >= HW_MULTI_DISPLAY_COUNT, ESP_ERR_INVALID_ARG, TAG, "not enough panel slots");

    esp_err_t ret = ESP_OK;
    bool bus_initialized_in_call = false;
    size_t created_panels = 0;

    if (!s_bus_initialized) {
        const spi_bus_config_t buscfg = {
            .sclk_io_num = BSP_LCD_PCLK,
            .mosi_io_num = BSP_LCD_DATA0,
            .miso_io_num = GPIO_NUM_NC,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .max_transfer_sz = HW_LCD_MAX_TRANSFER_SZ,
        };
        ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "spi_bus_initialize failed");
        s_bus_initialized = true;
        bus_initialized_in_call = true;
        ESP_LOGI(TAG, "SPI bus initialized");
    }

    const gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << BSP_LCD_BL,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&bl_cfg);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "Failed to configure backlight GPIO");
    gpio_set_level(BSP_LCD_BL, 1);

    const gpio_num_t cs_pins[HW_MULTI_DISPLAY_COUNT] = {
        BSP_LCD_CS_0,
        BSP_LCD_CS_1,
    };

    for (size_t i = 0; i < HW_MULTI_DISPLAY_COUNT; ++i) {
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_handle_t panel_handle = NULL;

        esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num = BSP_LCD_DC,
            .cs_gpio_num = cs_pins[i],
            .pclk_hz = HW_LCD_PIXEL_CLOCK_HZ,
            .lcd_cmd_bits = HW_LCD_CMD_BITS,
            .lcd_param_bits = HW_LCD_PARAM_BITS,
            .spi_mode = 0,
            .trans_queue_depth = 10,
        };
        ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle);
        ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "esp_lcd_new_panel_io_spi failed for display %d", (int)i);

        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = BSP_LCD_RST,
            .rgb_ele_order = HW_LCD_COLOR_SPACE,
            .bits_per_pixel = HW_LCD_BITS_PER_PIXEL,
        };
        ret = esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle);
        if (ret != ESP_OK) {
            esp_lcd_panel_io_del(io_handle);
            ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "Failed to install GC9A01 panel driver for display %d", (int)i);
        }

        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);

        bool swap_xy = false;
        bool mirror_x = false;
        bool mirror_y = false;
        lcd_get_orientation_flags(HW_LCD_BASE_SWAP_XY,
                                  HW_LCD_BASE_MIRROR_X,
                                  HW_LCD_BASE_MIRROR_Y,
                                  rotation,
                                  &swap_xy,
                                  &mirror_x,
                                  &mirror_y);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_swap_xy(panel_handle, swap_xy));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_mirror(panel_handle, mirror_x, mirror_y));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_invert_color(panel_handle, true));
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel_handle, true));

        out_panels[i].panel_io = io_handle;
        out_panels[i].panel = panel_handle;
        created_panels = i + 1;

        ESP_LOGI(TAG, "Display %d initialized (CS GPIO %d)", (int)i, (int)cs_pins[i]);
    }

    if (out_count) {
        *out_count = created_panels;
    }
    return ESP_OK;

cleanup:
    for (size_t j = 0; j < created_panels; ++j) {
        if (out_panels[j].panel) {
            esp_lcd_panel_disp_on_off(out_panels[j].panel, false);
            esp_lcd_panel_del(out_panels[j].panel);
            out_panels[j].panel = NULL;
        }
        if (out_panels[j].panel_io) {
            esp_lcd_panel_io_del(out_panels[j].panel_io);
            out_panels[j].panel_io = NULL;
        }
    }

    if (bus_initialized_in_call) {
        spi_bus_free(SPI2_HOST);
        s_bus_initialized = false;
    }

    return ret;
}
