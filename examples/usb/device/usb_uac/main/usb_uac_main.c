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
#include "bsp/esp-bsp.h"
#ifdef CONFIG_ESP32_S3_LCD_EV_BOARD
#include "bsp_board_extra.h"
#else
#include "bsp/bsp_board_extra.h"
#endif
#include "usb_device_uac.h"

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
    bsp_extra_codec_mute_set(mute);
}

static void uac_device_set_volume_cb(uint32_t volume, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    bsp_extra_codec_volume_set(volume, NULL);
}

void app_main(void)
{
    bsp_extra_codec_init();
    bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE, 16, CONFIG_UAC_SPEAKER_CHANNEL_NUM);

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };

    uac_device_init(&config);
}
