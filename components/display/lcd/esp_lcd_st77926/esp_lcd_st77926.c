/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_lcd_types.h"

#include "esp_lcd_st77926.h"
#include "esp_lcd_st77926_interface.h"

static const char *TAG = "st77926";

esp_err_t esp_lcd_new_panel_st77926(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid arguments");

    return esp_lcd_new_panel_st77926_qspi(io, panel_dev_config, ret_panel);
}
