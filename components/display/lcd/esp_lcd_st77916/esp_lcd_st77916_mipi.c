/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_st77916_interface.h"

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const st77916_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} st77916_mipi_panel_t;

static const char *TAG = "st77916_mipi";

static esp_err_t panel_st77916_mipi_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st77916_mipi_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st77916_mipi_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st77916_mipi_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st77916_mipi_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st77916_mipi_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_st77916_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                         esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    st77916_vendor_config_t *vendor_config = (st77916_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)calloc(1, sizeof(st77916_mipi_panel_t));
    ESP_RETURN_ON_FALSE(st77916_mipi, ESP_ERR_NO_MEM, TAG, "no mem for st77916_mipi panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        st77916_mipi->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        st77916_mipi->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    st77916_mipi->io = io;
    st77916_mipi->init_cmds = vendor_config->init_cmds;
    st77916_mipi->init_cmds_size = vendor_config->init_cmds_size;
    st77916_mipi->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st77916_mipi->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    st77916_mipi->del = panel_handle->del;
    st77916_mipi->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_st77916_mipi_del;
    panel_handle->init = panel_st77916_mipi_init;
    panel_handle->reset = panel_st77916_mipi_reset;
    panel_handle->mirror = panel_st77916_mipi_mirror;
    panel_handle->invert_color = panel_st77916_mipi_invert_color;
    panel_handle->disp_on_off = panel_st77916_mipi_disp_on_off;
    panel_handle->user_data = st77916_mipi;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new st77916_mipi panel @%p", st77916_mipi);

    return ESP_OK;

err:
    if (st77916_mipi) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(st77916_mipi);
    }
    return ret;
}

static const st77916_lcd_init_cmd_t vendor_specific_init_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    // ===== Page control / config =====
    {0xF0, (uint8_t[]){0x00, 0x28}, 2, 0},
    {0xF2, (uint8_t[]){0x00, 0x28}, 2, 0},
    {0x73, (uint8_t[]){0x00, 0xF0}, 2, 0},
    {0x7C, (uint8_t[]){0x00, 0xD1}, 2, 0},
    {0x83, (uint8_t[]){0x00, 0xE0}, 2, 0},
    {0x84, (uint8_t[]){0x00, 0x61}, 2, 0},
    {0xF2, (uint8_t[]){0x00, 0x82}, 2, 0},
    {0xF0, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xF0, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xF1, (uint8_t[]){0x00, 0x01}, 2, 0},

    // ===== Register settings (single-param → +0x00) =====
    {0xB0, (uint8_t[]){0x00, 0x56}, 2, 0},
    {0xB1, (uint8_t[]){0x00, 0x4D}, 2, 0},
    {0xB2, (uint8_t[]){0x00, 0x24}, 2, 0},
    {0xB4, (uint8_t[]){0x00, 0x87}, 2, 0},
    {0xB5, (uint8_t[]){0x00, 0x44}, 2, 0},
    {0xB6, (uint8_t[]){0x00, 0x8B}, 2, 0},
    {0xB7, (uint8_t[]){0x00, 0x40}, 2, 0},
    {0xB8, (uint8_t[]){0x00, 0x86}, 2, 0},
    {0xBA, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xBB, (uint8_t[]){0x00, 0x08}, 2, 0},
    {0xBC, (uint8_t[]){0x00, 0x08}, 2, 0},
    {0xBD, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xC0, (uint8_t[]){0x00, 0x80}, 2, 0},
    {0xC1, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xC2, (uint8_t[]){0x00, 0x37}, 2, 0},
    {0xC3, (uint8_t[]){0x00, 0x80}, 2, 0},
    {0xC4, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xC5, (uint8_t[]){0x00, 0x37}, 2, 0},
    {0xC6, (uint8_t[]){0x00, 0xA9}, 2, 0},
    {0xC7, (uint8_t[]){0x00, 0x41}, 2, 0},
    {0xC8, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xC9, (uint8_t[]){0x00, 0xA9}, 2, 0},
    {0xCA, (uint8_t[]){0x00, 0x41}, 2, 0},
    {0xCB, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xD0, (uint8_t[]){0x00, 0x91}, 2, 0},
    {0xD1, (uint8_t[]){0x00, 0x68}, 2, 0},
    {0xD2, (uint8_t[]){0x00, 0x68}, 2, 0},

    // ===== Special case: F5 (only add dummy before first arg) =====
    {0xF5, (uint8_t[]){0x00, 0x00, 0xA5}, 3, 0},

    {0xDD, (uint8_t[]){0x00, 0x4F}, 2, 0},
    {0xDE, (uint8_t[]){0x00, 0x4F}, 2, 0},
    {0xF1, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xF0, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xF0, (uint8_t[]){0x00, 0x02}, 2, 0},

    // ===== Gamma (multi-param → prepend 0x00) =====
    {0xE0, (uint8_t[]){0x00, 0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E, 0x34}, 15, 0},
    {0xE1, (uint8_t[]){0x00, 0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D, 0x33}, 15, 0},

    {0xF0, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xF3, (uint8_t[]){0x00, 0x10}, 2, 0},

    // ===== More registers =====
    {0xE0, (uint8_t[]){0x00, 0x07}, 2, 0},
    {0xE1, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xE2, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xE3, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xE4, (uint8_t[]){0x00, 0xE0}, 2, 0},
    {0xE5, (uint8_t[]){0x00, 0x06}, 2, 0},
    {0xE6, (uint8_t[]){0x00, 0x21}, 2, 0},
    {0xE7, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xE8, (uint8_t[]){0x00, 0x05}, 2, 0},
    {0xE9, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xEA, (uint8_t[]){0x00, 0xDA}, 2, 0},
    {0xEB, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xEC, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0x00, 0x0F}, 2, 0},
    {0xEE, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xEF, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xF8, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xF9, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFA, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFB, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFC, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFD, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFE, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xFF, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x60, (uint8_t[]){0x00, 0x40}, 2, 0},
    {0x61, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0x62, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x63, (uint8_t[]){0x00, 0x42}, 2, 0},
    {0x64, (uint8_t[]){0x00, 0xD9}, 2, 0},
    {0x65, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x66, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x67, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x68, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x69, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x6A, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x6B, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x70, (uint8_t[]){0x00, 0x40}, 2, 0},
    {0x71, (uint8_t[]){0x00, 0x03}, 2, 0},
    {0x72, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x73, (uint8_t[]){0x00, 0x42}, 2, 0},
    {0x74, (uint8_t[]){0x00, 0xD8}, 2, 0},
    {0x75, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x76, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x77, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x78, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x79, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x7A, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x7B, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x80, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0x81, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x82, (uint8_t[]){0x00, 0x06}, 2, 0},
    {0x83, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0x84, (uint8_t[]){0x00, 0xD6}, 2, 0},
    {0x85, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0x86, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x87, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x88, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0x89, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x8A, (uint8_t[]){0x00, 0x08}, 2, 0},
    {0x8B, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0x8C, (uint8_t[]){0x00, 0xD8}, 2, 0},
    {0x8D, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0x8E, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x8F, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x90, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0x91, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x92, (uint8_t[]){0x00, 0x0A}, 2, 0},
    {0x93, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0x94, (uint8_t[]){0x00, 0xDA}, 2, 0},
    {0x95, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0x96, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x97, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0x98, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0x99, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x9A, (uint8_t[]){0x00, 0x0C}, 2, 0},
    {0x9B, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0x9C, (uint8_t[]){0x00, 0xDC}, 2, 0},
    {0x9D, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0x9E, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0x9F, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xA0, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0xA1, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xA2, (uint8_t[]){0x00, 0x05}, 2, 0},
    {0xA3, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xA4, (uint8_t[]){0x00, 0xD5}, 2, 0},
    {0xA5, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0xA6, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xA7, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xA8, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0xA9, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xAA, (uint8_t[]){0x00, 0x07}, 2, 0},
    {0xAB, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xAC, (uint8_t[]){0x00, 0xD7}, 2, 0},
    {0xAD, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0xAE, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xAF, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xB0, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0xB1, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xB2, (uint8_t[]){0x00, 0x09}, 2, 0},
    {0xB3, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xB4, (uint8_t[]){0x00, 0xD9}, 2, 0},
    {0xB5, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0xB6, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xB7, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xB8, (uint8_t[]){0x00, 0x48}, 2, 0},
    {0xB9, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xBA, (uint8_t[]){0x00, 0x0B}, 2, 0},
    {0xBB, (uint8_t[]){0x00, 0x02}, 2, 0},
    {0xBC, (uint8_t[]){0x00, 0xDB}, 2, 0},
    {0xBD, (uint8_t[]){0x00, 0x04}, 2, 0},
    {0xBE, (uint8_t[]){0x00, 0x00}, 2, 0},
    {0xBF, (uint8_t[]){0x00, 0x00}, 2, 0},

    {0xC0, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xC1, (uint8_t[]){0x00, 0x47}, 2, 0},
    {0xC2, (uint8_t[]){0x00, 0x56}, 2, 0},
    {0xC3, (uint8_t[]){0x00, 0x65}, 2, 0},
    {0xC4, (uint8_t[]){0x00, 0x74}, 2, 0},
    {0xC5, (uint8_t[]){0x00, 0x88}, 2, 0},
    {0xC6, (uint8_t[]){0x00, 0x99}, 2, 0},
    {0xC7, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xC8, (uint8_t[]){0x00, 0xBB}, 2, 0},
    {0xC9, (uint8_t[]){0x00, 0xAA}, 2, 0},

    {0xD0, (uint8_t[]){0x00, 0x10}, 2, 0},
    {0xD1, (uint8_t[]){0x00, 0x47}, 2, 0},
    {0xD2, (uint8_t[]){0x00, 0x56}, 2, 0},
    {0xD3, (uint8_t[]){0x00, 0x65}, 2, 0},
    {0xD4, (uint8_t[]){0x00, 0x74}, 2, 0},
    {0xD5, (uint8_t[]){0x00, 0x88}, 2, 0},
    {0xD6, (uint8_t[]){0x00, 0x99}, 2, 0},
    {0xD7, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xD8, (uint8_t[]){0x00, 0xBB}, 2, 0},
    {0xD9, (uint8_t[]){0x00, 0xAA}, 2, 0},

    {0xF3, (uint8_t[]){0x00, 0x01}, 2, 0},
    {0xF0, (uint8_t[]){0x00, 0x00}, 2, 0},

    // ===== Display ON sequence (Video mode) =====
    {0x21, (uint8_t[]){}, 0, 0},
    {0x11, (uint8_t[]){}, 0, 120},
    {0x29, (uint8_t[]){}, 0, 0},
    {0x00, (uint8_t[]){}, 0, 0},
};

static esp_err_t panel_st77916_mipi_del(esp_lcd_panel_t *panel)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;

    if (st77916_mipi->reset_gpio_num >= 0) {
        gpio_reset_pin(st77916_mipi->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    st77916_mipi->del(panel);
    ESP_LOGD(TAG, "del st77916_mipi panel @%p", st77916_mipi);
    free(st77916_mipi);

    return ESP_OK;
}

static esp_err_t panel_st77916_mipi_init(esp_lcd_panel_t *panel)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77916_mipi->io;
    const st77916_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    uint8_t ID[3];
    ESP_LOGI(TAG, "read ID");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0x04, ID, 3), TAG, "read ID failed");
    ESP_LOGI(TAG, "LCD ID: %02X %02X %02X", ID[0], ID[1], ID[2]);

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77916_mipi->madctl_val,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (st77916_mipi->init_cmds) {
        init_cmds = st77916_mipi->init_cmds;
        init_cmds_size = st77916_mipi->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st77916_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                st77916_mipi->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                st77916_mipi->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                         init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    ESP_RETURN_ON_ERROR(st77916_mipi->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_st77916_mipi_reset(esp_lcd_panel_t *panel)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77916_mipi->io;

    // Perform hardware reset
    if (st77916_mipi->reset_gpio_num >= 0) {
        gpio_set_level(st77916_mipi->reset_gpio_num, !st77916_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(st77916_mipi->reset_gpio_num, st77916_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st77916_mipi->reset_gpio_num, !st77916_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_st77916_mipi_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77916_mipi->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t panel_st77916_mipi_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77916_mipi->io;
    uint8_t madctl_val = st77916_mipi->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x) {
        ESP_LOGW(TAG, "Mirror X is not supported");
    }

    if (mirror_y) {
        madctl_val |= BIT(7);
    } else {
        madctl_val &= ~BIT(7);
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    st77916_mipi->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_st77916_mipi_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st77916_mipi_panel_t *st77916_mipi = (st77916_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77916_mipi->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
#endif
