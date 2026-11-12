/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#define QSPI_TE_GPIO 43
esp_err_t qspi_board_create(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io);
