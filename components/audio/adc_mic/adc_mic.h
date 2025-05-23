/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ADC_MIC_H
#define ADC_MIC_H

#include <stdint.h>
#include "audio_codec_data_if.h"
#include "esp_adc/adc_continuous.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(_channel, _sample) \
{                                                           \
    .handle = NULL,                                         \
    .max_store_buf_size = 1024 * 2,                         \
    .conv_frame_size = 1024,                                \
    .unit_id = ADC_UNIT_1,                                  \
    .adc_channel_list = ((uint8_t[]){_channel}),            \
    .adc_channel_num = 1,                                   \
    .sample_rate_hz = _sample,                              \
}

#define DEFAULT_AUDIO_CODEC_ADC_STEREO_CFG(_channel_l, _channel_r,_sample) \
{                                                                          \
    .handle = NULL,                                                        \
    .max_store_buf_size = 1024 * 2,                                        \
    .conv_frame_size = 1024,                                               \
    .unit_id = ADC_UNIT_1,                                                 \
    .adc_channel_list = ((uint8_t[]){_channel_l, _channel_r}),             \
    .adc_channel_num = 1,                                                  \
    .atten = ADC_ATTEN_DB_0,                                               \
    .sample_rate_hz = _sample,                                             \
}

/**************************************************************************************************
 *
 * ADC MIC
 *
 * After create a adc mic driver, it can be accessed with stdio functions ie.:
 * \code{.c}
 * const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);
 * esp_codec_dev_cfg_t codec_dev_cfg = {
 *     .dev_type = ESP_CODEC_DEV_TYPE_IN,
 *     .data_if = adc_if,
 * };
 * esp_codec_dev_handle_t dev = esp_codec_dev_new(&codec_dev_cfg);
 * esp_codec_dev_open(dev, &fs);
 * esp_codec_dev_read(dev, audio_buffer, buffer_len);
 * \endcode
 **************************************************************************************************/

/**
 * @brief codec adc configuration
 *
 */
typedef struct {
    adc_continuous_handle_t *handle;   /*!< Type of adc continuous mode driver handle, if NULL will create new one internal, else will use the handle */
    size_t max_store_buf_size;         /*!< Max length of the conversion results that driver can store, in bytes. */
    size_t conv_frame_size;            /*!< Conversion frame size, in bytes. This should be in multiples of `SOC_ADC_DIGI_DATA_BYTES_PER_CONV` */
    adc_unit_t unit_id;                /*!< ADC unit */
    uint8_t *adc_channel_list;         /*!< Channel of ADC */
    uint8_t adc_channel_num;           /*!< Number of channels */
    adc_atten_t atten;                 /*!< It should correspond to the actual range, with 0dB attenuation having the least ripple. */
    uint32_t sample_rate_hz;           /*!< Sample rate of ADC */
} audio_codec_adc_cfg_t;

/**
 * @brief Initialize codec adc.
 *
 * @param adc_cfg pointer of configuration struct
 * @return const audio_codec_data_if_t* adc data interface.
 */
const audio_codec_data_if_t *audio_codec_new_adc_data(audio_codec_adc_cfg_t *adc_cfg);

#ifdef __cplusplus
}
#endif

#endif
