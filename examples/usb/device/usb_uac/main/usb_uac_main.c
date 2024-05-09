/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <math.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "tusb.h"
#include "usb_device_uac.h"
#include "bsp_board_extra.h"

/* Can be used for i2s_std_gpio_config_t and/or i2s_std_config_t initialization */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = BSP_I2S_MCLK,  \
        .bclk = BSP_I2S_SCLK,  \
        .ws = BSP_I2S_LCLK,    \
        .dout = BSP_I2S_DOUT,  \
        .din = BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

/* This configuration is used by default in bsp_audio_init() */
#define BSP_I2S_DUPLEX_STEREO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

static const char *TAG = "usb_uac_main";

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    size_t bytes_written = 0;
    bsp_extra_i2s_write(buf, len, &bytes_written, 0);
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    if (bsp_extra_i2s_read(buf, len, bytes_read, 0) != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed");
    }
    return ESP_OK;
}

static void uac_device_set_mute_cb(uint32_t mute, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    bsp_extra_codec_mute_set(!mute);
}

static void uac_device_set_volume_cb(uint32_t volume, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    bsp_extra_codec_volume_set(volume, NULL);
}

void app_main(void)
{
    /* Initialize audio i2s */
    i2s_std_config_t i2s_config = BSP_I2S_DUPLEX_STEREO_CFG(CONFIG_UAC_SAMPLE_RATE);
    i2s_config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
    bsp_i2c_init();
    bsp_audio_init(&i2s_config);

    bsp_extra_codec_init();
    bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE, 16, CONFIG_UAC_SPEAKER_CHANNEL_NUM);
    bsp_audio_poweramp_enable(true);

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };

    uac_device_init(&config);
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
