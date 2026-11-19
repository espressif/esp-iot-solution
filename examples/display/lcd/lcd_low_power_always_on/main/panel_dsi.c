/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "panel.h"
#include "driver/gpio.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "sdkconfig.h"

#define DSI_LANE_COUNT         2
#define DSI_PHY_LDO_CHANNEL    3
#define DSI_PHY_VOLTAGE_MV     2500
#define LCD_RESET_GPIO         GPIO_NUM_27

// The link speed must carry the pixel stream at the selected panel timing.
static const struct {
    float pixel_clock_mhz;
    uint32_t lane_rate_mbps;
} dsi_timings[] = {
    [DISPLAY_MODE_NORMAL] = {.pixel_clock_mhz = 52.0f, .lane_rate_mbps = 900},
    [DISPLAY_MODE_STANDBY] = {.pixel_clock_mhz = 8.61144f, .lane_rate_mbps = 224},
};

static esp_lcd_dsi_bus_handle_t dsi_bus;
static esp_lcd_panel_io_handle_t command_io;
static esp_lcd_panel_handle_t lcd_panel;

static clock_panel_t open_panel(display_mode_t mode)
{
    const esp_lcd_dsi_bus_config_t bus_config = {
        .num_data_lanes = DSI_LANE_COUNT,
        .lane_bit_rate_mbps = dsi_timings[mode].lane_rate_mbps,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    const esp_lcd_dbi_io_config_t io_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &io_config, &command_io));

    // Use RGB565 for both the framebuffer and the panel link.
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
    dpi_config.num_fbs = 1;
    dpi_config.dpi_clock_freq_mhz = dsi_timings[mode].pixel_clock_mhz;
    dpi_config.out_color_format = LCD_COLOR_FMT_RGB565;

    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = DSI_LANE_COUNT,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
#if CONFIG_EXAMPLE_DSI_SOFTWARE_RESET
        .reset_gpio_num = -1, // EK79007 sends DCS SWRESET through command I/O.
#else
        .reset_gpio_num = LCD_RESET_GPIO,
#endif
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(command_io, &panel_config, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));

    void *framebuffer;
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcd_panel, 1, &framebuffer));
    return (clock_panel_t) {
        .handle = lcd_panel,
        .pixels = framebuffer,
        .width = dpi_config.video_timing.h_size,
        .height = dpi_config.video_timing.v_size,
    };
}

clock_panel_t clock_panel_set_mode(display_mode_t mode)
{
    static esp_ldo_channel_handle_t phy_supply;
    if (lcd_panel) {
        // Pixel and lane rates are set at creation. Deleting the panel frees its framebuffer.
        ESP_ERROR_CHECK(esp_lcd_panel_del(lcd_panel));
        ESP_ERROR_CHECK(esp_lcd_panel_io_del(command_io));
        ESP_ERROR_CHECK(esp_lcd_del_dsi_bus(dsi_bus));
    } else {
        // Keep the PHY supply enabled across mode changes.
        const esp_ldo_channel_config_t supply = {
            .chan_id = DSI_PHY_LDO_CHANNEL,
            .voltage_mv = DSI_PHY_VOLTAGE_MV,
        };
        ESP_ERROR_CHECK(esp_ldo_acquire_channel(&supply, &phy_supply));
    }

    return open_panel(mode);
}
