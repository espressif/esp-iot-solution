/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_idf_version.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "app_lcd.h"

static const char *TAG = "app_lcd";

/* ESP32-P4 Function EV Board: 7-inch 1024x600 EK79007 MIPI-DSI panel */
#define LCD_H_RES                       (1024)
#define LCD_V_RES                       (600)
#define LCD_MIPI_DSI_LANE_NUM           (2)
#define LCD_MIPI_DSI_LANE_BITRATE_MBPS  (1000)
#define LCD_MIPI_DSI_PHY_LDO_CHAN       (3)         /* LDO_VO3 supplies VDD_MIPI_DPHY */
#define LCD_MIPI_DSI_PHY_LDO_VOLTAGE_MV (2500)
#define LCD_GPIO_BACKLIGHT              (GPIO_NUM_26)
#define LCD_GPIO_RST                    (GPIO_NUM_27)
#define LCD_LEDC_TIMER                  (LEDC_TIMER_1)
#define LCD_LEDC_CHANNEL                (LEDC_CHANNEL_1)
#define LCD_LEDC_RESOLUTION             (LEDC_TIMER_10_BIT)

/* GT911 touch controller on the board I2C bus */
#define TOUCH_I2C_NUM                   (1)
#define TOUCH_GPIO_SDA                  (GPIO_NUM_7)
#define TOUCH_GPIO_SCL                  (GPIO_NUM_8)

/* Match the previous BSP task placement. */
#define LVGL_TASK_PRIORITY              (4)
#define LVGL_TASK_CORE                  (0)

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LCD_LEDC_RESOLUTION;
    timer_cfg.timer_num = LCD_LEDC_TIMER;
    timer_cfg.freq_hz = 5000;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer config failed");

    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num = LCD_GPIO_BACKLIGHT;
    channel_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_cfg.channel = LCD_LEDC_CHANNEL;
    channel_cfg.intr_type = LEDC_INTR_DISABLE;
    channel_cfg.timer_sel = LCD_LEDC_TIMER;
    channel_cfg.duty = 0;
    channel_cfg.hpoint = 0;
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_cfg), TAG, "LEDC channel config failed");

    return ESP_OK;
}

static esp_err_t backlight_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }
    uint32_t duty = ((1 << LCD_LEDC_RESOLUTION) - 1) * brightness_percent / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL, duty), TAG, "LEDC set duty failed");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL);
}

static esp_err_t panel_init(esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {};
    ldo_cfg.chan_id = LCD_MIPI_DSI_PHY_LDO_CHAN;
    ldo_cfg.voltage_mv = LCD_MIPI_DSI_PHY_LDO_VOLTAGE_MV;
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan), TAG, "Acquire LDO channel for DPHY failed");

    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {};
    bus_config.bus_id = 0;
    bus_config.num_data_lanes = LCD_MIPI_DSI_LANE_NUM;
    bus_config.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT;
    bus_config.lane_bit_rate_mbps = LCD_MIPI_DSI_LANE_BITRATE_MBPS;
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus), TAG, "New DSI bus failed");

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = {};
    dbi_config.virtual_channel = 0;
    dbi_config.lcd_cmd_bits = 8;
    dbi_config.lcd_param_bits = 8;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io), TAG, "New panel IO failed");

    ESP_LOGI(TAG, "Install EK79007 LCD control panel");
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
    dpi_config.num_fbs = 1;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    dpi_config.flags.use_dma2d = true;
#endif

    ek79007_vendor_config_t vendor_config = {};
    vendor_config.mipi_config.dsi_bus = mipi_dsi_bus;
    vendor_config.mipi_config.dpi_config = &dpi_config;

    esp_lcd_panel_dev_config_t lcd_dev_config = {};
    lcd_dev_config.reset_gpio_num = LCD_GPIO_RST;
    lcd_dev_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    lcd_dev_config.bits_per_pixel = 16;
    lcd_dev_config.vendor_config = &vendor_config;

    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &panel), TAG, "New EK79007 panel failed");

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(panel), TAG, "Enable DMA2D failed");
#endif

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "Panel init failed");

    *ret_panel = panel;
    *ret_io = io;
    return ESP_OK;
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *ret_touch)
{
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_bus_config_t i2c_bus_conf = {};
    i2c_bus_conf.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_conf.sda_io_num = TOUCH_GPIO_SDA;
    i2c_bus_conf.scl_io_num = TOUCH_GPIO_SCL;
    i2c_bus_conf.i2c_port = TOUCH_I2C_NUM;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle), TAG, "New I2C bus failed");

    esp_lcd_panel_io_handle_t tp_io = NULL;
    /* The GT911 macro's designator order is invalid in C++. */
    esp_lcd_panel_io_i2c_config_t tp_io_config = {};
    tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    tp_io_config.control_phase_bytes = 1;
    tp_io_config.dc_bit_offset = 0;
    tp_io_config.lcd_cmd_bits = 16;
    tp_io_config.scl_speed_hz = 100000;
    tp_io_config.flags.disable_control_phase = 1;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io), TAG, "New touch IO failed");

    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = LCD_H_RES;
    tp_cfg.y_max = LCD_V_RES;
    tp_cfg.rst_gpio_num = GPIO_NUM_NC;
    tp_cfg.int_gpio_num = GPIO_NUM_NC;
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 1;
    tp_cfg.flags.mirror_y = 1;
    return esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, ret_touch);
}

esp_err_t app_lcd_init(void)
{
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "Backlight init failed");

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(panel_init(&panel, &io), TAG, "Panel init failed");

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_priority = LVGL_TASK_PRIORITY;
    adapter_config.task_core_id = LVGL_TASK_CORE;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_config), TAG, "LVGL adapter init failed");

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         panel, io, LCD_H_RES, LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    /* Tear avoidance requires multiple frame buffers. */
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
    lv_display_t *disp = esp_lv_adapter_register_display(&display_config);
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_FAIL, TAG, "Register display failed");

    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(touch_init(&tp), TAG, "Touch init failed");
    esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    ESP_RETURN_ON_FALSE(esp_lv_adapter_register_touch(&touch_config) != NULL, ESP_FAIL, TAG, "Register touch failed");

    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "LVGL adapter start failed");

    ESP_RETURN_ON_ERROR(backlight_set(100), TAG, "Backlight set failed");
    ESP_LOGI(TAG, "LCD %dx%d initialized", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

bool app_lcd_lock(uint32_t timeout_ms)
{
    /* Preserve bsp_display_lock semantics. */
    return esp_lv_adapter_lock(timeout_ms == 0 ? -1 : (int32_t)timeout_ms) == ESP_OK;
}

void app_lcd_unlock(void)
{
    esp_lv_adapter_unlock();
}
