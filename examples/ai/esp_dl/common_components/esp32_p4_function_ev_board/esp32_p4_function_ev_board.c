/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#if CONFIG_BSP_LCD_TYPE_1024_600
#include "esp_lcd_ek79007.h"
#else
#include "esp_lcd_ili9881c.h"
#endif

#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_lcd_touch_gt911.h"
#include "bsp_err_check.h"
#include "esp_video_device.h"
#include "esp_video_init.h"

static const char *TAG = "ESP32_P4_EV";

static lv_display_t *disp_lvgl = NULL;
static lv_indev_t *disp_indev = NULL;

static bool i2c_initialized = false;
static i2c_master_bus_handle_t i2c_handle = NULL;  // I2C Handle
static bsp_lcd_handles_t disp_handles;
static esp_ldo_channel_handle_t disp_phy_pwr_chan = NULL;
static esp_lcd_touch_handle_t tp = NULL;
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (i2c_initialized && i2c_handle) {
        BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
        i2c_initialized = false;
    }
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return i2c_handle;
}

// Bit number used to represent command and parameter
#define LCD_LEDC_CH            CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH

esp_err_t bsp_display_brightness_init(void)
{
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));
    return ESP_OK;
}

esp_err_t bsp_display_brightness_deinit(void)
{
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = 1,
        .deconfigure = 1
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_pause(LEDC_LOW_SPEED_MODE, 1));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

static esp_err_t bsp_enable_dsi_phy_power(void)
{
#if BSP_MIPI_DSI_PHY_PWR_LDO_CHAN > 0
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &disp_phy_pwr_chan), TAG, "Acquire LDO channel for DPHY failed");
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif // BSP_MIPI_DSI_PHY_PWR_LDO_CHAN > 0

    return ESP_OK;
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    bsp_lcd_handles_t handles;
    ret = bsp_display_new_with_handles(config, &handles);

    *ret_panel = handles.panel;
    *ret_io = handles.io;

    return ret;
}

esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t disp_panel = NULL;

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");
    ESP_RETURN_ON_ERROR(bsp_enable_dsi_phy_power(), TAG, "DSI PHY power failed");

    /* create MIPI DSI bus first, it will initialize the DSI PHY as well */
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = BSP_LCD_MIPI_DSI_LANE_NUM,
        .phy_clk_src = config->dsi_bus.phy_clk_src,
        .lane_bit_rate_mbps = config->dsi_bus.lane_bit_rate_mbps,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus), TAG, "New DSI bus init failed");

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io), err, TAG, "New panel IO failed");

#if CONFIG_BSP_LCD_TYPE_1024_600
    // create EK79007 control panel
    ESP_LOGI(TAG, "Install EK79007 LCD control panel");

#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG_CF(LCD_COLOR_FMT_RGB565);
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;

#if CONFIG_BSP_LCD_USE_DMA2D && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
    dpi_config.flags.use_dma2d = true;
#endif

    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 16,
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .reset_gpio_num = BSP_LCD_RST,
        .vendor_config = &vendor_config,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &disp_panel), err, TAG,
                      "New LCD panel EK79007 failed");

#if CONFIG_BSP_LCD_USE_DMA2D && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
    ESP_GOTO_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(disp_panel), err, TAG, "LCD panel enable DMA2D failed");
#endif

    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(disp_panel), err, TAG, "LCD panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(disp_panel), err, TAG, "LCD panel init failed");
#elif CONFIG_BSP_LCD_TYPE_1280_800
    // create ILI9881C control panel
    ESP_LOGI(TAG, "Install ILI9881C LCD control panel");
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
    esp_lcd_dpi_panel_config_t dpi_config = ILI9881C_800_1280_PANEL_60HZ_DPI_CONFIG_CF(LCD_COLOR_FMT_RGB888);
#else
    esp_lcd_dpi_panel_config_t dpi_config = ILI9881C_800_1280_PANEL_60HZ_DPI_CONFIG_CF(LCD_COLOR_FMT_RGB565);
#endif
    dpi_config.num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS;

#if CONFIG_BSP_LCD_USE_DMA2D && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
    dpi_config.flags.use_dma2d = true;
#endif

    ili9881c_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = BSP_LCD_MIPI_DSI_LANE_NUM,
        },
    };
    const esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9881c(io, &lcd_dev_config, &disp_panel), err, TAG,
                      "New LCD panel ILI9881C failed");

#if CONFIG_BSP_LCD_USE_DMA2D && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
    ESP_GOTO_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(disp_panel), err, TAG, "LCD panel enable DMA2D failed");
#endif

    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(disp_panel), err, TAG, "LCD panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(disp_panel), err, TAG, "LCD panel init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(disp_panel, true), err, TAG, "LCD panel ON failed");

#endif //CONFIG_BSP_LCD_TYPE_

    /* Return all handles */
    ret_handles->io = io;
    disp_handles.io = io;
    ret_handles->mipi_dsi_bus = mipi_dsi_bus;
    disp_handles.mipi_dsi_bus = mipi_dsi_bus;
    ret_handles->panel = disp_panel;
    disp_handles.panel = disp_panel;
    ret_handles->control = NULL;
    disp_handles.control = NULL;

    ESP_LOGI(TAG, "Display initialized");

    return ret;

err:
    bsp_display_delete();
    return ret;
}

void bsp_display_delete(void)
{
    if (disp_handles.panel) {
        esp_lcd_panel_del(disp_handles.panel);
        disp_handles.panel = NULL;
    }
    if (disp_handles.io) {
        esp_lcd_panel_io_del(disp_handles.io);
        disp_handles.io = NULL;
    }
    if (disp_handles.mipi_dsi_bus) {
        esp_lcd_del_dsi_bus(disp_handles.mipi_dsi_bus);
        disp_handles.mipi_dsi_bus = NULL;
    }

    if (disp_phy_pwr_chan) {
        esp_ldo_release_channel(disp_phy_pwr_chan);
        disp_phy_pwr_chan = NULL;
    }

    bsp_display_brightness_deinit();
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Initialize touch */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST, // Shared with LCD reset
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
}

void bsp_touch_delete(void)
{
    if (tp) {
        esp_lcd_touch_del(tp);
    }
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
        tp_io_handle = NULL;
    }
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new_with_handles(&cfg->hw_cfg, &disp_handles));

    ESP_LOGI(TAG, "Display resolution %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);

    if (!esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
        if (cfg->task_stack_size > 0) {
            adapter_config.task_stack_size = cfg->task_stack_size;
        }
        BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_init(&adapter_config));
    }

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                         disp_handles.panel, disp_handles.io,
                                                         BSP_LCD_H_RES, BSP_LCD_V_RES, cfg->rotation);
#if !CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    /* Only one frame buffer is available: fall back to the no-tear-avoidance mode. */
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
#endif

    return esp_lv_adapter_register_display(&display_config);
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    return esp_lv_adapter_register_touch(&touch_cfg);
}

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .hw_cfg = {
            .dsi_bus = {
                .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_DEFAULT, // let the driver choose the default clock source
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            }
        },
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .task_stack_size = 0,
    };
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);

    BSP_NULL_CHECK(disp_lvgl = bsp_display_lcd_init(cfg), NULL);
    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp_lvgl), NULL);

    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_start());
    return disp_lvgl;
}

void bsp_display_stop(lv_display_t *display)
{
    if (disp_indev) {
        esp_lv_adapter_unregister_touch(disp_indev);
        disp_indev = NULL;
    }
    if (display) {
        esp_lv_adapter_unregister_display(display);
        if (display == disp_lvgl) {
            disp_lvgl = NULL;
        }
    }
    if (esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_deinit();
    }

    bsp_touch_delete();
    bsp_display_delete();
    bsp_i2c_deinit();
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    /* Match the previous esp_lvgl_port semantics: 0 blocks indefinitely. */
    return esp_lv_adapter_lock(timeout_ms == 0 ? -1 : (int32_t)timeout_ms) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg)
{
    /* Initialize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    const esp_video_init_csi_config_t base_csi_config = {
        .sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_handle,
            .freq = 400000,
        },
        .reset_pin = BSP_CAMERA_RST,
        .pwdn_pin  = -1,
    };

    esp_video_init_config_t cam_config = {
        .csi      = &base_csi_config,
    };

    return esp_video_init(&cam_config);
}
