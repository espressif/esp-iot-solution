/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "esp_lcd_st77926.h"
#include "esp_lcd_st77926_interface.h"

#define LCD_OPCODE_WRITE_CMD        (0x02ULL)
#define LCD_OPCODE_WRITE_COLOR      (0x32ULL)

static const char *TAG = "st77926_qspi";

static esp_err_t panel_st77926_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st77926_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st77926_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st77926_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_st77926_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_st77926_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st77926_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_st77926_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_st77926_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val;
    uint8_t colmod_val;
    const st77926_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level: 1;
    } flags;
} st77926_panel_t;

esp_err_t esp_lcd_new_panel_st77926_qspi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                         esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    st77926_panel_t *st77926 = calloc(1, sizeof(st77926_panel_t));
    ESP_GOTO_ON_FALSE(st77926, ESP_ERR_NO_MEM, err, TAG, "no mem for st77926 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        st77926->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        st77926->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        st77926->colmod_val = 0x01;
        st77926->fb_bits_per_pixel = 16;
        break;
    case 18:
        st77926->colmod_val = 0x02;
        st77926->fb_bits_per_pixel = 24;
        break;
    case 24:
        st77926->colmod_val = 0x03;
        st77926->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    st77926->io = io;
    st77926->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st77926->flags.reset_level = panel_dev_config->flags.reset_active_high;

    st77926_vendor_config_t *vendor_config = (st77926_vendor_config_t *)panel_dev_config->vendor_config;
    if (vendor_config) {
        st77926->init_cmds = vendor_config->init_cmds;
        st77926->init_cmds_size = vendor_config->init_cmds_size;
    }

    st77926->base.del = panel_st77926_del;
    st77926->base.reset = panel_st77926_reset;
    st77926->base.init = panel_st77926_init;
    st77926->base.draw_bitmap = panel_st77926_draw_bitmap;
    st77926->base.invert_color = panel_st77926_invert_color;
    st77926->base.set_gap = panel_st77926_set_gap;
    st77926->base.mirror = panel_st77926_mirror;
    st77926->base.swap_xy = panel_st77926_swap_xy;
    st77926->base.disp_on_off = panel_st77926_disp_on_off;
    *ret_panel = &(st77926->base);
    ESP_LOGD(TAG, "new st77926 panel @%p", st77926);

    return ESP_OK;

err:
    if (st77926) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(st77926);
    }
    return ret;
}

static esp_err_t tx_param(st77926_panel_t *st77926, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    (void)st77926;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    return esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
}

static esp_err_t tx_color(st77926_panel_t *st77926, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    (void)st77926;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    return esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
}

static esp_err_t panel_st77926_del(esp_lcd_panel_t *panel)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);

    if (st77926->reset_gpio_num >= 0) {
        gpio_reset_pin(st77926->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del st77926 panel @%p", st77926);
    free(st77926);
    return ESP_OK;
}

static esp_err_t panel_st77926_reset(esp_lcd_panel_t *panel)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;

    if (st77926->reset_gpio_num >= 0) {
        gpio_set_level(st77926->reset_gpio_num, st77926->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st77926->reset_gpio_num, !st77926->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static const st77926_lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xF1, (uint8_t []){0x00}, 1, 0},
    {0x60, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x65, (uint8_t []){0x80}, 1, 0},
    {0x79, (uint8_t []){0x06}, 1, 0},
    {0x7B, (uint8_t []){0x00, 0x08, 0x08}, 3, 0},
    {0x80, (uint8_t []){0x55, 0x62, 0x2F, 0x17, 0xF0, 0x52, 0x70, 0xD2, 0x52, 0x62, 0xEA}, 11, 0},
    {0x81, (uint8_t []){0x26, 0x52, 0x72, 0x27}, 4, 0},
    {0x84, (uint8_t []){0x92, 0x25}, 2, 0},
    {0x87, (uint8_t []){0x10, 0x10, 0x58, 0x00, 0x02, 0x3A}, 6, 0},
    {0x88, (uint8_t []){0x00, 0x00, 0x2C, 0x10, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x06}, 15, 0},
    {0x89, (uint8_t []){0x00, 0x00, 0x00}, 3, 0},
    {0x8A, (uint8_t []){0x13, 0x00, 0x2C, 0x00, 0x00, 0x2C, 0x10, 0x10, 0x00, 0x3E, 0x19}, 11, 0},
    {0x8B, (uint8_t []){0x15, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x97, 0x8E}, 9, 0},
    {0x8C, (uint8_t []){0x1D, 0xB1, 0xB1, 0x44, 0x96, 0x2C, 0x10, 0x50, 0x0F, 0x01, 0xC5, 0x12, 0x09}, 13, 0},
    {0x8D, (uint8_t []){0x0C}, 1, 0},
    {0x8E, (uint8_t []){0x33, 0x01, 0x0C, 0x13, 0x01, 0x01}, 6, 0},
    {0xB3, (uint8_t []){0x00, 0x30}, 2, 0},
    {0xF1, (uint8_t []){0x00}, 1, 0},
    {0x71, (uint8_t []){0xD0}, 1, 0},
    {0x66, (uint8_t []){0x02, 0x3F}, 2, 0},
    {0xBE, (uint8_t []){0x20, 0x00, 0x9D}, 3, 0},
    {0x70, (uint8_t []){0x01, 0xA4, 0x11, 0x40, 0xE0, 0x00, 0x0F, 0x65, 0x00, 0x00, 0x00, 0x1A}, 12, 0},
    {0x90, (uint8_t []){0x00, 0x44, 0x55, 0x31, 0xC4, 0x71, 0x44, 0x61, 0x61}, 9, 0},
    {0x91, (uint8_t []){0x00, 0x44, 0x55, 0x36, 0x00, 0x76, 0x40, 0x61, 0x61}, 9, 0},
    {0x92, (uint8_t []){0x00, 0x44, 0x55, 0x37, 0x00, 0x37, 0x00, 0x05, 0x61, 0x61}, 10, 0},
    {0x93, (uint8_t []){0x00, 0x43, 0x11, 0x00, 0x00, 0x00, 0x00, 0x05, 0x61, 0x61}, 10, 0},
    {0x94, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6, 0},
    {0x95, (uint8_t []){0x99, 0x19, 0x00, 0x00, 0xFF}, 5, 0},
    {0x96, (uint8_t []){0x44, 0x44, 0x07, 0x16, 0x00, 0x20, 0x04, 0x03, 0xC8, 0x61, 0x00, 0x40}, 12, 0},
    {0x97, (uint8_t []){0x44, 0x44, 0x25, 0x34, 0x00, 0x20, 0x02, 0x01, 0xC8, 0x61, 0x00, 0x40}, 12, 0},
    {0xBA, (uint8_t []){0x44, 0xC8, 0x61, 0xC8, 0x61}, 5, 0},
    {0x9A, (uint8_t []){0x40, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9B, (uint8_t []){0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00}, 7, 0},
    {0x9C, (uint8_t []){0x40, 0x12, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00}, 13, 0},
    {0x9D, (uint8_t []){0x8C, 0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 8, 0},
    {0x9E, (uint8_t []){0x51, 0x00, 0x00, 0x00, 0x80, 0x1E, 0x01}, 7, 0},
    {0xB4, (uint8_t []){0x14, 0x1E, 0x10, 0x1C, 0x19, 0x1D, 0x1E, 0x18, 0x03, 0x0A, 0x01, 0x08}, 12, 0},
    {0xB5, (uint8_t []){0x12, 0x1E, 0x10, 0x1C, 0x19, 0x1D, 0x1E, 0x18, 0x02, 0x0B, 0x00, 0x09}, 12, 0},
    {0xB6, (uint8_t []){0x99, 0x99, 0x00, 0x0F, 0xBF, 0x0F, 0xBF}, 7, 0},
    {0x86, (uint8_t []){0xC9, 0x04, 0xB1, 0x02, 0x58, 0x12, 0x58, 0x0C, 0x13, 0x01, 0xA5, 0x00, 0xA5, 0xA5}, 14, 0},
    {0xB7, (uint8_t []){0x00, 0x0C, 0x0C, 0x0E, 0x0B, 0x07, 0x34, 0x03, 0x04, 0x49, 0x08, 0x15, 0x15, 0x2C, 0x32, 0x0F}, 16, 0},
    {0xB8, (uint8_t []){0x00, 0x0C, 0x0C, 0x0E, 0x0B, 0x06, 0x34, 0x03, 0x04, 0x49, 0x08, 0x14, 0x14, 0x2C, 0x32, 0x0F}, 16, 0},
    {0xB9, (uint8_t []){0x23, 0x23}, 2, 0},
    {0xBF, (uint8_t []){0x0D, 0x11, 0x11, 0x0B, 0x0B, 0x0B}, 6, 0},
    {0xF2, (uint8_t []){0x00}, 1, 0},
    {0x73, (uint8_t []){0x04, 0xDA, 0x12, 0x52, 0x51}, 5, 0},
    {0x77, (uint8_t []){0x6B, 0x5B, 0xFD, 0xC3, 0xC5}, 5, 0},
    {0x7A, (uint8_t []){0x15, 0x27}, 2, 0},
    {0x7B, (uint8_t []){0x04, 0x57}, 2, 0},
    {0x7E, (uint8_t []){0x01, 0x0E}, 2, 0},
    {0xBF, (uint8_t []){0x36}, 1, 0},
    {0xE3, (uint8_t []){0x40, 0x40}, 2, 0},
    {0xF0, (uint8_t []){0x00}, 1, 0},
    {0xD0, (uint8_t []){0x00}, 1, 0},
    {LCD_CMD_CASET, (uint8_t []){0x00, 0x00, 0x01, 0x3F}, 4, 0},
    {LCD_CMD_RASET, (uint8_t []){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {LCD_CMD_INVON, NULL, 0, 0},
    {LCD_CMD_SLPOUT, NULL, 0, 120},
    {LCD_CMD_DISPON, NULL, 0, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0xF3, (uint8_t []){0x00}, 1, 0},
    {0x70, (uint8_t []){0x00, 0x00}, 2, 0},
};

static esp_err_t panel_st77926_init(esp_lcd_panel_t *panel)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;
    const st77926_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_user_cmds = st77926->init_cmds != NULL;

    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77926->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_COLMOD, (uint8_t[]) {
        st77926->colmod_val,
    }, 1), TAG, "send command failed");

    if (st77926->init_cmds) {
        init_cmds = st77926->init_cmds;
        init_cmds_size = st77926->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st77926_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                if (is_user_cmds && (st77926->madctl_val != ((uint8_t *)init_cmds[i].data)[0])) {
                    ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by initialization sequence", init_cmds[i].cmd);
                }
                st77926->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                if (is_user_cmds && (st77926->colmod_val != ((uint8_t *)init_cmds[i].data)[0])) {
                    ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by initialization sequence", init_cmds[i].cmd);
                }
                st77926->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                break;
            }
        }

        ESP_RETURN_ON_ERROR(tx_param(st77926, io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_st77926_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = st77926->io;

    x_start += st77926->x_gap;
    x_end += st77926->x_gap;
    y_start += st77926->y_gap;
    y_end += st77926->y_gap;

    ESP_RETURN_ON_FALSE(((x_start & 0x03) == 0) && (((x_end - x_start) & 0x03) == 0), ESP_ERR_INVALID_ARG, TAG,
                        "x_start and width must be 4-pixel aligned");

    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");

    size_t len = (x_end - x_start) * (y_end - y_start) * st77926->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(tx_color(st77926, io, LCD_CMD_RAMWR, color_data, len), TAG, "send color data failed");

    return ESP_OK;
}

static esp_err_t panel_st77926_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;
    int command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;

    ESP_RETURN_ON_ERROR(tx_param(st77926, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st77926_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;

    if (mirror_x) {
        st77926->madctl_val |= BIT(6);
    } else {
        st77926->madctl_val &= ~BIT(6);
    }
    if (mirror_y) {
        st77926->madctl_val |= BIT(7);
    } else {
        st77926->madctl_val &= ~BIT(7);
    }
    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77926->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st77926_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;

    if (swap_axes) {
        st77926->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        st77926->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(tx_param(st77926, io, LCD_CMD_MADCTL, (uint8_t[]) {
        st77926->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_st77926_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    st77926->x_gap = x_gap;
    st77926->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_st77926_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st77926_panel_t *st77926 = __containerof(panel, st77926_panel_t, base);
    esp_lcd_panel_io_handle_t io = st77926->io;
    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;

    ESP_RETURN_ON_ERROR(tx_param(st77926, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
