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
#include "esp_lcd_hx8399.h"

#define HX8399_CMD_PAGE         (0xB9)
#define HX8399_BKxSEL_BYTE0     (0xFF)
#define HX8399_BKxSEL_BYTE1     (0x83)
#define HX8399_BKxSEL_BYTE2     (0x99)
#define HX8399_CMD_CLOSE        (0x00)

#define HX8399_CMD_DSI_INT0     (0xBA)
#define HX8399_DSI_1_LANE       (0x00)
#define HX8399_DSI_2_LANE       (0x01)
#define HX8399_DSI_3_LANE       (0x10)
#define HX8399_DSI_4_LANE       (0x11)

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save surrent value of LCD_CMD_COLMOD register
    const hx8399_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    uint8_t lane_num;
    struct {
        unsigned int reset_level: 1;
    } flags;
    // To save the original functions of MIPI DPI panel
    esp_err_t (*del)(esp_lcd_panel_t *panel);
    esp_err_t (*init)(esp_lcd_panel_t *panel);
} hx8399_panel_t;

static const char *TAG = "hx8399";

static esp_err_t panel_hx8399_del(esp_lcd_panel_t *panel);
static esp_err_t panel_hx8399_init(esp_lcd_panel_t *panel);
static esp_err_t panel_hx8399_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_hx8399_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_hx8399_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_hx8399(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_HX8399_VER_MAJOR, ESP_LCD_HX8399_VER_MINOR,
             ESP_LCD_HX8399_VER_PATCH);
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    hx8399_vendor_config_t *vendor_config = (hx8399_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config && vendor_config->mipi_config.dpi_config && vendor_config->mipi_config.dsi_bus, ESP_ERR_INVALID_ARG, TAG,
                        "invalid vendor config");

    esp_err_t ret = ESP_OK;
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)calloc(1, sizeof(hx8399_panel_t));
    ESP_RETURN_ON_FALSE(hx8399, ESP_ERR_NO_MEM, TAG, "no mem for hx8399 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
    case LCD_RGB_ELEMENT_ORDER_RGB:
        hx8399->madctl_val = 0;
        break;
    case LCD_RGB_ELEMENT_ORDER_BGR:
        hx8399->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16: // RGB565
        hx8399->colmod_val = 0x55;
        break;
    case 18: // RGB666
        hx8399->colmod_val = 0x66;
        break;
    case 24: // RGB888
        hx8399->colmod_val = 0x77;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    hx8399->io = io;
    hx8399->init_cmds = vendor_config->init_cmds;
    hx8399->init_cmds_size = vendor_config->init_cmds_size;
    hx8399->lane_num = vendor_config->mipi_config.lane_num;
    hx8399->reset_gpio_num = panel_dev_config->reset_gpio_num;
    hx8399->flags.reset_level = panel_dev_config->flags.reset_active_high;

    // Create MIPI DPI panel
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor_config->mipi_config.dsi_bus, vendor_config->mipi_config.dpi_config, &panel_handle), err, TAG,
                      "create MIPI DPI panel failed");
    ESP_LOGD(TAG, "new MIPI DPI panel @%p", panel_handle);

    // Save the original functions of MIPI DPI panel
    hx8399->del = panel_handle->del;
    hx8399->init = panel_handle->init;
    // Overwrite the functions of MIPI DPI panel
    panel_handle->del = panel_hx8399_del;
    panel_handle->init = panel_hx8399_init;
    panel_handle->reset = panel_hx8399_reset;
    panel_handle->invert_color = panel_hx8399_invert_color;
    panel_handle->disp_on_off = panel_hx8399_disp_on_off;
    panel_handle->user_data = hx8399;
    *ret_panel = panel_handle;
    ESP_LOGD(TAG, "new hx8399 panel @%p", hx8399);

    return ESP_OK;

err:
    if (hx8399) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(hx8399);
    }
    return ret;
}

static const hx8399_lcd_init_cmd_t vendor_specific_init_code_default[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xB9, (uint8_t []){0xFF, 0x83, 0x99}, 3, 0},
    {0xD2, (uint8_t []){0x77}, 1, 0},
    {0xB1, (uint8_t []){0x02, 0x04, 0x74, 0x94, 0x01, 0x32, 0x33, 0x11, 0x11, 0xAB, 0x4D, 0x56, 0x73, 0x02, 0x02}, 15, 0},
    {0xB2, (uint8_t []){0x00, 0x80, 0x80, 0xAE, 0x05, 0x07, 0x5A, 0x11, 0x00, 0x00, 0x10, 0x1E, 0x70, 0x03, 0xD4}, 15, 0},
    {0xB4, (uint8_t []){0x00, 0xFF, 0x02, 0xC0, 0x02, 0xC0, 0x00, 0x00, 0x08, 0x00, 0x04, 0x06, 0x00, 0x32, 0x04, 0x0A, 0x08, 0x21, 0x03, 0x01, 0x00, 0x0F, 0xB8, 0x8B, 0x02, 0xC0, 0x02, 0xC0, 0x00, 0x00, 0x08, 0x00, 0x04, 0x06, 0x00, 0x32, 0x04, 0x0A, 0x08, 0x01, 0x00, 0x0F, 0xB8, 0x01}, 44, 0},
    {0xD3, (uint8_t []){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x10, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x05, 0x07, 0x00, 0x00, 0x00, 0x05, 0x40}, 33, 10},
    {0xD5, (uint8_t []){0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x21, 0x20, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x18, 0x18, 0x18, 0x18}, 32, 10},
    {0xD6, (uint8_t []){0x18, 0x18, 0x19, 0x19, 0x40, 0x40, 0x20, 0x21, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x40, 0x40, 0x40, 0x40}, 32, 10},
    {0xD8, (uint8_t []){0xA2, 0xAA, 0x02, 0xA0, 0xA2, 0xA8, 0x02, 0xA0, 0xB0, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00}, 16, 0},
    {0xBD, (uint8_t []){0x01}, 1, 0},
    {0xD8, (uint8_t []){0xB0, 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00, 0xE2, 0xAA, 0x03, 0xF0, 0xE2, 0xAA, 0x03, 0xF0}, 16, 0},
    {0xBD, (uint8_t []){0x02}, 1, 0},
    {0xD8, (uint8_t []){0xE2, 0xAA, 0x03, 0xF0, 0xE2, 0xAA, 0x03, 0xF0}, 8, 0},
    {0xBD, (uint8_t []){0x00}, 1, 0},
    {0xB6, (uint8_t []){0x8D, 0x8D}, 2, 0},
    {0xE0, (uint8_t []){0x00, 0x0E, 0x19, 0x13, 0x2E, 0x39, 0x48, 0x44, 0x4D, 0x57, 0x5F, 0x66, 0x6C, 0x76, 0x7F, 0x85, 0x8A, 0x95, 0x9A, 0xA4, 0x9B, 0xAB, 0xB0, 0x5C, 0x58, 0x64, 0x77, 0x00, 0x0E, 0x19, 0x13, 0x2E, 0x39, 0x48, 0x44, 0x4D, 0x57, 0x5F, 0x66, 0x6C, 0x76, 0x7F, 0x85, 0x8A, 0x95, 0x9A, 0xA4, 0x9B, 0xAB, 0xB0, 0x5C, 0x58, 0x64, 0x77}, 54, 10},
    {0xCC, (uint8_t []){0x08}, 1, 0},
    //============ Gamma END===========
};

static esp_err_t panel_hx8399_del(esp_lcd_panel_t *panel)
{
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)panel->user_data;

    // Delete MIPI DPI panel
    ESP_RETURN_ON_ERROR(hx8399->del(panel), TAG, "del hx8399 panel failed");
    if (hx8399->reset_gpio_num >= 0) {
        gpio_reset_pin(hx8399->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del hx8399 panel @%p", hx8399);
    free(hx8399);

    return ESP_OK;
}

static esp_err_t panel_hx8399_init(esp_lcd_panel_t *panel)
{
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hx8399->io;
    const hx8399_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    uint8_t lane_command = HX8399_DSI_2_LANE;
    bool is_cmd_overwritten = false;

    switch (hx8399->lane_num) {
    case 0:
        lane_command = HX8399_DSI_2_LANE;
        break;
    case 1:
        lane_command = HX8399_DSI_1_LANE;
        break;
    case 2:
        lane_command = HX8399_DSI_2_LANE;
        break;
    case 3:
        lane_command = HX8399_DSI_3_LANE;
        break;
    case 4:
        lane_command = HX8399_DSI_4_LANE;
        break;
    default:
        ESP_LOGE(TAG, "Invalid lane number %d", hx8399->lane_num);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HX8399_CMD_PAGE, (uint8_t[]) {
        HX8399_CMD_CLOSE,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG,
                        "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        hx8399->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        hx8399->colmod_val,
    }, 1), TAG, "send command failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HX8399_CMD_PAGE, (uint8_t[]) {
        HX8399_BKxSEL_BYTE0, HX8399_BKxSEL_BYTE1, HX8399_BKxSEL_BYTE2
    }, 3), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HX8399_CMD_DSI_INT0, (uint8_t[]) {
        lane_command,
    }, 1), TAG, "send command failed");

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    if (hx8399->init_cmds) {
        init_cmds = hx8399->init_cmds;
        init_cmds_size = hx8399->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_code_default;
        init_cmds_size = sizeof(vendor_specific_init_code_default) / sizeof(hx8399_lcd_init_cmd_t);
    }

    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        if (init_cmds[i].data_bytes > 0) {
            switch (init_cmds[i].cmd) {
            case LCD_CMD_MADCTL:
                is_cmd_overwritten = true;
                hx8399->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
                break;
            case LCD_CMD_COLMOD:
                is_cmd_overwritten = true;
                hx8399->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
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

    ESP_RETURN_ON_ERROR(hx8399->init(panel), TAG, "init MIPI DPI panel failed");

    return ESP_OK;
}

static esp_err_t panel_hx8399_reset(esp_lcd_panel_t *panel)
{
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hx8399->io;

    // Perform hardware reset
    if (hx8399->reset_gpio_num >= 0) {
        gpio_set_level(hx8399->reset_gpio_num, hx8399->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(hx8399->reset_gpio_num, !hx8399->flags.reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else if (io) { // Perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    return ESP_OK;
}

static esp_err_t panel_hx8399_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hx8399->io;
    uint8_t command = 0;

    ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_STATE, TAG, "invalid panel IO");

    if (invert_color_data) {
        command = LCD_CMD_INVON;
    } else {
        command = LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HX8399_CMD_PAGE, (uint8_t[]) {
        HX8399_CMD_CLOSE,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t panel_hx8399_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    hx8399_panel_t *hx8399 = (hx8399_panel_t *)panel->user_data;
    esp_lcd_panel_io_handle_t io = hx8399->io;
    int command = 0;

    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, HX8399_CMD_PAGE, (uint8_t[]) {
        HX8399_CMD_CLOSE,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}
#endif
