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

#include "esp_lcd_gc9b71.h"

#define LCD_OPCODE_WRITE_CMD        (0x02ULL)
#define LCD_OPCODE_READ_CMD         (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR      (0x32ULL)

static const char *TAG = "gc9b71";

static esp_err_t panel_gc9b71_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9b71_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9b71_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9b71_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_gc9b71_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_gc9b71_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9b71_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_gc9b71_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_gc9b71_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const gc9b71_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int use_qspi_interface: 1;
        unsigned int reset_level: 1;
    } flags;
} gc9b71_panel_t;

esp_err_t esp_lcd_new_panel_gc9b71(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_err_t ret = ESP_OK;
    gc9b71_panel_t *gc9b71 = NULL;
    gc9b71 = calloc(1, sizeof(gc9b71_panel_t));
    ESP_GOTO_ON_FALSE(gc9b71, ESP_ERR_NO_MEM, err, TAG, "no mem for gc9b71 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        gc9b71->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        gc9b71->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    uint8_t fb_bits_per_pixel = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        gc9b71->colmod_val = 0x55;
        fb_bits_per_pixel = 16;
        break;
    case 18: // RGB666
        gc9b71->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        fb_bits_per_pixel = 24;
        break;
    case 24: // RGB888
        gc9b71->colmod_val = 0x77;
        fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    gc9b71->io = io;
    gc9b71->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9b71->fb_bits_per_pixel = fb_bits_per_pixel;
    gc9b71_vendor_config_t *vendor_config = (gc9b71_vendor_config_t *)panel_dev_config->vendor_config;
    if (vendor_config) {
        gc9b71->init_cmds = vendor_config->init_cmds;
        gc9b71->init_cmds_size = vendor_config->init_cmds_size;
        gc9b71->flags.use_qspi_interface = vendor_config->flags.use_qspi_interface;
    }
    gc9b71->flags.reset_level = panel_dev_config->flags.reset_active_high;
    gc9b71->base.del = panel_gc9b71_del;
    gc9b71->base.reset = panel_gc9b71_reset;
    gc9b71->base.init = panel_gc9b71_init;
    gc9b71->base.draw_bitmap = panel_gc9b71_draw_bitmap;
    gc9b71->base.invert_color = panel_gc9b71_invert_color;
    gc9b71->base.set_gap = panel_gc9b71_set_gap;
    gc9b71->base.mirror = panel_gc9b71_mirror;
    gc9b71->base.swap_xy = panel_gc9b71_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    gc9b71->base.disp_off = panel_gc9b71_disp_on_off;
#else
    gc9b71->base.disp_on_off = panel_gc9b71_disp_on_off;
#endif
    *ret_panel = &(gc9b71->base);
    ESP_LOGD(TAG, "new gc9b71 panel @%p", gc9b71);

    ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_GC9B71_VER_MAJOR, ESP_LCD_GC9B71_VER_MINOR,
             ESP_LCD_GC9B71_VER_PATCH);

    return ESP_OK;

err:
    if (gc9b71) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9b71);
    }
    return ret;
}

static esp_err_t tx_param(gc9b71_panel_t *gc9b71, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (gc9b71->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
    }
    return esp_lcd_panel_io_tx_param(io, lcd_cmd, param, param_size);
}

static esp_err_t tx_color(gc9b71_panel_t *gc9b71, esp_lcd_panel_io_handle_t io, int lcd_cmd, const void *param, size_t param_size)
{
    if (gc9b71->flags.use_qspi_interface) {
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_COLOR << 24;
    }
    return esp_lcd_panel_io_tx_color(io, lcd_cmd, param, param_size);
}

static esp_err_t panel_gc9b71_del(esp_lcd_panel_t *panel)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);

    if (gc9b71->reset_gpio_num >= 0) {
        gpio_reset_pin(gc9b71->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del gc9b71 panel @%p", gc9b71);
    free(gc9b71);
    return ESP_OK;
}

static esp_err_t panel_gc9b71_reset(esp_lcd_panel_t *panel)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;

    // Perform hardware reset
    if (gc9b71->reset_gpio_num >= 0) {
        gpio_set_level(gc9b71->reset_gpio_num, gc9b71->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gc9b71->reset_gpio_num, !gc9b71->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else { // Perform software reset
        ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static const gc9b71_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t []){0x00}, 0, 0},
    {0xef, (uint8_t []){0x00}, 0, 0},
    {0x80, (uint8_t []){0x11}, 1, 0},
    {0x81, (uint8_t []){0x70}, 1, 0},
    {0x82, (uint8_t []){0x09}, 1, 0},
    {0x83, (uint8_t []){0x03}, 1, 0},
    {0x84, (uint8_t []){0x62}, 1, 0},
    {0x89, (uint8_t []){0x18}, 1, 0},
    {0x8A, (uint8_t []){0x40}, 1, 0},
    {0x8B, (uint8_t []){0x0A}, 1, 0},
    {0xEC, (uint8_t []){0x07}, 1, 0},
    {0x74, (uint8_t []){0x01, 0x80, 0x00, 0x00, 0x00, 0x00}, 6, 0},
    {0x98, (uint8_t []){0x3E}, 1, 0},
    {0x99, (uint8_t []){0x3E}, 1, 0},
    {0xA1, (uint8_t []){0x01, 0x04}, 2, 0},
    {0xA2, (uint8_t []){0x01, 0x04}, 2, 0},
    {0xCB, (uint8_t []){0x02}, 1, 0},
    {0x7C, (uint8_t []){0xB6, 0x24}, 2, 0},
    {0xAC, (uint8_t []){0x74}, 1, 0},
    {0xF6, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x09, 0x09}, 2, 0},
    {0xEB, (uint8_t []){0x01, 0x81}, 2, 0},
    {0x60, (uint8_t []){0x38, 0x06, 0x13, 0x56}, 4, 0},
    {0x63, (uint8_t []){0x38, 0x08, 0x13, 0x56}, 4, 0},
    {0x61, (uint8_t []){0x3B, 0x1b, 0x58, 0x38}, 4, 0},
    {0x62, (uint8_t []){0x3B, 0x1b, 0x58, 0x38}, 4, 0},
    {0x64, (uint8_t []){0x38, 0x0a, 0x73, 0x16, 0x13, 0x56}, 6, 0},
    {0x66, (uint8_t []){0x38, 0x0b, 0x73, 0x17, 0x13, 0x56}, 6, 0},
    {0x68, (uint8_t []){0x00, 0x0B, 0x22, 0x0B, 0x22, 0x1C, 0x1C}, 7, 0},
    {0x69, (uint8_t []){0x00, 0x0B, 0x26, 0x0B, 0x26, 0x1C, 0x1C}, 7, 0},
    {0x6A, (uint8_t []){0x15, 0x00}, 2, 0},
    {
        0x6E, (uint8_t [])
        {
            0x08, 0x02, 0x1a, 0x00, 0x12, 0x12, 0x11, 0x11, 0x14, 0x14, 0x13, 0x13, 0x04, 0x19, 0x1e, 0x1d,
            0x1d, 0x1e, 0x19, 0x04, 0x0b, 0x0b, 0x0c, 0x0c, 0x09, 0x09, 0x0a, 0x0a, 0x00, 0x1a, 0x01, 0x07
        },
        32, 0
    },
    {0x6C, (uint8_t []){0xCC, 0x0C, 0xCC, 0x84, 0xCC, 0x04, 0x50}, 7, 0},
    {0x7D, (uint8_t []){0x72}, 1, 0},
    {0x70, (uint8_t []){0x02, 0x03, 0x09, 0x07, 0x09, 0x03, 0x09, 0x07, 0x09, 0x03}, 10, 0},
    {0x90, (uint8_t []){0x06, 0x06, 0x05, 0x06}, 4, 0},
    {0x93, (uint8_t []){0x45, 0xFF, 0x00}, 3, 0},
    {0xC3, (uint8_t []){0x15}, 1, 0},
    {0xC4, (uint8_t []){0x36}, 1, 0},
    {0xC9, (uint8_t []){0x3d}, 1, 0},
    {0xF0, (uint8_t []){0x47, 0x07, 0x0A, 0x0A, 0x00, 0x29}, 6, 0},
    {0xF2, (uint8_t []){0x47, 0x07, 0x0a, 0x0A, 0x00, 0x29}, 6, 0},
    {0xF1, (uint8_t []){0x42, 0x91, 0x10, 0x2D, 0x2F, 0x6F}, 6, 0},
    {0xF3, (uint8_t []){0x42, 0x91, 0x10, 0x2D, 0x2F, 0x6F}, 6, 0},
    {0xF9, (uint8_t []){0x30}, 1, 0},
    {0xBE, (uint8_t []){0x11}, 1, 0},
    {0xFB, (uint8_t []){0x00, 0x00}, 2, 0},
    {0x11, (uint8_t []){0x00}, 0, 1000},
};

static esp_err_t panel_gc9b71_init(esp_lcd_panel_t *panel)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;
    const gc9b71_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9b71->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_COLMOD, (uint8_t[]) {
        gc9b71->colmod_val,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (gc9b71->init_cmds) {
        init_cmds = gc9b71->init_cmds;
        init_cmds_size = gc9b71->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(gc9b71_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            gc9b71->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            gc9b71->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG,
                            "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_gc9b71_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = gc9b71->io;

    x_start += gc9b71->x_gap;
    x_end += gc9b71->x_gap;
    y_start += gc9b71->y_gap;
    y_end += gc9b71->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * gc9b71->fb_bits_per_pixel / 8;
    ESP_RETURN_ON_ERROR(tx_color(gc9b71, io, LCD_CMD_RAMWR, color_data, len), TAG, "send color failed");

    return ESP_OK;
}

static esp_err_t panel_gc9b71_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;
    int command = 0;
    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9b71_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;
    if (mirror_x) {
        gc9b71->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        gc9b71->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        gc9b71->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        gc9b71->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9b71->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9b71_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;
    if (swap_axes) {
        gc9b71->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        gc9b71->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9b71->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t panel_gc9b71_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    gc9b71->x_gap = x_gap;
    gc9b71->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_gc9b71_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gc9b71_panel_t *gc9b71 = __containerof(panel, gc9b71_panel_t, base);
    esp_lcd_panel_io_handle_t io = gc9b71->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(tx_param(gc9b71, io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
