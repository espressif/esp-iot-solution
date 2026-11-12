/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Mosaico BSP 0.2.0: eFuse board revision selects clock/reset pins. */
#include "board_qspi.h"
#include "esp_lcd_co5300.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const co5300_lcd_init_cmd_t s_vendor_init[] = {
    {0x11, NULL, 0, 600},
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0x80}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x29, NULL, 0, 600},
};

esp_err_t qspi_board_create(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io)
{
    uint16_t version = 0;
    ESP_RETURN_ON_ERROR(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, &version, 16), "qspi", "board revision");
    ESP_RETURN_ON_FALSE(version == 0x100 || version == 0x101 || version == 0x102, ESP_ERR_NOT_SUPPORTED, "qspi", "unknown board revision 0x%04x", version);
    int clk = version == 0x100 ? 44 : 42, rst = version == 0x100 ? 42 : 44;
    ESP_LOGI("qspi", "BSP revision=%04x clk=%d rst=%d TE=43 CS=50 QSPI=40MHz", version, clk, rst);
    gpio_config_t power = {.pin_bit_mask = 1ULL << 60, .mode = GPIO_MODE_OUTPUT};
    ESP_RETURN_ON_ERROR(gpio_config(&power), "qspi", "power gpio");
    ESP_RETURN_ON_ERROR(gpio_set_level(60, 0), "qspi", "power on");
    vTaskDelay(pdMS_TO_TICKS(100));
    spi_bus_config_t bus = CO5300_PANEL_BUS_QSPI_CONFIG(clk, 36, 51, 35, 9, 480 * 480 * 2);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO), "qspi", "bus");
    esp_lcd_panel_io_spi_config_t cfg = CO5300_PANEL_IO_QSPI_CONFIG(50, NULL, NULL);
    cfg.flags.psram_dma_direct = true; /* Audited SPI2 + S31 EDMA path; no full-frame internal bounce. */
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &cfg, io), "qspi", "IO");
    co5300_vendor_config_t vendor = {.init_cmds = s_vendor_init,
                                     .init_cmds_size = sizeof(s_vendor_init) / sizeof(s_vendor_init[0]), .flags.use_qspi_interface = true
                                    };
    esp_lcd_panel_dev_config_t dev = {.reset_gpio_num = rst, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
                                      .bits_per_pixel = 16, .vendor_config = &vendor
                                     };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(*io, &dev, panel), "qspi", "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), "qspi", "reset");
    return esp_lcd_panel_init(*panel);
}
