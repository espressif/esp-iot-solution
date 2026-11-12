/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "hw_init.h"
#include "board_qspi.h"
esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *p, esp_lcd_panel_io_handle_t *io, uint8_t n, hw_rotation_t rotation)
{
    (void)n; (void)rotation;
    return qspi_board_create(p, io);
}
