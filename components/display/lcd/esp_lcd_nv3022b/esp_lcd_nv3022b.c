/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_lcd_nv3022b.h"

static const char *TAG = "lcd_panel.nv3022b";

static esp_err_t panel_nv3022b_del(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3022b_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3022b_init(esp_lcd_panel_t *panel);
static esp_err_t panel_nv3022b_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_nv3022b_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_nv3022b_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_nv3022b_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_nv3022b_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_nv3022b_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const nv3022b_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} nv3022b_panel_t;

esp_err_t esp_lcd_new_panel_nv3022b(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    nv3022b_panel_t *nv3022b = NULL;
    gpio_config_t io_conf = { 0 };

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    nv3022b = (nv3022b_panel_t *)calloc(1, sizeof(nv3022b_panel_t));
    ESP_GOTO_ON_FALSE(nv3022b, ESP_ERR_NO_MEM, err, TAG, "no mem for nv3022b panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (panel_dev_config->rgb_ele_order) {
    case ESP_LCD_COLOR_SPACE_RGB:
        nv3022b->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        nv3022b->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        nv3022b->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        nv3022b->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }
#else
    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        nv3022b->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        nv3022b->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb element order");
        break;
    }
#endif

    switch (panel_dev_config->bits_per_pixel) {
    case 12: // RGB444
        nv3022b->colmod_val = 0x33;
        nv3022b->fb_bits_per_pixel = 16;
        break;
    case 16: // RGB565
        nv3022b->colmod_val = 0x55;
        nv3022b->fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        nv3022b->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        nv3022b->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    nv3022b->io = io;
    nv3022b->reset_gpio_num = panel_dev_config->reset_gpio_num;
    nv3022b->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config) {
        nv3022b->init_cmds = ((nv3022b_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        nv3022b->init_cmds_size = ((nv3022b_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    nv3022b->base.del = panel_nv3022b_del;
    nv3022b->base.reset = panel_nv3022b_reset;
    nv3022b->base.init = panel_nv3022b_init;
    nv3022b->base.draw_bitmap = panel_nv3022b_draw_bitmap;
    nv3022b->base.invert_color = panel_nv3022b_invert_color;
    nv3022b->base.set_gap = panel_nv3022b_set_gap;
    nv3022b->base.mirror = panel_nv3022b_mirror;
    nv3022b->base.swap_xy = panel_nv3022b_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    nv3022b->base.disp_off = panel_nv3022b_disp_on_off;
#else
    nv3022b->base.disp_on_off = panel_nv3022b_disp_on_off;
#endif
    *ret_panel = &(nv3022b->base);
    ESP_LOGD(TAG, "new nv3022b panel @%p", nv3022b);

    ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_NV3022B_VER_MAJOR, ESP_LCD_NV3022B_VER_MINOR,
             ESP_LCD_NV3022B_VER_PATCH);

    return ESP_OK;

err:
    if (nv3022b) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(nv3022b);
    }
    return ret;
}

static esp_err_t panel_nv3022b_del(esp_lcd_panel_t *panel)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);

    if (nv3022b->reset_gpio_num >= 0) {
        gpio_reset_pin(nv3022b->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del nv3022b panel @%p", nv3022b);
    free(nv3022b);
    return ESP_OK;
}

static esp_err_t panel_nv3022b_reset(esp_lcd_panel_t *panel)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;

    // perform hardware reset
    if (nv3022b->reset_gpio_num >= 0) {
        gpio_set_level(nv3022b->reset_gpio_num, nv3022b->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(nv3022b->reset_gpio_num, !nv3022b->reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // perform software reset
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static const nv3022b_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xfd, (uint8_t []){0x06, 0x08}, 2, 0},
    {0x61, (uint8_t []){0x07, 0x07}, 2, 0},
    {0x73, (uint8_t []){0x70}, 1, 0},
    {0x73, (uint8_t []){0x00}, 1, 0},
    {0x62, (uint8_t []){0x00, 0x44, 0x40}, 3, 0},
    {0x65, (uint8_t []){0x08, 0x10, 0x21}, 3, 0},
    {0x66, (uint8_t []){0x08, 0x10, 0x21}, 3, 0},
    {0x67, (uint8_t []){0x21, 0x40}, 2, 0},
    {0x68, (uint8_t []){0x9f, 0x30, 0x27, 0x21}, 4, 0},
    {0xb1, (uint8_t []){0x0f, 0x02, 0x01}, 3, 0},
    {0xb4, (uint8_t []){0x01}, 1, 0},
    {0xb5, (uint8_t []){0x02, 0x02, 0x0a, 0x14}, 4, 0},
    {0xb6, (uint8_t []){0x44, 0x01, 0x9f, 0x00, 0x02}, 5, 0},
    {0xe6, (uint8_t []){0x00, 0xff}, 2, 0},
    {0xe7, (uint8_t []){0x01, 0x04, 0x03, 0x03, 0x00, 0x12}, 6, 0},
    {0xe8, (uint8_t []){0x00, 0x70, 0x00}, 3, 0},
    {0xec, (uint8_t []){0x52}, 1, 0},
    {0xdf, (uint8_t []){0x11}, 1, 0},
    {0xe0, (uint8_t []){0x06, 0x05, 0x0b, 0x12, 0x10, 0x10, 0x10, 0x15}, 8, 0},
    {0xe3, (uint8_t []){0x15, 0x10, 0x11, 0x0e, 0x12, 0x0d, 0x06, 0x06}, 8, 0},
    {0xe1, (uint8_t []){0x35, 0x75}, 2, 0},
    {0xe4, (uint8_t []){0x74, 0x35}, 2, 0},
    {0xe2, (uint8_t []){0x22, 0x22, 0x21, 0x35, 0x36, 0x3f}, 6, 0},
    {0xe5, (uint8_t []){0x3f, 0x35, 0x34, 0x21, 0x22, 0x22}, 6, 0},
    {0xf1, (uint8_t []){0x01, 0x01, 0x02}, 3, 0},
    {0xf6, (uint8_t []){0x09, 0x30, 0x00, 0x00}, 4, 0},
    {0xfd, (uint8_t []){0xfa, 0xfc}, 2, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x11, (uint8_t []){0x00}, 0, 200},
    {0x2a, (uint8_t []){0x00, 0x00, 0x00, 0xef}, 4, 0},
    {0x2b, (uint8_t []){0x00, 0x00, 0x01, 0x1b}, 4, 0},
    {0x29, (uint8_t []){0}, 0, 10},
    {0x2c, (uint8_t []){0}, 0, 0},
};

static esp_err_t panel_nv3022b_init(esp_lcd_panel_t *panel)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        nv3022b->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        nv3022b->colmod_val,
    }, 1), TAG, "send command failed");

    const nv3022b_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (nv3022b->init_cmds) {
        init_cmds = nv3022b->init_cmds;
        init_cmds_size = nv3022b->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(nv3022b_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            nv3022b->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            nv3022b->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_nv3022b_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = nv3022b->io;

    x_start += nv3022b->x_gap;
    x_end += nv3022b->x_gap;
    y_start += nv3022b->y_gap;
    y_end += nv3022b->y_gap;

    // define an area of frame memory where MCU can access
    esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4);
    esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4);
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * nv3022b->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len), TAG, "send color data failed");

    return ESP_OK;
}

static esp_err_t panel_nv3022b_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_nv3022b_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;
    if (mirror_x) {
        nv3022b->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        nv3022b->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        nv3022b->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        nv3022b->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        nv3022b->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_nv3022b_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;
    if (swap_axes) {
        nv3022b->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        nv3022b->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        nv3022b->madctl_val
    }, 1);
    return ESP_OK;
}

static esp_err_t panel_nv3022b_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    nv3022b->x_gap = x_gap;
    nv3022b->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_nv3022b_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    nv3022b_panel_t *nv3022b = __containerof(panel, nv3022b_panel_t, base);
    esp_lcd_panel_io_handle_t io = nv3022b->io;
    int command = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    on_off = !on_off;
#endif

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
