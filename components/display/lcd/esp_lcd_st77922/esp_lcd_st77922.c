/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_lcd_types.h"

#include "st77922_interface.h"
#include "esp_lcd_st77922.h"

static const char *TAG = "st77922";

esp_err_t esp_lcd_new_panel_st77922(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_ST77922_VER_MAJOR, ESP_LCD_ST77922_VER_MINOR,
             ESP_LCD_ST77922_VER_PATCH);
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

    if (panel_dev_config->vendor_config) {
        st77922_vendor_config_t *vendor_config = (st77922_vendor_config_t *)panel_dev_config->vendor_config;

        if (vendor_config->flags.use_rgb_interface + vendor_config->flags.use_qspi_interface +
                vendor_config->flags.use_mipi_interface > 1) {
            ESP_LOGE(TAG, "Only one interface is supported");
            return ESP_ERR_NOT_SUPPORTED;
        }

#if SOC_MIPI_DSI_SUPPORTED
        if (vendor_config->flags.use_mipi_interface) {
            ret = esp_lcd_new_panel_st77922_mipi(io, panel_dev_config, ret_panel);

            return ret;
        }
#endif

#if SOC_LCD_RGB_SUPPORTED
        if (vendor_config->flags.use_rgb_interface) {
            ret = esp_lcd_new_panel_st77922_rgb(io, panel_dev_config, ret_panel);

            return ret;
        }
#endif

        ret = esp_lcd_new_panel_st77922_general(io, panel_dev_config, ret_panel);

        return ret;
    } else {
        ret = esp_lcd_new_panel_st77922_general(io, panel_dev_config, ret_panel);
    }

    return ret;
}
