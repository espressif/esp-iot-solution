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
#include "esp_lcd_er88577.h"

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const er88577_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} er88577_panel_t;

static const char *TAG = "er88577";

static esp_err_t panel_er88577_del(esp_lcd_panel_t *panel);
static esp_err_t panel_er88577_init(esp_lcd_panel_t *panel);
static esp_err_t panel_er88577_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_er88577_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_er88577_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_er88577(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_ER88577_VER_MAJOR, ESP_LCD_ER88577_VER_MINOR,
             ESP_LCD_ER88577_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    er88577_vendor_config_t *vendor_config = (er88577_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    er88577_panel_t *er88577 = (er88577_panel_t *)calloc(1, sizeof(er88577_panel_t));
    ESP_RETURN_ON_FALSE(er88577, ESP_ERR_NO_MEM, TAG, "no mem for er88577 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        er88577->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        er88577->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        er88577->colmod_val = 0x55;
        break;
    case 18: // RGB666
        er88577->colmod_val = 0x66;
        break;
    case 24: // RGB888
        er88577->colmod_val = 0x77;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    er88577->io = io;
    er88577->init_cmds = vendor_config->init_cmds;
    er88577->init_cmds_size = vendor_config->init_cmds_size;
    er88577->reset_gpio_num = panel_dev_config->reset_gpio_num;
    er88577->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    er88577->del = panel_handle->del;
    er88577->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_er88577_del;
    panel_handle->init = panel_er88577_init;
    panel_handle->reset = panel_er88577_reset;
    panel_handle->invert_color = panel_er88577_invert_color;
    panel_handle->disp_on_off = panel_er88577_disp_on_off;
    panel_handle->user_data = er88577;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new er88577 panel @%p", er88577);

    return ESP_OK;

err:
    if (er88577) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(er88577);
    }
    return ret;
}

static const er88577_lcd_init_cmd_t vendor_specific_init_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xE0, (uint8_t[]){0xAB, 0xBA}, 2, 0},
    {0xE1, (uint8_t[]){0xBA, 0xAB}, 2, 0},
    {0xB1, (uint8_t[]){0x10, 0x01, 0x47, 0xFF}, 4, 0},
    {0xB2, (uint8_t[]){0x0C, 0x14, 0x04, 0x50, 0x50, 0x14}, 6, 0},
    {0xB3, (uint8_t[]){0x56, 0x53, 0x00}, 3, 0},
    {0xB4, (uint8_t[]){0x33, 0x30, 0x04}, 3, 0},
    {0xB6, (uint8_t[]){0xB0, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00}, 7, 0},
    {0xB8, (uint8_t[]){0x05, 0x12, 0x29, 0x49, 0x40}, 5, 0},
    {
        0xB9, (uint8_t[])
        {
            0x7C, 0x61, 0x4F, 0x42, 0x3E, 0x2D, 0x31, 0x1A, 0x33, 0x33, 0x33, 0x52, 0x40, 0x47, 0x38, 0x34, 0x26, 0x0E, 0x06,
            0x7C, 0x61, 0x4F, 0x42, 0x3E, 0x2D, 0x31, 0x1A, 0x33, 0x33, 0x33, 0x52, 0x40, 0x47, 0x38, 0x34, 0x26, 0x0E, 0x06
        }, 38, 0
    },
    {0xC0, (uint8_t[]){0xCC, 0x76, 0x12, 0x34, 0x44, 0x44, 0x44, 0x44, 0x98, 0x04, 0x98, 0x04, 0x0F, 0x00, 0x00, 0xC1}, 16, 0},
    {0xC1, (uint8_t[]){0x54, 0x94, 0x02, 0x85, 0x9F, 0x00, 0x6F, 0x00, 0x54, 0x00}, 10, 0},
    {0xC2, (uint8_t[]){0x17, 0x09, 0x08, 0x89, 0x08, 0x11, 0x22, 0x20, 0x44, 0xFF, 0x18, 0x00}, 12, 0},
    {0xC3, (uint8_t[]){0x87, 0x47, 0x05, 0x05, 0x1C, 0x1C, 0x1D, 0x1D, 0x02, 0x1E, 0x1E, 0x1F, 0x1F, 0x0F, 0x0F, 0x0D, 0x0D, 0x13, 0x13, 0x11, 0x11, 0x24}, 22, 0},
    {0xC4, (uint8_t[]){0x06, 0x06, 0x04, 0x04, 0x1C, 0x1C, 0x1D, 0x1D, 0x02, 0x1E, 0x1E, 0x1F, 0x1F, 0x0E, 0x0E, 0x0C, 0x0C, 0x12, 0x12, 0x10, 0x10, 0x24}, 22, 0},
    {0xC6, (uint8_t[]){0x25, 0x25}, 2, 0},
    {0xC8, (uint8_t[]){0x21, 0x00, 0x31, 0x42, 0x34, 0x16}, 6, 0},
    {0xCA, (uint8_t[]){0xCB, 0x43}, 2, 0},
    {0xCD, (uint8_t[]){0x0E, 0x4B, 0x4B, 0x20, 0x19, 0x6B, 0x06, 0xB3}, 8, 0},
    {0xD2, (uint8_t[]){0xE1, 0x2B, 0x38, 0x08}, 4, 0},
    {0xD4, (uint8_t[]){0x00, 0x01, 0x00, 0x0E, 0x04, 0x44, 0x08, 0x10, 0x00, 0x00, 0x00}, 11, 0},
    {0xE6, (uint8_t[]){0x80, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 8, 0},
    {0xF0, (uint8_t[]){0x12, 0x03, 0x20, 0x00, 0xFF}, 5, 0},
    {0xF3, (uint8_t[]){0x00}, 1, 0},
    {0x11, NULL, 0, 750},
    {0x29, NULL, 0, 100},
};

static esp_err_t panel_er88577_del(esp_lcd_panel_t *panel)
{
    er88577_panel_t *er88577 = (er88577_panel_t *)panel->user_data;

    if (er88577->reset_gpio_num >= 0) {
        gpio_reset_pin(er88577->reset_gpio_num);
    }
    // Delete MIPI DPI panel
    er88577->del(panel);
    ESP_LOGD(TAG, "del er88577 panel @%p", er88577);
    free(er88577);

    return ESP_OK;
}

static esp_err_t panel_er88577_init(esp_lcd_panel_t *panel)
{
    er88577_panel_t *er88577 = (er88577_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = er88577->io;
    const er88577_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    uint8_t ID[3];
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0x04, ID, 3), TAG, "read ID failed");
    ESP_LOGI(TAG, "LCD ID: %02X %02X %02X", ID[0], ID[1], ID[2]);

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        er88577->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        er88577->colmod_val,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (er88577->init_cmds) {
        init_cmds = er88577->init_cmds;
        init_cmds_size = er88577->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(er88577_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                er88577->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                er88577->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
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

    ESP_RETURN_ON_ERROR(er88577->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_er88577_reset(esp_lcd_panel_t *panel)
{
    er88577_panel_t *er88577 = (er88577_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = er88577->io;

    // Perform hardware reset
    if (er88577->reset_gpio_num >= 0) {
        gpio_set_level(er88577->reset_gpio_num, er88577->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(er88577->reset_gpio_num, !er88577->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_er88577_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    er88577_panel_t *er88577 = (er88577_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = er88577->io;
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

static esp_err_t panel_er88577_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    er88577_panel_t *er88577 = (er88577_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = er88577->io;
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
