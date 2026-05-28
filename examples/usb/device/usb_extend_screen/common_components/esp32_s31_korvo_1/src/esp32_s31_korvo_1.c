/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev_defaults.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt1151.h"
#include "esp_log.h"

static const char *TAG = "S31-Korvo";

#define BSP_LCD_DEFAULT_FB_COUNT  (2)

static esp_lcd_panel_handle_t s_lcd_panel;
static esp_lcd_panel_io_handle_t s_touch_io;
static esp_lcd_touch_handle_t s_touch;
static i2c_master_bus_handle_t s_i2c_bus;
static i2s_chan_handle_t s_audio_tx_chan;
static i2s_chan_handle_t s_audio_rx_chan;
static const audio_codec_data_if_t *s_audio_data_if;
static int s_lcd_brightness = 100;

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
    return gpio_set_level(gpio_num, level);
}

static void bsp_display_fill_data_gpio_nums(gpio_num_t data_gpio_nums[ESP_LCD_RGB_BUS_WIDTH_MAX])
{
    const gpio_num_t pins[ESP_LCD_RGB_BUS_WIDTH_MAX] = {
        BSP_LCD_DATA0,
        BSP_LCD_DATA1,
        BSP_LCD_DATA2,
        BSP_LCD_DATA3,
        BSP_LCD_DATA4,
        BSP_LCD_DATA5,
        BSP_LCD_DATA6,
        BSP_LCD_DATA7,
        BSP_LCD_DATA8,
        BSP_LCD_DATA9,
        BSP_LCD_DATA10,
        BSP_LCD_DATA11,
        BSP_LCD_DATA12,
        BSP_LCD_DATA13,
        BSP_LCD_DATA14,
        BSP_LCD_DATA15,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
        GPIO_NUM_NC,
    };

    memcpy(data_gpio_nums, pins, sizeof(pins));
}

static esp_err_t bsp_i2c_init(void)
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

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus), TAG, "create I2C bus failed");
    ESP_LOGI(TAG, "I2C bus initialized: SDA=%d, SCL=%d", BSP_I2C_SDA, BSP_I2C_SCL);
    return ESP_OK;
}

static i2s_std_config_t bsp_audio_default_i2s_config(void)
{
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_AUDIO_DEFAULT_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_AUDIO_I2S_MCLK,
            .bclk = BSP_AUDIO_I2S_SCLK,
            .ws = BSP_AUDIO_I2S_LRCLK,
            .dout = BSP_AUDIO_I2S_SDOUT,
            .din = BSP_AUDIO_I2S_DSIN,
        },
    };

    return config;
}

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    if (s_audio_data_if) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_audio_tx_chan, &s_audio_rx_chan),
                        TAG, "create I2S channels failed");

    i2s_std_config_t default_config = bsp_audio_default_i2s_config();
    const i2s_std_config_t *active_config = i2s_config ? i2s_config : &default_config;

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_audio_tx_chan, active_config), TAG, "init I2S TX failed");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_audio_rx_chan, active_config), TAG, "init I2S RX failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_audio_tx_chan), TAG, "enable I2S TX failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_audio_rx_chan), TAG, "enable I2S RX failed");

    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port = I2S_NUM_0,
        .tx_handle = s_audio_tx_chan,
        .rx_handle = s_audio_rx_chan,
    };
    s_audio_data_if = audio_codec_new_i2s_data(&i2s_data_cfg);
    ESP_RETURN_ON_FALSE(s_audio_data_if, ESP_FAIL, TAG, "create audio I2S data interface failed");

    ESP_LOGI(TAG, "Audio I2S initialized");
    return ESP_OK;
}

static esp_codec_dev_handle_t bsp_audio_codec_init(esp_codec_dev_type_t dev_type,
                                                   esp_codec_dec_work_mode_t codec_mode,
                                                   int16_t pa_pin)
{
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed (%s)", esp_err_to_name(ret));
        return NULL;
    }
    ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed (%s)", esp_err_to_name(ret));
        return NULL;
    }

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    ESP_RETURN_ON_FALSE(gpio_if, NULL, TAG, "create codec GPIO interface failed");

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = ES8389_CODEC_DEFAULT_ADDR,
        .bus_handle = s_i2c_bus,
    };
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    ESP_RETURN_ON_FALSE(ctrl_if, NULL, TAG, "create codec I2C control interface failed");

    const esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5.0f,
        .codec_dac_voltage = 3.3f,
    };

    es8389_codec_cfg_t es8389_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = codec_mode,
        .pa_pin = pa_pin,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = false,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
        .no_dac_ref = false,
    };
    const audio_codec_if_t *codec_if = es8389_codec_new(&es8389_cfg);
    ESP_RETURN_ON_FALSE(codec_if, NULL, TAG, "create ES8389 codec interface failed");

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = dev_type,
        .codec_if = codec_if,
        .data_if = s_audio_data_if,
    };

    return esp_codec_dev_new(&codec_dev_cfg);
}

esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void)
{
    return bsp_audio_codec_init(ESP_CODEC_DEV_TYPE_OUT, ESP_CODEC_DEV_WORK_MODE_DAC, BSP_AUDIO_PA_CTRL);
}

esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
{
    return bsp_audio_codec_init(ESP_CODEC_DEV_TYPE_IN, ESP_CODEC_DEV_WORK_MODE_ADC, GPIO_NUM_NC);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_config_output_gpio(BSP_LCD_BACKLIGHT, 1);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_config_output_gpio(BSP_LCD_BACKLIGHT, 0);
}

esp_err_t bsp_display_brightness_init(void)
{
    s_lcd_brightness = 100;
    return bsp_display_backlight_on();
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent < 0) {
        brightness_percent = 0;
    } else if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    s_lcd_brightness = brightness_percent;
    return brightness_percent ? bsp_display_backlight_on() : bsp_display_backlight_off();
}

int bsp_display_brightness_get(void)
{
    return s_lcd_brightness;
}

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    (void)config;

    ESP_RETURN_ON_FALSE(ret_touch, ESP_ERR_INVALID_ARG, TAG, "touch handle output is null");

    if (s_touch) {
        *ret_touch = s_touch;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "I2C init failed");

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
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
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

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    ESP_RETURN_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, TAG, "panel handle output is null");
    if (ret_io) {
        *ret_io = NULL;
    }

    if (s_lcd_panel) {
        *ret_panel = s_lcd_panel;
        return ESP_OK;
    }

    int fb_count = BSP_LCD_DEFAULT_FB_COUNT;
    if (config && config->dpi_fb_buf_num > 0) {
        fb_count = config->dpi_fb_buf_num;
    }
    ESP_RETURN_ON_FALSE(fb_count > 0 && fb_count <= ESP_RGB_LCD_PANEL_MAX_FB_NUM,
                        ESP_ERR_INVALID_ARG, TAG, "invalid RGB frame buffer count: %d", fb_count);

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
        .num_fbs = fb_count,
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
        esp_lcd_panel_del(panel);
        return ret;
    }
    ret = esp_lcd_panel_init(panel);
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel);
        return ret;
    }
    ret = bsp_display_backlight_on();
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel);
        return ret;
    }

    s_lcd_panel = panel;
    *ret_panel = panel;

    ESP_LOGI(TAG, "RGB LCD initialized: %dx%d RGB565, fb_count=%d", BSP_LCD_H_RES, BSP_LCD_V_RES, fb_count);
    return ESP_OK;
}
