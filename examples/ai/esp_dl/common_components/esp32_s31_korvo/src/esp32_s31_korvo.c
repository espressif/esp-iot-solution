/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt1151.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_video_init.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "S31-Korvo";

static esp_lcd_panel_handle_t s_lcd_panel;
static bsp_display_config_t s_lcd_config;
static bool s_lcd_config_valid;
static lv_display_t *s_lvgl_display;
static i2c_master_bus_handle_t s_i2c_bus;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static lv_indev_t *s_touch_indev;

/* ==========================================================================
 * Private helpers
 * ========================================================================== */

static bsp_display_config_t bsp_display_get_default_config(void)
{
    const bsp_display_config_t config = BSP_DISPLAY_DEFAULT_CONFIG();
    return config;
}

static esp_err_t bsp_config_output_gpio(gpio_num_t gpio_num, int level)
{
    if (gpio_num == GPIO_NUM_NC) {
        return ESP_OK;
    }

    const gpio_config_t config = {
        .pin_bit_mask = BIT64(gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "configure GPIO %d failed", (int)gpio_num);
    ESP_RETURN_ON_ERROR(gpio_set_level(gpio_num, level), TAG, "set GPIO %d failed", (int)gpio_num);
    return ESP_OK;
}

static void bsp_display_fill_data_gpio_nums(gpio_num_t data_gpio_nums[ESP_LCD_RGB_BUS_WIDTH_MAX])
{
    const gpio_num_t pins[ESP_LCD_RGB_BUS_WIDTH_MAX] = {
        BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
        BSP_LCD_DATA4, BSP_LCD_DATA5, BSP_LCD_DATA6, BSP_LCD_DATA7,
        BSP_LCD_DATA8, BSP_LCD_DATA9, BSP_LCD_DATA10, BSP_LCD_DATA11,
        BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15,
        GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC,
        GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC, GPIO_NUM_NC,
    };

    memcpy(data_gpio_nums, pins, sizeof(pins));
}

static void bsp_touch_get_rotation_flags(esp_lv_adapter_rotation_t rotation,
                                         bool *swap_xy,
                                         bool *mirror_x,
                                         bool *mirror_y)
{
    bool swap = false;
    bool x_mirror = false;
    bool y_mirror = false;

    switch (rotation) {
    case ESP_LV_ADAPTER_ROTATE_90:
        swap = true;
        x_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_180:
        x_mirror = true;
        y_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_270:
        swap = true;
        y_mirror = true;
        break;
    case ESP_LV_ADAPTER_ROTATE_0:
    default:
        break;
    }

    if (swap_xy) {
        *swap_xy = swap;
    }
    if (mirror_x) {
        *mirror_x = x_mirror;
    }
    if (mirror_y) {
        *mirror_y = y_mirror;
    }
}

/* ==========================================================================
 * Shared I2C
 * ========================================================================== */

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = I2C_NUM_0,
        .flags.enable_internal_pullup = true,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus),
                        TAG, "create I2C bus failed");
    ESP_LOGI(TAG, "I2C bus initialized: SDA=%d, SCL=%d", BSP_I2C_SDA, BSP_I2C_SCL);
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    if (bsp_i2c_init() != ESP_OK) {
        return NULL;
    }
    return s_i2c_bus;
}

/* ==========================================================================
 * Touch (GT1151)
 * ========================================================================== */

esp_err_t bsp_touch_new(esp_lv_adapter_rotation_t rotation, esp_lcd_touch_handle_t *ret_touch)
{
    ESP_RETURN_ON_FALSE(ret_touch, ESP_ERR_INVALID_ARG, TAG, "touch handle output is null");

    if (s_touch) {
        *ret_touch = s_touch;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C init failed");

    bool swap_xy = false;
    bool mirror_x = false;
    bool mirror_y = false;
    bsp_touch_get_rotation_flags(rotation, &swap_xy, &mirror_x, &mirror_y);

    const esp_lcd_touch_config_t touch_config = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST,
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
    };

    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_GT1151_CONFIG();
    touch_io_config.scl_speed_hz = 400000;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &touch_io_config, &s_touch_io),
                        TAG, "create GT1151 panel IO failed");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt1151(s_touch_io, &touch_config, &s_touch),
                        TAG, "create GT1151 touch failed");

    *ret_touch = s_touch;
    ESP_LOGI(TAG, "GT1151 touch initialized");
    return ESP_OK;
}

/* ==========================================================================
 * Display (RGB 800x480 + esp_lvgl_adapter)
 * ========================================================================== */

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_config_output_gpio(BSP_LCD_BACKLIGHT, 1);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_config_output_gpio(BSP_LCD_BACKLIGHT, 0);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, TAG, "panel handle output is null");

    if (s_lcd_panel) {
        *ret_panel = s_lcd_panel;
        return ESP_OK;
    }

    bsp_display_config_t active_config = config ? *config : bsp_display_get_default_config();
    uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(active_config.tear_avoid_mode,
                                                                     active_config.rotation);
    ESP_RETURN_ON_FALSE(num_fbs > 0 && num_fbs <= ESP_RGB_LCD_PANEL_MAX_FB_NUM,
                        ESP_ERR_INVALID_ARG, TAG, "invalid RGB frame buffer count: %u",
                        (unsigned int)num_fbs);

    ESP_LOGI(TAG, "Initializing RGB LCD: %dx%d RGB565, pclk=%d Hz, fb_count=%u",
             BSP_LCD_H_RES, BSP_LCD_V_RES, BSP_LCD_PIXEL_CLOCK_HZ, (unsigned int)num_fbs);

    ESP_RETURN_ON_ERROR(bsp_display_backlight_off(), TAG, "turn off backlight failed");

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_pulse_width = BSP_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = BSP_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = BSP_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = BSP_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = BSP_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = BSP_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .data_width = BSP_LCD_DATA_WIDTH,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = num_fbs,
        .dma_burst_size = 64,
        .hsync_gpio_num = BSP_LCD_HSYNC,
        .vsync_gpio_num = BSP_LCD_VSYNC,
        .de_gpio_num = BSP_LCD_DE,
        .pclk_gpio_num = BSP_LCD_PCLK,
        .disp_gpio_num = BSP_LCD_DISP_EN,
        .flags = {
            .fb_in_psram = true,
        },
    };
    bsp_display_fill_data_gpio_nums(panel_config.data_gpio_nums);

    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &panel), TAG, "create RGB panel failed");

    esp_err_t ret = esp_lcd_panel_reset(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "reset RGB panel failed (%s)", esp_err_to_name(ret));
        esp_lcd_panel_del(panel);
        return ret;
    }

    ret = esp_lcd_panel_init(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "init RGB panel failed (%s)", esp_err_to_name(ret));
        esp_lcd_panel_del(panel);
        return ret;
    }

    ret = bsp_display_backlight_on();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "turn on backlight failed (%s)", esp_err_to_name(ret));
        esp_lcd_panel_del(panel);
        return ret;
    }

    s_lcd_panel = panel;
    s_lcd_config = active_config;
    s_lcd_config_valid = true;
    *ret_panel = panel;

    ESP_LOGI(TAG, "RGB LCD initialized");
    return ESP_OK;
}

lv_display_t *bsp_display_start(void)
{
    return bsp_display_start_with_config(NULL);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config)
{
    if (s_lvgl_display) {
        return s_lvgl_display;
    }

    bsp_display_config_t active_config = config ? *config : bsp_display_get_default_config();

    esp_err_t ret = bsp_display_new(&active_config, &s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display panel init failed (%s)", esp_err_to_name(ret));
        return NULL;
    }
    if (s_lcd_config_valid) {
        active_config = s_lcd_config;
    }

    if (!esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
        if (active_config.task_stack_size > 0) {
            adapter_config.task_stack_size = active_config.task_stack_size;
        }
        ret = esp_lv_adapter_init(&adapter_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL adapter init failed (%s)", esp_err_to_name(ret));
            return NULL;
        }
    }

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         s_lcd_panel, NULL, BSP_LCD_H_RES, BSP_LCD_V_RES, active_config.rotation);
    display_config.tear_avoid_mode = active_config.tear_avoid_mode;
    if (active_config.buffer_height > 0) {
        display_config.profile.buffer_height = active_config.buffer_height;
    }
    display_config.profile.enable_ppa_accel = active_config.enable_ppa_accel;

    s_lvgl_display = esp_lv_adapter_register_display(&display_config);
    if (!s_lvgl_display) {
        ESP_LOGE(TAG, "register RGB display to LVGL adapter failed");
        return NULL;
    }

    if (!active_config.enable_touch) {
        ESP_LOGW(TAG, "Touch init skipped by display configuration");
    } else {
        ret = bsp_touch_new(active_config.rotation, &s_touch);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "touch init failed (%s), continue without touch", esp_err_to_name(ret));
        } else {
            esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_lvgl_display, s_touch);
            s_touch_indev = esp_lv_adapter_register_touch(&touch_config);
            if (!s_touch_indev) {
                ESP_LOGW(TAG, "register touch to LVGL adapter failed");
            }
        }
    }

    ret = esp_lv_adapter_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL adapter start failed (%s)", esp_err_to_name(ret));
        esp_lv_adapter_unregister_display(s_lvgl_display);
        s_lvgl_display = NULL;
        return NULL;
    }

    ESP_LOGI(TAG, "Display registered to local esp_lvgl_adapter");
    return s_lvgl_display;
}

lv_display_t *bsp_display_get(void)
{
    return s_lvgl_display;
}

esp_lcd_panel_handle_t bsp_display_get_panel(void)
{
    return s_lcd_panel;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return s_touch_indev;
}

bool bsp_display_lock(int32_t timeout_ms)
{
    /* Match esp_lvgl_port semantics (P4): timeout 0 means wait forever. */
    return esp_lv_adapter_lock(timeout_ms == 0 ? -1 : timeout_ms) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    /* RGB sub-board has no dimmable backlight; nothing to do. */
    (void)brightness_percent;
    return ESP_OK;
}

/* ==========================================================================
 * Camera (DVP via esp_video)
 * ========================================================================== */

esp_err_t bsp_camera_start(const bsp_camera_cfg_t *cfg)
{
    (void)cfg;

    /* The DVP SCCB shares the board I2C bus that the display/touch already set up. */
    i2c_master_bus_handle_t i2c_handle = bsp_i2c_get_handle();

    esp_cam_ctlr_dvp_pin_config_t dvp_pin = {
        .data_width = CAM_CTLR_DATA_WIDTH_8,
        .data_io = {
            BSP_CAMERA_D0, BSP_CAMERA_D1, BSP_CAMERA_D2, BSP_CAMERA_D3,
            BSP_CAMERA_D4, BSP_CAMERA_D5, BSP_CAMERA_D6, BSP_CAMERA_D7,
        },
        .vsync_io = BSP_CAMERA_VSYNC,
        .de_io = BSP_CAMERA_HSYNC,
        .pclk_io = BSP_CAMERA_PCLK,
        .xclk_io = BSP_CAMERA_XCLK,
    };

    esp_video_init_dvp_config_t dvp_config = {
        .sccb_config = {
            .init_sccb = (i2c_handle == NULL),
            .i2c_handle = i2c_handle,
            .freq = 100000,
        },
        .reset_pin = BSP_CAMERA_RESET,
        .pwdn_pin = BSP_CAMERA_PWDN,
        .dvp_pin = dvp_pin,
        .xclk_freq = BSP_CAMERA_XCLK_FREQ,
    };

    esp_video_init_config_t cam_config = {
        .dvp = &dvp_config,
    };

    return esp_video_init(&cam_config);
}
