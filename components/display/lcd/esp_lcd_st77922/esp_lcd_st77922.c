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

const char *TAG = "st77922";

esp_err_t esp_lcd_new_panel_st77922(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_ST77922_VER_MAJOR, ESP_LCD_ST77922_VER_MINOR,
             ESP_LCD_ST77922_VER_PATCH);
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

    if (panel_dev_config->vendor_config) {
        st77922_vendor_config_t *vendor_config = (st77922_vendor_config_t *)panel_dev_config->vendor_config;

        if (vendor_config->flags.use_rgb_interface && vendor_config->flags.use_qspi_interface) {
            ESP_LOGE(TAG, "Both RGB and QSPI interfaces are not supported");
            return ESP_ERR_NOT_SUPPORTED;
        }

#if SOC_LCD_RGB_SUPPORTED
        if (vendor_config->flags.use_rgb_interface) {
            ESP_RETURN_ON_FALSE(io && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`io` and `rgb_config` are necessary when use mipi interface");
            ret = esp_lcd_new_panel_st77922_rgb(io, panel_dev_config, ret_panel);
        }
#endif

        if (!vendor_config->flags.use_rgb_interface) {
            ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "`io` is necessary when use rgb interface");
            ret = esp_lcd_new_panel_st77922_general(io, panel_dev_config, ret_panel);
        }
    } else {
        ESP_RETURN_ON_FALSE(io, ESP_ERR_INVALID_ARG, TAG, "`io` is necessary when use rgb interface");
        ret = esp_lcd_new_panel_st77922_general(io, panel_dev_config, ret_panel);
    }

    return ret;
}
