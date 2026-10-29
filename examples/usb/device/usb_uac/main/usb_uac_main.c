/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "usb_device_uac.h"

static const char *TAG = "usb_uac_main";

static dev_audio_codec_handles_t *s_playback_codec;
static dev_audio_codec_handles_t *s_record_codec;

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    ESP_RETURN_ON_FALSE(s_playback_codec && s_playback_codec->codec_dev,
                        ESP_ERR_INVALID_STATE, TAG, "playback codec is unavailable");
    return esp_codec_dev_write(s_playback_codec->codec_dev, buf, len) == ESP_CODEC_DEV_OK
           ? ESP_OK : ESP_FAIL;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    ESP_RETURN_ON_FALSE(s_record_codec && s_record_codec->codec_dev,
                        ESP_ERR_INVALID_STATE, TAG, "record codec is unavailable");
    int ret = esp_codec_dev_read(s_record_codec->codec_dev, buf, len);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "i2s read failed");
        return ESP_FAIL;
    }
    *bytes_read = len;
    return ESP_OK;
}

static void uac_device_set_mute_cb(uint32_t mute, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    if (s_playback_codec && s_playback_codec->codec_dev) {
        esp_codec_dev_set_out_mute(s_playback_codec->codec_dev, mute);
    }
}

static void uac_device_set_volume_cb(uint32_t volume, void *arg)
{
    ESP_LOGI(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    if (s_playback_codec && s_playback_codec->codec_dev) {
        esp_codec_dev_set_out_vol(s_playback_codec->codec_dev, volume);
    }
}

static esp_err_t uac_open_codec(const char *name, uint8_t channels,
                                dev_audio_codec_handles_t **codec)
{
    ESP_RETURN_ON_FALSE(esp_board_manager_check_name(name), ESP_ERR_NOT_FOUND, TAG,
                        "selected board has no %s device", name);
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(name), TAG,
                        "initialize %s failed", name);
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(name, (void **)codec), TAG,
                        "get %s handle failed", name);
    ESP_RETURN_ON_FALSE(*codec && (*codec)->codec_dev, ESP_ERR_INVALID_STATE, TAG,
                        "%s returned an invalid codec handle", name);

    esp_codec_dev_sample_info_t sample_info = {
        .sample_rate = CONFIG_UAC_SAMPLE_RATE,
        .channel = channels,
        .bits_per_sample = CONFIG_UAC_BYTES_PER_SAMPLE * 8,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open((*codec)->codec_dev, &sample_info) == ESP_CODEC_DEV_OK,
                        ESP_FAIL, TAG, "open %s failed", name);
    return ESP_OK;
}

void app_main(void)
{
#if !CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT
    ESP_LOGE(TAG, "Selected Board Manager board has no audio codec device");
    return;
#else
    ESP_ERROR_CHECK(esp_board_manager_print_board_info());

#if CONFIG_UAC_SPEAKER_CHANNEL_NUM > 0
    ESP_ERROR_CHECK(uac_open_codec(ESP_BOARD_DEVICE_NAME_AUDIO_DAC,
                                   CONFIG_UAC_SPEAKER_CHANNEL_NUM,
                                   &s_playback_codec));
    esp_codec_dev_set_out_vol(s_playback_codec->codec_dev, 60);
#endif
#if CONFIG_UAC_MIC_CHANNEL_NUM > 0
    ESP_ERROR_CHECK(uac_open_codec(ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
                                   CONFIG_UAC_MIC_CHANNEL_NUM,
                                   &s_record_codec));
#endif

    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };

    ESP_ERROR_CHECK(uac_device_init(&config));
#endif
}
