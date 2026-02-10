/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"
#include "esp_lcd_st77922.h"
#include "st77922_interface.h"

static const char *TAG = "st77922_general";

static esp_err_t panel_st77922_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st77922_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st77922_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st77922_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_st77922_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st77922_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st77922_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_st77922_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_st77922_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const st77922_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int use_qspi_interface: 1;
        unsigned int reset_level: 1;
    } flags;
} st77922_panel_t;

esp_err_t esp_lcd_new_panel_st77922_general(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    st77922_panel_t *st77922 = NULL;
    st77922 = calloc(1, sizeof(st77922_panel_t));
    ESP_GOTO_ON_FALSE(st77922, ESP_ERR_NO_MEM, err, TAG, "no mem for st77922 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        st77922->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        st77922->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        st77922->colmod_val = 0x55;
        st77922->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        st77922->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        st77922->fb_bits_per_pixel = 24;
        break;
    case 24: // RGB888
        st77922->colmod_val = 0x77;
        st77922->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    st77922->io = io;
    st77922->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st77922->flags.reset_level = panel_dev_config->flags.reset_active_high;
    st77922_vendor_config_t *vendor_config = (st77922_vendor_config_t *)panel_dev_config->vendor_config;
    if (vendor_config) {
        st77922->init_cmds = vendor_config->init_cmds;
        st77922->init_cmds_size = vendor_config->init_cmds_size;
        st77922->flags.use_qspi_interface = vendor_config->flags.use_qspi_interface;
    }
    st77922->base.del = panel_st77922_del;
    st77922->base.reset = panel_st77922_reset;
    st77922->base.init = panel_st77922_init;
    st77922->base.draw_bitmap = panel_st77922_draw_bitmap;
    st77922->base.invert_color = panel_st77922_invert_color;
    st77922->base.set_gap = panel_st77922_set_gap;
    st77922->base.mirror = panel_st77922_mirror;
    st77922->base.swap_xy = panel_st77922_swap_xy;
    st77922->base.disp_on_off = panel_st77922_disp_on_off;
    *ret_panel = &(st77922->base);
    ESP_LOGD(TAG, "new st77922 panel @%p", st77922);

    ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_ST77922_VER_MAJOR, ESP_LCD_ST77922_VER_MINOR,
             ESP_LCD_ST77922_VER_PATCH);

    return ESP_OK;

err:
    if (st77922) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(st77922);
    }
    return ret;
}

static esp_err_t tx_param(st77922_panel_t *st77922, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (st77922->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    }
    return esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
}

static esp_err_t tx_color(st77922_panel_t *st77922, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (st77922->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    }
    return esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
}

static esp_err_t panel_st77922_del(esp_lcd_panel_t *panel)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);

    if (st77922->reset_gpio_num >= 0) {
        gpio_reset_pin(st77922->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del st77922 panel @%p", st77922);
    free(st77922);
    return ESP_OK;
}

static esp_err_t panel_st77922_reset(esp_lcd_panel_t *panel)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77922->io;

    // Perform hardware reset
    if (st77922->reset_gpio_num >= 0) {
        gpio_set_level(st77922->reset_gpio_num, st77922->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st77922->reset_gpio_num, !st77922->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // Perform software reset
        ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static const st77922_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0x28, (uint8_t []){0x00}, 0, 0},
    {0x10, (uint8_t []){0x00}, 0, 0},
    {0x2A, (uint8_t []){0x00, 0x00, 0x02, 0x13}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x2B}, 4, 0},
    {0xD0, (uint8_t []){0x80}, 1, 0},
    // ======================CMD2======================
    {0xF1, (uint8_t []){0x00}, 0, 0},
    {0x60, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x65, (uint8_t []){0x00}, 1, 0},
    {0x66, (uint8_t []){0x00, 0x3F}, 2, 0},
    {0xBE, (uint8_t []){0x1E, 0x01, 0x06}, 3, 0},   // VCM SSI
    {0x70, (uint8_t []){0x02, 0x7D, 0x12, 0x14, 0x30, 0x00, 0x07, 0x52, 0x01, 0x00, 0x00, 0x1A}, 12, 0},    // VFP, VBP, Gate line
    {0x71, (uint8_t []){0xD0}, 1, 0},   // MIPI CMD Mode
    {0x7B, (uint8_t []){0x00, 0x08, 0x08}, 3, 0},
    {0x80, (uint8_t []){0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}, 11, 0},
    {0x81, (uint8_t []){0x26, 0x52, 0x72, 0x27}, 4, 0},
    {0x84, (uint8_t []){0x92, 0x25}, 2, 0},
    {0x86, (uint8_t []){0xC6, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x10, 0x13, 0x01, 0xAA, 0x00, 0xAA, 0xAA}, 14, 0},
    {0x87, (uint8_t []){0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}, 6, 0},
    {0x88, (uint8_t []){0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}, 15, 0},
    {0x89, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x8A, (uint8_t []){0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}, 11, 0},
    {0x8B, (uint8_t []){0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}, 9, 0},   // VGL Pump
    {0x8C, (uint8_t []){0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}, 13, 0},  // VGH Pump
    {0x8D, (uint8_t []){0x0C}, 1, 0},
    {0x8E, (uint8_t []){0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}, 6, 0},
    {0x90, (uint8_t []){0x00, 0x44, 0x33, 0x36, 0x00, 0x79, 0x40, 0xB6, 0xB6}, 9, 0},
    {0x91, (uint8_t []){0x00, 0x44, 0x33, 0x37, 0x00, 0x78, 0x40, 0xB6, 0xB6}, 9, 0},
    {0x92, (uint8_t []){0x02, 0x44, 0x55, 0x82, 0x86, 0x2F, 0x00, 0x04, 0x73, 0xB6}, 10, 0},
    {0x93, (uint8_t []){0x0C, 0x00, 0x11, 0x81, 0x87, 0x3F, 0x00, 0x00, 0x73, 0x73}, 10, 0},
    {0x94, (uint8_t []){0x08, 0x00, 0x00, 0x00, 0x00, 0x00}, 6, 0},
    {0x95, (uint8_t []){0x1A, 0x1A, 0x00, 0x00, 0xFF}, 5, 0},
    {0x96, (uint8_t []){0x44, 0x35, 0x07, 0x16, 0x20, 0x21, 0x07, 0x06, 0xB6, 0xB6, 0x00, 0x40}, 12, 0},
    {0x97, (uint8_t []){0x44, 0x35, 0x25, 0x34, 0x22, 0x23, 0x05, 0x04, 0xB6, 0xB6, 0x00, 0x40}, 12, 0},
    {0xBA, (uint8_t []){0x55, 0xB6, 0xB6, 0xB6, 0xB6}, 5, 0},
    {0x9A, (uint8_t []){0x40, 0x0C, 0x56, 0x01, 0x08, 0xB6, 0xB6}, 7, 0},
    {0x9B, (uint8_t []){0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9C, (uint8_t []){0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00}, 13, 0},
    {0x9D, (uint8_t []){0x80, 0x15, 0x00, 0x07, 0x01, 0x80, 0x73, 0x73}, 8, 0},
    {0x9E, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 7, 0},
    {0x9F, (uint8_t []){0xA0, 0x09, 0x00, 0x57}, 4, 0},
    {0xB3, (uint8_t []){0x00, 0x30, 0x0F, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0xB4, (uint8_t []){0x10, 0x1C, 0x19, 0x14, 0x18, 0x01, 0x1D, 0x03, 0x12, 0x0A, 0x11, 0x08}, 12, 0},
    {0xB5, (uint8_t []){0x1C, 0x1C, 0x19, 0x14, 0x18, 0x00, 0x1D, 0x02, 0x10, 0x0B, 0x13, 0x09}, 12, 0},
    {0xB6, (uint8_t []){0xFF, 0xFF, 0x00, 0x0F, 0xFE, 0x0F, 0xFE}, 7, 0},
    {0xB7, (uint8_t []){0x00, 0x09, 0x10, 0x0B, 0x0A, 0x06, 0x38, 0x04, 0x04, 0x4F, 0x09, 0x15, 0x15, 0x30, 0x37, 0x0F}, 16, 0},  // GammaP
    {0xB8, (uint8_t []){0x00, 0x09, 0x0F, 0x0A, 0x09, 0x05, 0x37, 0x03, 0x03, 0x4F, 0x09, 0x15, 0x15, 0x31, 0x36, 0x0F}, 16, 0},  // GammaN
    {0xB9, (uint8_t []){0x23, 0x23}, 2, 0},
    {0xBF, (uint8_t []){0x0F, 0x13, 0x13, 0x09, 0x09, 0x09}, 6, 0},     // VGHP/VGLP
    // ======================CMD3======================
    {0xF2, (uint8_t []){0x00}, 0, 0},
    {0x73, (uint8_t []){0x04, 0xBA, 0x1A, 0x58, 0x5B}, 5, 0},           // VOP= 5v
    {0x77, (uint8_t []){0x6B, 0x5B, 0xFB, 0xC3, 0xC5}, 5, 0},
    {0x7A, (uint8_t []){0x15, 0x27}, 2, 0},
    {0x7B, (uint8_t []){0x04, 0x57}, 2, 0},
    {0x7E, (uint8_t []){0x01, 0x0E}, 2, 0},
    {0xBF, (uint8_t []){0x36}, 1, 0},
    {0xE3, (uint8_t []){0x43, 0x43}, 2, 0},                             // VMF
    // ======================CMD1======================
    {0xF0, (uint8_t []){0x00}, 0, 0},
    {0x21, (uint8_t []){0x00}, 1, 0},
    {0x11, (uint8_t []){0x00}, 1, 120},
    {0x35, (uint8_t []){0x00}, 1, 20},
};

static esp_err_t panel_st77922_init(esp_lcd_panel_t *panel)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77922->io;
    const st77922_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_command1_enable = true;
    bool is_cmd_overwritten = false;

    ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77922->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_COLMOD, (uint8_t[]) {
        st77922->colmod_val,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (st77922->init_cmds) {
        init_cmds = st77922->init_cmds;
        init_cmds_size = st77922->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st77922_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (is_command1_enable && (init_cmds[i].data_bytes > 0)) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                st77922->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                st77922->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
            }
        }

        // Send command
        ESP_RETURN_ON_ERROR(tx_param(st77922, io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));

        // Check if the current cmd is the command1 enable cmd
        if ((init_cmds[i].cmd == ST77922_PAGE_CMD2 || init_cmds[i].cmd == ST77922_PAGE_CMD3) && init_cmds[i].data_bytes > 0) {
            is_command1_enable = false;
        } else if (init_cmds[i].cmd == ST77922_PAGE_CMD1 && init_cmds[i].data_bytes > 0) {
            is_command1_enable = true;
        }
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_st77922_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = st77922->io;

    x_start += st77922->x_gap;
    x_end += st77922->x_gap;
    y_start += st77922->y_gap;
    y_end += st77922->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * st77922->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(tx_color(st77922, io, LCD_CMD_RAMWR, color_data, len), TAG, "send color data failed");

    return ESP_OK;
}

static esp_err_t panel_st77922_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77922->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st77922_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77922->io;
    esp_err_t ret = ESP_OK;

    if (mirror_x) {
        st77922->madctl_val |= BIT(6);
    } else {
        st77922->madctl_val &= ~BIT(6);
    }
    if (mirror_y) {
        st77922->madctl_val |= BIT(7);
    } else {
        st77922->madctl_val &= ~BIT(7);
    }
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77922->madctl_val
    }, 1), TAG, "send command failed");
    return ret;
}

static esp_err_t panel_st77922_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ESP_LOGW(TAG, "swap_xy is not supported by this panel");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t panel_st77922_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    st77922->x_gap = x_gap;
    st77922->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st77922_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st77922_panel_t *st77922 = __containerof(panel, st77922_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77922->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(st77922, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
