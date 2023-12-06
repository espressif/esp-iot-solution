/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_lcd_types.h"

#include "st77903_interface.h"
#include "esp_lcd_st77903.h"

const char *TAG = "st77903";

esp_err_t esp_lcd_new_panel_st77903(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguements");
    st77903_vendor_config_t *vendor_config = (st77903_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` is necessary");

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

    if (vendor_config->flags.use_qspi_interface) {
        ESP_RETURN_ON_FALSE(vendor_config->qspi_config, ESP_ERR_INVALID_ARG, TAG, "`qspi_config` is necessary when use qspi interface");
        (void)io;
        ret = lcd_new_panel_st77903_qspi(panel_dev_config, ret_panel);
    }
#if SOC_LCD_RGB_SUPPORTED
    else if (vendor_config->flags.use_rgb_interface) {
        ESP_RETURN_ON_FALSE(io && vendor_config->rgb_config, ESP_ERR_INVALID_ARG, TAG, "`io` and `rgb_config` are necessary when use rgb interface");
        ret = lcd_new_panel_st77903_rgb(io, panel_dev_config, ret_panel);
    }
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LCD panel create success, version: %d.%d.%d", ESP_LCD_ST77903_VER_MAJOR, ESP_LCD_ST77903_VER_MINOR,
                 ESP_LCD_ST77903_VER_PATCH);
    }

    return ret;
}
