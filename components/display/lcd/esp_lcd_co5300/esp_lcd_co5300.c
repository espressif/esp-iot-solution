/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_lcd_types.h"

#include "esp_lcd_co5300.h"
#include "esp_lcd_co5300_interface.h"

static const char *TAG = "co5300";

esp_err_t esp_lcd_new_panel_co5300(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_CO5300_VER_MAJOR, ESP_LCD_CO5300_VER_MINOR, ESP_LCD_CO5300_VER_PATCH);
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    co5300_vendor_config_t *vendor_config = NULL;
    if (panel_dev_config->vendor_config) {
        vendor_config = (co5300_vendor_config_t *)panel_dev_config->vendor_config;
    }

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

#if SOC_MIPI_DSI_SUPPORTED
    if (vendor_config && vendor_config->flags.use_mipi_interface) {
        ret = esp_lcd_new_panel_co5300_mipi(io, panel_dev_config, ret_panel);

        return ret;
    }
#endif

    ret = esp_lcd_new_panel_co5300_spi(io, panel_dev_config, ret_panel);

    return ret;
}
