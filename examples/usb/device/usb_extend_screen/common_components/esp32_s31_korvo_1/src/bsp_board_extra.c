/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "bsp/bsp_board_extra.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "s31_bsp_extra";

static esp_codec_dev_handle_t play_dev_handle;
static esp_codec_dev_handle_t record_dev_handle;
static bool s_audio_codec_ready;
static int s_volume = CODEC_DEFAULT_VOLUME;

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    (void)timeout_ms;

    esp_err_t ret = esp_codec_dev_read(record_dev_handle, audio_buffer, len);
    if (bytes_read) {
        *bytes_read = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    (void)timeout_ms;

    esp_err_t ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    if (bytes_written) {
        *bytes_written = (ret == ESP_OK) ? len : 0;
    }
    return ret;
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }
    if (record_dev_handle) {
        ret |= esp_codec_dev_close(record_dev_handle);
        ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    }
    if (play_dev_handle) {
        ret |= esp_codec_dev_open(play_dev_handle, &fs);
    }
    if (record_dev_handle) {
        ret |= esp_codec_dev_open(record_dev_handle, &fs);
    }
    return ret;
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, volume), TAG, "set codec volume failed");
    s_volume = volume;
    if (volume_set) {
        *volume_set = volume;
    }
    return ESP_OK;
}

int bsp_extra_codec_volume_get(void)
{
    return s_volume;
}

esp_err_t bsp_extra_codec_mute_set(bool enable)
{
    return esp_codec_dev_set_out_mute(play_dev_handle, enable);
}

esp_err_t bsp_extra_codec_dev_stop(void)
{
    esp_err_t ret = ESP_OK;

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }
    if (record_dev_handle) {
        ret |= esp_codec_dev_close(record_dev_handle);
    }
    return ret;
}

esp_err_t bsp_extra_codec_dev_resume(void)
{
    return bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

esp_err_t bsp_extra_codec_init(void)
{
    if (s_audio_codec_ready) {
        return ESP_OK;
    }

    play_dev_handle = bsp_audio_codec_speaker_init();
    assert((play_dev_handle) && "play_dev_handle not initialized");

    record_dev_handle = bsp_audio_codec_microphone_init();
    assert((record_dev_handle) && "record_dev_handle not initialized");

    ESP_RETURN_ON_ERROR(bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE,
                                               CODEC_DEFAULT_BIT_WIDTH,
                                               CODEC_DEFAULT_CHANNEL),
                        TAG, "open codec failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL), TAG, "set default volume failed");
    s_audio_codec_ready = true;
    return ESP_OK;
}
