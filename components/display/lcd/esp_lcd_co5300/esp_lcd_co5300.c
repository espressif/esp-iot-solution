/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"
#include "esp_check.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_interface.h"

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
#if !SOC_MIPI_DSI_SUPPORTED
    (void)vendor_config;
#endif

    ret = esp_lcd_new_panel_co5300_spi(io, panel_dev_config, ret_panel);

    return ret;
}

esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t panel, uint8_t brightness_percent)
{
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_ARG, TAG, "invalid panel handle");
    ESP_RETURN_ON_FALSE(brightness_percent <= 100, ESP_ERR_INVALID_ARG, TAG, "brightness percentage must be 0-100");

    co5300_panel_context_t *panel_ctx = (co5300_panel_context_t *)panel->user_data;
    ESP_RETURN_ON_FALSE(panel_ctx, ESP_ERR_INVALID_STATE, TAG, "panel context not initialized");
    ESP_RETURN_ON_FALSE(panel_ctx->apply_brightness, ESP_ERR_NOT_SUPPORTED, TAG, "brightness control not supported");

    ESP_RETURN_ON_ERROR(panel_ctx->apply_brightness(panel_ctx->driver_data, brightness_percent), TAG,
                        "apply brightness failed");

    return ESP_OK;
}
