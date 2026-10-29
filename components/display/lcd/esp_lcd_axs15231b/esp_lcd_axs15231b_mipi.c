/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
#include "esp_lcd_axs15231b.h"

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const axs15231b_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    bool init_in_command_mode;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // Save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} axs15231b_mipi_panel_t;

static const char *TAG = "axs15231b_mipi";

static esp_err_t panel_axs15231b_mipi_del(esp_lcd_panel_t *panel);
static esp_err_t panel_axs15231b_mipi_init(esp_lcd_panel_t *panel);
static esp_err_t panel_axs15231b_mipi_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_axs15231b_mipi_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_axs15231b_mipi_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_axs15231b_mipi_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_axs15231b_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                           esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_AXS15231B_VER_MAJOR, ESP_LCD_AXS15231B_VER_MINOR,
             ESP_LCD_AXS15231B_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    axs15231b_vendor_config_t *vendor_config = (axs15231b_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus,
                        ESP_ERR_INVALID_ARG, TAG, "invalid vendor config");

    esp_err_t ret = ESP_OK;
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)calloc(1, sizeof(axs15231b_mipi_panel_t));
    ESP_RETURN_ON_FALSE(axs15231b_mipi, ESP_ERR_NO_MEM, TAG, "no memory for axs15231b MIPI panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        axs15231b_mipi->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        axs15231b_mipi->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        axs15231b_mipi->colmod_val = 0x55;
        break;
    case 18: // RGB666
        axs15231b_mipi->colmod_val = 0x66;
        break;
    case 24: // RGB888
        axs15231b_mipi->colmod_val = 0x77;
        break;
    }

    axs15231b_mipi->io = io;
    axs15231b_mipi->init_cmds = vendor_config->init_cmds;
    axs15231b_mipi->init_cmds_size = vendor_config->init_cmds_size;
    axs15231b_mipi->init_in_command_mode = vendor_config->init_in_command_mode;
    axs15231b_mipi->reset_gpio_num = panel_dev_config->reset_gpio_num;
    axs15231b_mipi->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    axs15231b_mipi->del = panel_handle->del;
    axs15231b_mipi->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_axs15231b_mipi_del;
    panel_handle->init = panel_axs15231b_mipi_init;
    panel_handle->reset = panel_axs15231b_mipi_reset;
    panel_handle->mirror = panel_axs15231b_mipi_mirror;
    panel_handle->invert_color = panel_axs15231b_mipi_invert_color;
    panel_handle->disp_on_off = panel_axs15231b_mipi_disp_on_off;
    panel_handle->user_data = axs15231b_mipi;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new axs15231b MIPI panel @%p", axs15231b_mipi);

    return ESP_OK;

err:
    if (axs15231b_mipi) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(axs15231b_mipi);
    }
    return ret;
}

static const axs15231b_lcd_init_cmd_t vendor_specific_init_default[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5}, 8, 0},
    {0xA0, (uint8_t []){0x00, 0x70, 0x00, 0x02, 0x00, 0x00, 0x08, 0x3F, 0x20, 0x05, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00}, 17, 0},
    {0xA2, (uint8_t []){0x20, 0x02, 0x0A, 0x0D, 0x32, 0x0A, 0x32, 0xC0, 0xF0, 0x38, 0x7F, 0x7F, 0x7F, 0x20, 0xF8, 0x10, 0x02, 0xFF, 0xFF, 0xF0, 0x90, 0x01, 0x32, 0xA0, 0x91, 0xC0, 0x20, 0x7F, 0xFF, 0x00, 0x04}, 31, 0},
    {0xD0, (uint8_t []){0xC0, 0xF0, 0x32, 0x24, 0x08, 0x05, 0x10, 0x0B, 0x60, 0x21, 0xC2, 0x40, 0x20, 0x02, 0xAA, 0x03, 0x10, 0x12, 0x60, 0x15, 0x1E, 0x51, 0x15, 0x00, 0xAC, 0x00, 0x00, 0x03, 0x0D, 0x12}, 30, 0},
    {0xA3, (uint8_t []){0xA0, 0x06, 0xA9, 0x00, 0x08, 0x01, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x55, 0x55}, 22, 0},
    {0xC1, (uint8_t []){0x31, 0x04, 0x02, 0x02, 0x71, 0x05, 0x24, 0x55, 0x02, 0x00, 0x41, 0x01, 0x53, 0xFF, 0xFF, 0xFF, 0x4F, 0x52, 0x00, 0x4F, 0x52, 0x00, 0x45, 0x3B, 0x0B, 0x02, 0x0D, 0x00, 0xFF, 0x40}, 30, 0},
    {0xC3, (uint8_t []){0x00, 0x00, 0x00, 0x50, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x01}, 11, 0},
    {0xC4, (uint8_t []){0x00, 0x24, 0x33, 0x80, 0x60, 0xEA, 0x64, 0x32, 0xC8, 0x64, 0xC8, 0x32, 0x90, 0x90, 0x11, 0x06, 0xDC, 0xFA, 0x00, 0x00, 0x80, 0xFE, 0x10, 0x10, 0x00, 0x0A, 0x0A, 0x44, 0x50}, 29, 0},
    {0xC5, (uint8_t []){0x18, 0x00, 0x00, 0x03, 0xFE, 0x18, 0x38, 0x40, 0x10, 0x10, 0x88, 0xDE, 0x0D, 0x08, 0x0F, 0x0F, 0x01, 0x18, 0x38, 0x40, 0x10, 0x10, 0x00}, 23, 0},
    {0xC6, (uint8_t []){0x05, 0x0A, 0x05, 0x0A, 0x00, 0xE0, 0x2E, 0x0B, 0x12, 0x22, 0x12, 0x22, 0x01, 0x00, 0x00, 0x02, 0x6A, 0x18, 0xC8, 0x22}, 20, 0},
    {0xC7, (uint8_t []){0x50, 0x36, 0x28, 0x00, 0xA2, 0x80, 0x8F, 0x00, 0x80, 0xFF, 0x07, 0x11, 0x9C, 0x6F, 0xFF, 0x26, 0x0C, 0x0D, 0x0E, 0x0F}, 20, 0},
    {0xC9, (uint8_t []){0x33, 0x44, 0x44, 0x01}, 4, 0},
    {0xCF, (uint8_t []){0x2C, 0x1E, 0x88, 0x58, 0x13, 0x18, 0x56, 0x18, 0x1E, 0x69, 0xF2, 0x00, 0x6F, 0x0F, 0x22, 0xC4, 0x0C, 0x77, 0x22, 0x44, 0xAA, 0x55, 0x04, 0x04, 0x15, 0xA0, 0x08}, 27, 0},
    {0xD5, (uint8_t []){0x1E, 0x1C, 0x82, 0x00, 0x26, 0x03, 0x0C, 0x0E, 0x88, 0xA0, 0xA2, 0x77, 0x08, 0x28, 0x03, 0x41, 0x26, 0x26, 0x03, 0x33, 0x84, 0x00, 0x00, 0x00, 0xC0, 0x53, 0x8C, 0x26, 0x26, 0x00}, 30, 0},
    {0xD6, (uint8_t []){0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x95, 0x00, 0x01, 0x83, 0x0D, 0x0F, 0x88, 0x75, 0x36, 0x20, 0x26, 0x26, 0x03, 0x03, 0x10, 0x10, 0x00, 0x84, 0x51, 0x20, 0x01, 0x00}, 30, 0},
    {0xD7, (uint8_t []){0x1F, 0x01, 0x0F, 0x0D, 0x0B, 0x09, 0x1F, 0x1E, 0x1A, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C, 0x04, 0x00, 0x24, 0x36, 0x1F}, 19, 0},
    {0xD8, (uint8_t []){0x1F, 0x00, 0x0E, 0x0C, 0x0A, 0x08, 0x1F, 0x1E, 0x18, 0x1F, 0x1F, 0x1F}, 12, 0},
    {0xDF, (uint8_t []){0x44, 0x33, 0x4B, 0x69, 0x00, 0x0A, 0x02, 0x90}, 8, 0},
    {0xE0, (uint8_t []){0x22, 0x08, 0x0C, 0x10, 0x06, 0x05, 0x11, 0x1B, 0x44, 0x15, 0x0C, 0x16, 0x13, 0x2A, 0x31, 0x08, 0x00}, 17, 0},
    {0xE1, (uint8_t []){0x3A, 0x08, 0x0C, 0x10, 0x06, 0x05, 0x11, 0x1B, 0x44, 0x15, 0x0C, 0x16, 0x13, 0x2A, 0x31, 0x08, 0x1E}, 17, 0},
    {0xE2, (uint8_t []){0x19, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xD4, 0x22, 0x0B, 0x13, 0x12, 0x2D, 0x32, 0x2F, 0x03}, 17, 0},
    {0xE3, (uint8_t []){0x38, 0x20, 0x0A, 0x11, 0x09, 0x06, 0x11, 0x25, 0xC4, 0x21, 0x0A, 0x12, 0x11, 0x2C, 0x32, 0x2F, 0x27}, 17, 0},
    {0xE4, (uint8_t []){0x19, 0x20, 0x0D, 0x14, 0x0D, 0x08, 0x12, 0x2A, 0xD4, 0x26, 0x0E, 0x15, 0x13, 0x34, 0x39, 0x2F, 0x03}, 17, 0},
    {0xE5, (uint8_t []){0x38, 0x20, 0x0D, 0x13, 0x0D, 0x07, 0x12, 0x29, 0xC4, 0x25, 0x0D, 0x15, 0x12, 0x33, 0x39, 0x2F, 0x27}, 17, 0},
    {0xBB, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8, 0},
    {0x35, (uint8_t []){0x00}, 1, 300},
    {0x11, (uint8_t []){0x00}, 0, 300},
    {0x29, (uint8_t []){0x00}, 0, 200},
};

static esp_err_t panel_axs15231b_mipi_del(esp_lcd_panel_t *panel)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;

    ESP_RETURN_ON_ERROR(axs15231b_mipi->del(panel), TAG, "delete axs15231b MIPI panel failed");
    if (axs15231b_mipi->reset_gpio_num >= 0) {
        gpio_reset_pin(axs15231b_mipi->reset_gpio_num);
    }
    ESP_LOGD(TAG, "delete axs15231b MIPI panel @%p", axs15231b_mipi);
    free(axs15231b_mipi);

    return ESP_OK;
}

static esp_err_t panel_axs15231b_mipi_init(esp_lcd_panel_t *panel)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = axs15231b_mipi->io;
    const axs15231b_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    if (!axs15231b_mipi->init_in_command_mode) {
        ESP_RETURN_ON_ERROR(axs15231b_mipi->init(panel), TAG, "init MIPI DPI panel failed");
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        axs15231b_mipi->madctl_val,
    }, 1), TAG, "send command failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        axs15231b_mipi->colmod_val,
    }, 1), TAG, "send command failed");

    if (axs15231b_mipi->init_cmds) {
        init_cmds = axs15231b_mipi->init_cmds;
        init_cmds_size = axs15231b_mipi->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(axs15231b_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                axs15231b_mipi->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                axs15231b_mipi->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            default:
                is_cmd_overwritten = false;
                break;
            }

            if (is_cmd_overwritten) {
                is_cmd_overwritten = false;
                ESP_LOGW(TAG, "Command %02Xh is overwritten by external initialization sequence",
                         init_cmds[i].cmd);
            }
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    if (axs15231b_mipi->init_in_command_mode) {
        ESP_RETURN_ON_ERROR(axs15231b_mipi->init(panel), TAG, "init MIPI DPI panel failed");
    }

    return ESP_OK;
}

static esp_err_t panel_axs15231b_mipi_reset(esp_lcd_panel_t *panel)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = axs15231b_mipi->io;

    if (axs15231b_mipi->reset_gpio_num >= 0) {
        gpio_set_level(axs15231b_mipi->reset_gpio_num, !axs15231b_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(130));
        gpio_set_level(axs15231b_mipi->reset_gpio_num, axs15231b_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(130));
        gpio_set_level(axs15231b_mipi->reset_gpio_num, !axs15231b_mipi->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(220));
    } else if (io) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_axs15231b_mipi_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = axs15231b_mipi->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    command = invert_color_data ? LCD_CMD_INVON : LCD_CMD_INVOFF;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t panel_axs15231b_mipi_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = axs15231b_mipi->io;
    uint8_t madctl_val = axs15231b_mipi->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (mirror_x) {
        ESP_LOGW(TAG, "mirror x is not supported");
    }

    if (mirror_y) {
        madctl_val |= LCD_CMD_MV_BIT;
    } else {
        madctl_val &= ~LCD_CMD_MV_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    axs15231b_mipi->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_axs15231b_mipi_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    axs15231b_mipi_panel_t *axs15231b_mipi = (axs15231b_mipi_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = axs15231b_mipi->io;
    int command = on_off ? LCD_CMD_DISPON : LCD_CMD_DISPOFF;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
#endif
