/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lcd_ek79007.h"

#define EK79007_PAD_CONTROL     (0xB2)
#define EK79007_DSI_2_LANE      (0x10)
#define EK79007_DSI_4_LANE      (0x00)

#define EK79007_CMD_SHLR_BIT    (1ULL << 0)
#define EK79007_CMD_UPDN_BIT    (1ULL << 1)
#define EK79007_MDCTL_VALUE_DEFAULT   (0x01)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    const ek79007_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} ek79007_panel_t;

static const char *TAG = "ek79007";

static esp_err_t panel_ek79007_send_init_cmds(ek79007_panel_t *ek79007);

static esp_err_t panel_ek79007_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ek79007_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ek79007_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);

esp_err_t esp_lcd_new_panel_ek79007(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_EK79007_VER_MAJOR, ESP_LCD_EK79007_VER_MINOR,
             ESP_LCD_EK79007_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    ek79007_vendor_config_t *vendor_config = (ek79007_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)calloc(1, sizeof(ek79007_panel_t));
    ESP_RETURN_ON_FALSE(ek79007, ESP_ERR_NO_MEM, TAG, "no mem for ek79007 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    ek79007->io = io;
    ek79007->init_cmds = vendor_config->init_cmds;
    ek79007->init_cmds_size = vendor_config->init_cmds_size;
    ek79007->lane_num = vendor_config->mipi_config.lane_num;
    ek79007->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ek79007->flags.reset_level = panel_dev_config->flags.reset_active_high;
    ek79007->madctl_val = EK79007_MDCTL_VALUE_DEFAULT;

    // Create MIPI DPI panel
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, ret_panel), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", *ret_panel);

    // Save the original functions of MIPI DPI panel
    ek79007->del = (*ret_panel)->del;
    ek79007->init = (*ret_panel)->init;
    // Overwrite the functions of MIPI DPI panel
    (*ret_panel)->del = panel_ek79007_del;
    (*ret_panel)->init = panel_ek79007_init;
    (*ret_panel)->reset = panel_ek79007_reset;
    (*ret_panel)->mirror = panel_ek79007_mirror;
    (*ret_panel)->invert_color = panel_ek79007_invert_color;
    (*ret_panel)->user_data = ek79007;
    ESP_LOGD(TAG, "new ek79007 panel @%p", ek79007);

    return ESP_OK;

err:
    if (ek79007) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ek79007);
    }
    return ret;
}

static const ek79007_lcd_init_cmd_t vendor_specific_init_default[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x80, (uint8_t []){0x8B}, 1, 0},
    {0x81, (uint8_t []){0x78}, 1, 0},
    {0x82, (uint8_t []){0x84}, 1, 0},
    {0x83, (uint8_t []){0x88}, 1, 0},
    {0x84, (uint8_t []){0xA8}, 1, 0},
    {0x85, (uint8_t []){0xE3}, 1, 0},
    {0x86, (uint8_t []){0x88}, 1, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
};

static esp_err_t panel_ek79007_send_init_cmds(ek79007_panel_t *ek79007)
{
    esp_lcd_panel_io_handle_t io = ek79007->io;
    const ek79007_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    uint8_t lane_command = EK79007_DSI_2_LANE;
    bool is_cmd_overwritten = false;

    switch (ek79007->lane_num) {
    case 0:
    case 2:
        lane_command = EK79007_DSI_2_LANE;
        break;
    case 4:
        lane_command = EK79007_DSI_4_LANE;
        break;
    default:
        ESP_LOGE(TAG, "Invalid lane number %d", ek79007->lane_num);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, EK79007_PAD_CONTROL, (uint8_t[]) {
        lane_command,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (ek79007->init_cmds) {
        init_cmds = ek79007->init_cmds;
        init_cmds_size = ek79007->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(ek79007_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                ek79007->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
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

    return ESP_OK;
}

static esp_err_t panel_ek79007_del(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;

    // Delete MIPI DPI panel
    ESP_RETURN_ON_ERROR(ek79007->del(panel), TAG, "del ek79007 panel failed");
    if (ek79007->reset_gpio_num >= 0) {
        gpio_reset_pin(ek79007->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del ek79007 panel @%p", ek79007);
    free(ek79007);

    return ESP_OK;
}

static esp_err_t panel_ek79007_init(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;

    ESP_RETURN_ON_ERROR(panel_ek79007_send_init_cmds(ek79007), TAG, "send init commands failed");
    ESP_RETURN_ON_ERROR(ek79007->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_ek79007_reset(esp_lcd_panel_t *panel)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;

    // Perform hardware reset
    if (ek79007->reset_gpio_num >= 0) {
        gpio_set_level(ek79007->reset_gpio_num, ek79007->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ek79007->reset_gpio_num, !ek79007->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(20));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_ek79007_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;
    uint8_t madctl_val = ek79007->madctl_val;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    // Control mirror through LCD command
    if (mirror_x) {
        madctl_val |= EK79007_CMD_SHLR_BIT;
    } else {
        madctl_val &= ~EK79007_CMD_SHLR_BIT;
    }
    if (mirror_y) {
        madctl_val |= EK79007_CMD_UPDN_BIT;
    } else {
        madctl_val &= ~EK79007_CMD_UPDN_BIT;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t []) {
        madctl_val
    }, 1), TAG, "send command failed");
    ek79007->madctl_val = madctl_val;

    return ESP_OK;
}

static esp_err_t panel_ek79007_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ek79007_panel_t *ek79007 = (ek79007_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = ek79007->io;
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
#endif
