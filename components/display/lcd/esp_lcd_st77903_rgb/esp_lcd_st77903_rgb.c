/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>

#include "soc/soc_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "esp_lcd_st77903_rgb.h"

#define ST77903_CMD_BPC             (0xB5)
#define ST77903_CMD_DISCN           (0xB6)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // Save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // Save current value of LCD_CMD_COLMOD register
    uint16_t hor_res;
    uint16_t ver_res;
    const st77903_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct {
        unsigned int enable_io_multiplex: 1;
        unsigned int display_on_off_use_cmd: 1;
        unsigned int reset_level: 1;
        unsigned int mirror_by_cmd: 1;
    } flags;
    // To save the original functions of RGB panel
    esp_err_t (*init)(esp_lcd_panel_t *panel);
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*reset)(esp_lcd_panel_t *panel);
    esp_err_t (*mirror)(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
    esp_err_t (*disp_on_off)(esp_lcd_panel_t *panel, bool on_off);
} st77903_panel_t;

static const char *TAG = "st77903";

static esp_err_t panel_st77903_rgb_send_init_cmds(st77903_panel_t *st77903);

static esp_err_t panel_st77903_rgb_init(esp_lcd_panel_t *panel);
static esp_err_t panel_st77903_rgb_del(esp_lcd_panel_t *panel);
static esp_err_t panel_st77903_rgb_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_st77903_rgb_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_st77903_rgb_disp_on_off(esp_lcd_panel_t *panel, bool off);

esp_err_t esp_lcd_new_panel_st77903_rgb(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                        esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    st77903_vendor_config_t *vendor_config = (st77903_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` is necessary");
    ESP_RETURN_ON_FALSE(!vendor_config->flags.enable_io_multiplex || !vendor_config->flags.mirror_by_cmd,
                        ESP_ERR_INVALID_ARG, TAG, "`mirror_by_cmd` and `enable_io_multiplex` cannot work together");

    esp_err_t ret = ESP_OK;
    st77903_panel_t *st77903 = (st77903_panel_t *)calloc(1, sizeof(st77903_panel_t));
    ESP_RETURN_ON_FALSE(st77903, ESP_ERR_NO_MEM, TAG, "no mem for st77903 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        st77903->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        st77903->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color element order");
        break;
    }

    st77903->colmod_val = 0;
    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        st77903->colmod_val = 0x05;
        break;
    case 18: // RGB666
        st77903->colmod_val = 0x06;
        break;
    case 24: // RGB888
        st77903->colmod_val = 0x07;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    st77903->io = io;
    st77903->reset_gpio_num = panel_dev_config->reset_gpio_num;
    st77903->init_cmds = vendor_config->init_cmds;
    st77903->init_cmds_size = vendor_config->init_cmds_size;
    st77903->hor_res = vendor_config->rgb_config->timings.h_res;
    st77903->ver_res = vendor_config->rgb_config->timings.v_res;
    st77903->flags.enable_io_multiplex = vendor_config->flags.enable_io_multiplex;
    st77903->flags.display_on_off_use_cmd = (vendor_config->rgb_config->disp_gpio_num >= 0) ? 0 : 1;
    st77903->flags.reset_level = panel_dev_config->flags.reset_active_high;
    st77903->flags.mirror_by_cmd = vendor_config->flags.mirror_by_cmd;

    if (st77903->flags.enable_io_multiplex) {
        // Reset st77903
        if (st77903->reset_gpio_num >= 0) {
            gpio_set_level(st77903->reset_gpio_num, !st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(st77903->reset_gpio_num, st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(st77903->reset_gpio_num, !st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(120));
        } else { // Perform software reset
            ESP_GOTO_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), err, TAG, "send command failed");
            vTaskDelay(pdMS_TO_TICKS(120));
        }

        /**
         * In order to enable the 3-wire SPI interface pins (such as SDA and SCK) to share other pins of the RGB interface
         * (such as HSYNC) and save GPIOs, We need to send LCD initialization commands via the 3-wire SPI interface before
         * `esp_lcd_new_rgb_panel()` is called.
         */
        ESP_GOTO_ON_ERROR(panel_st77903_rgb_send_init_cmds(st77903), err, TAG, "send init commands failed");
        // After sending the initialization commands, the 3-wire SPI interface can be deleted
        ESP_GOTO_ON_ERROR(esp_lcd_panel_io_del(io), err, TAG, "delete panel IO failed");
        st77903->io = NULL;
        ESP_LOGD(TAG, "delete panel IO");
    }

    // Create RGB panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(vendor_config->rgb_config, ret_panel), err, TAG, "create RGB panel failed");
    ESP_LOGD(TAG, "new RGB panel @%p", ret_panel);

    // Save the original functions of RGB panel
    st77903->init = (*ret_panel)->init;
    st77903->del = (*ret_panel)->del;
    st77903->reset = (*ret_panel)->reset;
    st77903->mirror = (*ret_panel)->mirror;
    st77903->disp_on_off = (*ret_panel)->disp_on_off;
    // Overwrite the functions of RGB panel
    (*ret_panel)->init = panel_st77903_rgb_init;
    (*ret_panel)->del = panel_st77903_rgb_del;
    (*ret_panel)->reset = panel_st77903_rgb_reset;
    (*ret_panel)->mirror = panel_st77903_rgb_mirror;
    (*ret_panel)->disp_on_off = panel_st77903_rgb_disp_on_off;
    (*ret_panel)->user_data = st77903;

    ESP_LOGD(TAG, "new st77903 panel @%p", st77903);

    return ESP_OK;

err:
    if (st77903) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        if (*ret_panel) {
            esp_lcd_panel_del(*ret_panel);
        }
        free(st77903);
    }
    return ret;
}

const static st77903_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0xF0, (uint8_t []){0xC3}, 1, 0},
    {0xF0, (uint8_t []){0x96}, 1, 0},
    {0xF0, (uint8_t []){0xA5}, 1, 0},
    {0xC1, (uint8_t []){0x66, 0x07, 0x4B, 0x0B}, 4, 0},
    {0xC2, (uint8_t []){0x66, 0x07, 0x4B, 0x0B}, 4, 0},
    {0xC3, (uint8_t []){0x44, 0x04, 0x44, 0x04}, 4, 0},
    {0xC4, (uint8_t []){0x44, 0x04, 0x44, 0x04}, 4, 0},
    {0xC5, (uint8_t []){0xBF, 0x80}, 2, 0},
    {0xD6, (uint8_t []){0x09}, 1, 0},
    {0xD7, (uint8_t []){0x04}, 1, 0},
    {0xE0, (uint8_t []){0xD2, 0x09, 0x0C, 0x07, 0x06, 0x23, 0x2E, 0x43, 0x46, 0x17, 0x13, 0x13, 0x2D, 0x33}, 14, 0},
    {0xE1, (uint8_t []){0xD2, 0x09, 0x0C, 0x07, 0x05, 0x23, 0x2E, 0x33, 0x46, 0x17, 0x13, 0x13, 0x2D, 0x33}, 14, 0},
    {0xE5, (uint8_t []){0x5A, 0xF5, 0x64, 0x33, 0x22, 0x25, 0x10, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77}, 14, 0},
    {0xE6, (uint8_t []){0x5A, 0xF5, 0x64, 0x33, 0x22, 0x25, 0x10, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77}, 14, 0},
    {0xEC, (uint8_t []){0x00, 0x55, 0x00, 0x00, 0x00, 0x88}, 6, 0},
    {0xB2, (uint8_t []){0x09}, 1, 0},
    {0xB3, (uint8_t []){0x01}, 1, 0},
    {0xB4, (uint8_t []){0x01}, 1, 0},
    {0xA4, (uint8_t []){0xC0, 0x63}, 2, 0},
    {0xA5, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x2A, 0xBA, 0x02}, 9, 0},
    {0xA6, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x2A, 0xBA, 0x02}, 9, 0},
    {0xBA, (uint8_t []){0x5A, 0x09, 0x43, 0x00, 0x23, 0x01, 0x00}, 7, 0},
    {0xBB, (uint8_t []){0x00, 0x24, 0x00, 0x25, 0x83, 0x87, 0x18, 0x00}, 8, 0},
    {0xBC, (uint8_t []){0x00, 0x24, 0x00, 0x25, 0x83, 0x87, 0x18, 0x00}, 8, 0},
    {0xBD, (uint8_t []){0x33, 0x44, 0xFF, 0xFF, 0x98, 0xA7, 0x7A, 0x89, 0x52, 0xFF, 0x06}, 11, 0},
    {0xED, (uint8_t []){0xC3}, 1, 0},
    {0xE4, (uint8_t []){0x40, 0x0F, 0x2F}, 3, 0},
    {0xA0, (uint8_t []){0x00, 0x06, 0x06}, 3, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x21, (uint8_t []){0x00}, 0, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 120},

    // {0xb0, (uint8_t []){0xa5}, 1, 0},               /* This part of the parameters can be used for screen self-test */
    // {0xcc, (uint8_t []){0x40, 0x00, 0x3f, 0x00, 0x14, 0x14, 0x20, 0x20, 0x03}, 9, 0},
};

static esp_err_t panel_st77903_rgb_send_init_cmds(st77903_panel_t *st77903)
{
    esp_lcd_panel_io_handle_t io = st77903->io;

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t []) {
        0xc3
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t []) {
        0x96
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xf0, (uint8_t []) {
        0xa5
    }, 1), TAG, "Write cmd failed");
    uint8_t NL = (st77903->ver_res >> 1) - 1;
    uint8_t NC = (st77903->hor_res >> 3) - 1;
    // Set Resolution
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, ST77903_CMD_DISCN, (uint8_t []) {
        NL, NC
    }, 2), TAG, "Write cmd failed");
    // Set color format
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        st77903->madctl_val
    }, 1), TAG, "Write cmd failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t []) {
        st77903->colmod_val
    }, 1), TAG, "Write cmd failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    const st77903_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (st77903->init_cmds) {
        init_cmds = st77903->init_cmds;
        init_cmds_size = st77903->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(st77903_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    bool is_cmd_conflicting = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            st77903->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            st77903->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case ST77903_CMD_BPC:
            if ((init_cmds[i].data_bytes >= 2) && ((((uint8_t *)init_cmds[i].data)[0] != NL) ||
                                                   (((uint8_t *)init_cmds[i].data)[1] != NC))) {
                is_cmd_conflicting = true;
            }
            break;
        default:
            is_cmd_overwritten = false;
            is_cmd_conflicting = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence",
                     init_cmds[i].cmd);
        } else if (is_cmd_conflicting) {
            ESP_LOGE(TAG, "The %02Xh command conflicts with the internal, please remove it from external initialization sequence",
                     init_cmds[i].cmd);
        }

        // Only send the command if it is not conflicted
        if (!is_cmd_conflicting) {
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes),
                                TAG, "send command failed");
            vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
        }
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t panel_st77903_rgb_init(esp_lcd_panel_t *panel)
{
    st77903_panel_t *st77903 = (st77903_panel_t *)panel->user_data;

    if (!st77903->flags.enable_io_multiplex) {
        ESP_RETURN_ON_ERROR(panel_st77903_rgb_send_init_cmds(st77903), TAG, "send init commands failed");
    }
    // Init RGB panel
    ESP_RETURN_ON_ERROR(st77903->init(panel), TAG, "init RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_st77903_rgb_del(esp_lcd_panel_t *panel)
{
    st77903_panel_t *st77903 = (st77903_panel_t *)panel->user_data;

    // Delete RGB panel
    ESP_RETURN_ON_ERROR(st77903->del(panel), TAG, "del st77903 panel failed");
    if (st77903->reset_gpio_num >= 0) {
        gpio_reset_pin(st77903->reset_gpio_num);
    }
    free(st77903);
    ESP_LOGD(TAG, "del st77903 panel @%p", st77903);
    return ESP_OK;
}

static esp_err_t panel_st77903_rgb_reset(esp_lcd_panel_t *panel)
{
    st77903_panel_t *st77903 = (st77903_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77903->io;

    if (!st77903->flags.enable_io_multiplex) {
        // Perform hardware reset
        if (st77903->reset_gpio_num >= 0) {
            gpio_set_level(st77903->reset_gpio_num, !st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(st77903->reset_gpio_num, st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(10));
            gpio_set_level(st77903->reset_gpio_num, !st77903->flags.reset_level);
            vTaskDelay(pdMS_TO_TICKS(120));
        } else if (io) { // Perform software reset
            ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }
    // Reset RGB panel
    ESP_RETURN_ON_ERROR(st77903->reset(panel), TAG, "reset RGB panel failed");

    return ESP_OK;
}

static esp_err_t panel_st77903_rgb_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    st77903_panel_t *st77903 = (st77903_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77903->io;

    if (st77903->flags.mirror_by_cmd) {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control mirror through LCD command
        if (mirror_x) {
            st77903->madctl_val |= LCD_CMD_MH_BIT;
        } else {
            st77903->madctl_val &= ~LCD_CMD_MH_BIT;
        }
        if (mirror_y) {
            st77903->madctl_val |= LCD_CMD_ML_BIT;
        } else {
            st77903->madctl_val &= ~LCD_CMD_ML_BIT;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
            st77903->madctl_val,
        }, 1), TAG, "send command failed");;
    } else {
        // Control mirror through RGB panel
        ESP_RETURN_ON_ERROR(st77903->mirror(panel, mirror_x, mirror_y), TAG, "RGB panel mirror failed");
    }
    return ESP_OK;
}
static esp_err_t panel_st77903_rgb_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    st77903_panel_t *st77903 = (st77903_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = st77903->io;
    int command = 0;

    if (st77903->flags.display_on_off_use_cmd) {
        ESP_RETURN_ON_FALSE(io, ESP_FAIL, TAG, "Panel IO is deleted, cannot send command");
        // Control display on/off through LCD command
        if (on_off) {
            command = LCD_CMD_DISPON;
        } else {
            command = LCD_CMD_DISPOFF;
        }
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    } else {
        // Control display on/off through display control signal
        ESP_RETURN_ON_ERROR(st77903->disp_on_off(panel, on_off), TAG, "RGB panel disp_on_off failed");
    }
    return ESP_OK;
}
