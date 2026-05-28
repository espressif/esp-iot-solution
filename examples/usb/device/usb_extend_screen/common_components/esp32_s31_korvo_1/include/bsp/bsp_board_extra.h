/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "driver/i2s_std.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CODEC_DEFAULT_SAMPLE_RATE       (48000)
#define CODEC_DEFAULT_BIT_WIDTH         (16)
#define CODEC_DEFAULT_ADC_VOLUME        (24.0)
#define CODEC_DEFAULT_CHANNEL           (2)
#define CODEC_DEFAULT_VOLUME            (60)

esp_err_t bsp_extra_codec_mute_set(bool enable);
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);
int bsp_extra_codec_volume_get(void);
esp_err_t bsp_extra_codec_dev_stop(void);
esp_err_t bsp_extra_codec_dev_resume(void);
esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);
esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms);
esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);
esp_err_t bsp_extra_codec_init(void);

#ifdef __cplusplus
}
#endif
