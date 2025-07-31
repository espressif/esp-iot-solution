/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_hls_extractor.h"
#include "esp_wav_extractor.h"
#include "esp_mp4_extractor.h"
#include "esp_audio_es_extractor.h"
#include "esp_ogg_extractor.h"
#include "esp_ts_extractor.h"
#include "esp_avi_extractor.h"
#include "esp_caf_extractor.h"
#include "esp_flv_extractor.h"

esp_extr_err_t esp_extractor_register_all(void)
{
    esp_extr_err_t ret = ESP_EXTR_ERR_OK;
#ifdef CONFIG_WAV_EXTRACTOR_SUPPORT
    ret |= esp_wav_extractor_register();
#endif
#ifdef CONFIG_MP4_EXTRACTOR_SUPPORT
    ret |= esp_mp4_extractor_register();
#endif
#ifdef CONFIG_TS_EXTRACTOR_SUPPORT
    ret |= esp_ts_extractor_register();
#endif
#ifdef CONFIG_HLS_EXTRACTOR_SUPPORT
    ret |= esp_hls_extractor_register();
#endif
#ifdef CONFIG_OGG_EXTRACTOR_SUPPORT
    ret |= esp_ogg_extractor_register();
#endif
#ifdef CONFIG_AVI_EXTRACTOR_SUPPORT
    ret |= esp_avi_extractor_register();
#endif
#ifdef CONFIG_AUDIO_ES_EXTRACTOR_SUPPORT
    ret |= esp_audio_es_extractor_register();
#endif
#ifdef CONFIG_CAF_EXTRACTOR_SUPPORT
    ret |= esp_caf_extractor_register();
#endif
#ifdef CONFIG_FLV_EXTRACTOR_SUPPORT
    ret |= esp_flv_extractor_register();
#endif
    return ret;
}
