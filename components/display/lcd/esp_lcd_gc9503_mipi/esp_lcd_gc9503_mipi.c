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
#include "esp_lcd_gc9503_mipi.h"

#define GC9503_MIPI_CMD_GS_BIT       (1 << 0)
#define GC9503_MIPI_CMD_SS_BIT       (1 << 1)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const gc9503_mipi_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} gc9503_panel_t;

static const char *TAG = "gc9503_mipi";

static esp_err_t panel_gc9503_del(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_init(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_gc9503_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_gc9503_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_gc9503_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_gc9503_mipi(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                        esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_GC9503_MIPI_VER_MAJOR, ESP_LCD_GC9503_MIPI_VER_MINOR,
             ESP_LCD_GC9503_MIPI_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    gc9503_mipi_vendor_config_t *vendor_config = (gc9503_mipi_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)calloc(1, sizeof(gc9503_panel_t));
    ESP_RETURN_ON_FALSE(gc9503, ESP_ERR_NO_MEM, TAG, "no mem for gc9503 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        gc9503->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        gc9503->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    gc9503->io = io;
    gc9503->init_cmds = vendor_config->init_cmds;
    gc9503->init_cmds_size = vendor_config->init_cmds_size;
    gc9503->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gc9503->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    gc9503->del = panel_handle->del;
    gc9503->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_gc9503_del;
    panel_handle->init = panel_gc9503_init;
    panel_handle->reset = panel_gc9503_reset;
    panel_handle->mirror = panel_gc9503_mirror;
    panel_handle->invert_color = panel_gc9503_invert_color;
    panel_handle->disp_on_off = panel_gc9503_disp_on_off;
    panel_handle->user_data = gc9503;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new gc9503 panel @%p", gc9503);

    return ESP_OK;

err:
    if (gc9503) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gc9503);
    }
    return ret;
}

static const gc9503_mipi_lcd_init_cmd_t vendor_specific_init_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xF0, (uint8_t []){0x55, 0xAA, 0x52, 0x08, 0x00}, 5, 0},
    {0xF6, (uint8_t []){0x5A, 0x87}, 2, 0},
    {0xC1, (uint8_t []){0x3F}, 1, 0},
    {0xC2, (uint8_t []){0x0E}, 1, 0},
    {0xC6, (uint8_t []){0xF8}, 1, 0},
    {0xC9, (uint8_t []){0x10}, 1, 0},
    {0xCD, (uint8_t []){0x25}, 1, 0},
    {0x86, (uint8_t []){0x99, 0xA3, 0xA3, 0x11}, 4, 0},
    {0x87, (uint8_t []){0x04, 0x03, 0x66}, 3, 0},
    {0xAC, (uint8_t []){0x45}, 1, 0},
    {0xF8, (uint8_t []){0x8A}, 1, 0},
    {0xA7, (uint8_t []){0x47}, 1, 0},
    {0xA0, (uint8_t []){0xDD}, 1, 0},
    {0xFA, (uint8_t []){0x08, 0x08, 0x00, 0x04}, 4, 0},
    {0x71, (uint8_t []){0x48}, 1, 0},
    {0x72, (uint8_t []){0x48}, 1, 0},
    {0x73, (uint8_t []){0x00, 0x44}, 2, 0},
    {0xA3, (uint8_t []){0xEE}, 1, 0},
    {0xFD, (uint8_t []){0x28, 0x3C, 0x00}, 3, 0},
    {0x97, (uint8_t []){0xEE}, 1, 0},
    {0x83, (uint8_t []){0x93}, 1, 0},
    {0x9A, (uint8_t []){0x4A}, 1, 0},
    {0x9B, (uint8_t []){0x22}, 1, 0},
    {0x82, (uint8_t []){0x00, 0x00}, 2, 0},
    {0x80, (uint8_t []){0x28}, 1, 0},     // If OTP burned in module, mask this value
    {0xB1, (uint8_t []){0x10}, 1, 0},
    {0x7A, (uint8_t []){0x13, 0x1A}, 2, 0},
    {0x7B, (uint8_t []){0x13, 0x1A}, 2, 0},
    {0x6D, (uint8_t []){0x1E, 0x00, 0x09, 0x0F, 0x01, 0x1F, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1F, 0x08, 0x10, 0x0A, 0x00, 0x1E}, 32, 0},
    {0x64, (uint8_t []){0x18, 0x07, 0x03, 0x21, 0x03, 0x03, 0x18, 0x06, 0x03, 0x22, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A}, 16, 0},
    {0x67, (uint8_t []){0x18, 0x05, 0x03, 0x23, 0x03, 0x03, 0x18, 0x04, 0x03, 0x24, 0x03, 0x03, 0x7A, 0x7A, 0x7A, 0x7A}, 16, 0},
    {0x60, (uint8_t []){0x18, 0x08, 0x7A, 0x7A, 0x18, 0x00, 0x7A, 0x7A}, 8, 0},
    {0x63, (uint8_t []){0x18, 0x00, 0x7A, 0x7A, 0x18, 0x07, 0x7A, 0x7A}, 8, 0},
    {0x69, (uint8_t []){0x14, 0x22, 0x14, 0x22, 0x44, 0x22, 0x08}, 7, 0},
    {0xD1, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0xD2, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0xD3, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0xD4, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0xD5, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0xD6, (uint8_t []){0x00, 0x00, 0x00, 0x24, 0x00, 0x5A, 0x00, 0x7E, 0x00, 0x9D, 0x00, 0xCA, 0x00, 0xEF, 0x01, 0x2A, 0x01, 0x5E, 0x01, 0xAD, 0x01, 0xEE, 0x02, 0x55, 0x02, 0xA2, 0x02, 0xA5, 0x02, 0xEA, 0x03, 0x40, 0x03, 0x70, 0x03, 0xA4, 0x03, 0xB8, 0x03, 0xCA, 0x03, 0xD9, 0x03, 0xEA, 0x03, 0xEF, 0x03, 0xFA, 0x03, 0xFE, 0x03, 0xFF}, 52, 0},
    {0x11, (uint8_t []){0x00}, 1, 120},
    {0x29, (uint8_t []){0x00}, 1, 20},
};

static esp_err_t panel_gc9503_del(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;

    // Delete MIPI DPI panel
    ESP_RETURN_ON_ERROR(gc9503->del(panel), TAG, "del gc9503 panel failed");
    if (gc9503->reset_gpio_num >= 0) {
        gpio_reset_pin(gc9503->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del gc9503 panel @%p", gc9503);
    free(gc9503);

    return ESP_OK;
}

static esp_err_t panel_gc9503_init(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;
    const gc9503_mipi_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    bool is_cmd_overwritten = false;

    uint8_t ID[3];
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0x04, ID, 3), TAG, "read ID failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gc9503->madctl_val,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (gc9503->init_cmds) {
        init_cmds = gc9503->init_cmds;
        init_cmds_size = gc9503->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(gc9503_mipi_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                gc9503->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
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

    ESP_RETURN_ON_ERROR(gc9503->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_gc9503_reset(esp_lcd_panel_t *panel)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;

    // Perform hardware reset
    if (gc9503->reset_gpio_num >= 0) {
        gpio_set_level(gc9503->reset_gpio_num, !gc9503->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(gc9503->reset_gpio_num, gc9503->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gc9503->reset_gpio_num, !gc9503->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_gc9503_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;
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

static esp_err_t panel_gc9503_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;
    uint8_t madctl_val = gc9503->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x) {
        madctl_val |= GC9503_MIPI_CMD_GS_BIT;
    } else {
        madctl_val &= ~GC9503_MIPI_CMD_GS_BIT;
    }
    if (mirror_y) {
        madctl_val |= GC9503_MIPI_CMD_SS_BIT;
    } else {
        madctl_val &= ~GC9503_MIPI_CMD_SS_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    gc9503->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_gc9503_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gc9503_panel_t *gc9503 = (gc9503_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = gc9503->io;
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
