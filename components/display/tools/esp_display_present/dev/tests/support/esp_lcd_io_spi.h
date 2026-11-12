/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "esp_lcd_panel_io.h"
#ifndef PRESENT_HOST_SPI_NO_MEMORY_INFO
#define ESP_LCD_SPI_IO_HAS_MEMORY_INFO 1
#endif
typedef struct {
    bool psram_dma_direct;
} esp_lcd_panel_io_spi_memory_info_t;
esp_err_t esp_lcd_panel_io_spi_get_memory_info(esp_lcd_panel_io_handle_t,
                                               esp_lcd_panel_io_spi_memory_info_t *);
