/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "audio_codec_data_if.h"
#include "adc_mic.h"
#include "esp_codec_dev_defaults.h"
#include "esp_idf_version.h"
#include "soc/soc_caps.h"

static const char *TAG = "adc_if";
typedef struct {
    audio_codec_data_if_t       base;
    adc_continuous_handle_t     handle;
    adc_unit_t                  unit_id;             /*!< ADC unit */
    adc_atten_t                 atten;               /*!< ADC attenuation */
    bool                        if_config_by_user;   /*!< If the ADC is configured by the user, the internal initialization will not be performed. */
    bool                        is_open;
    bool                        enable;
    uint8_t                     *adc_channel;
    uint8_t                     adc_channel_num;
    uint32_t                    conv_frame_size;
#if SOC_ADC_DIGI_RESULT_BYTES != 2
    uint32_t                    *conv_data;
#endif
} adc_data_t;

static esp_err_t adc_channel_config(adc_data_t *adc_data, uint8_t *channel, uint8_t channel_num, int sample_freq_hz, adc_atten_t atten)
{
    adc_digi_convert_mode_t conv_mode = adc_data->unit_id == ADC_UNIT_1 ? ADC_CONV_SINGLE_UNIT_1 : ADC_CONV_SINGLE_UNIT_2;
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = sample_freq_hz * channel_num,
        .conv_mode = conv_mode,
    };

    /**
     * @brief For ESP32 and ESP32-S2, only `type1` can obtain 12-bit data.
     *
     */
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
#else
    dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
#endif

    adc_digi_pattern_config_t *adc_pattern = calloc(channel_num, sizeof(adc_digi_pattern_config_t));
    ESP_RETURN_ON_FALSE(adc_pattern != NULL, ESP_ERR_NO_MEM, TAG, "adc_pattern is NULL");
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < dig_cfg.pattern_num; i++) {
        adc_pattern[i].atten = atten;
        adc_pattern[i].channel = channel[i];
        adc_pattern[i].unit = adc_data->unit_id;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    esp_err_t ret = adc_continuous_config(adc_data->handle, &dig_cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "adc_continuous_config failed");
    free(adc_pattern);
    return ESP_OK;
}

static bool _adc_data_is_open(const audio_codec_data_if_t *h)
{
    adc_data_t *adc_data = (adc_data_t *) h;
    if (adc_data) {
        return adc_data->is_open;
    }
    return false;
}

static int _adc_data_enable(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, bool enable)
{
    adc_data_t *adc_data = (adc_data_t *) h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "adc_data is NULL");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "adc_data is not open");
    ESP_RETURN_ON_FALSE(dev_type == ESP_CODEC_DEV_TYPE_IN, ESP_CODEC_DEV_INVALID_ARG, TAG, "Invalid device type");
    esp_err_t ret = ESP_OK;
    if (enable) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        ret = adc_continuous_flush_pool(adc_data->handle);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CODEC_DEV_DRV_ERR, TAG, "adc_continuous_flush_pool failed");
#endif
        ret = adc_continuous_start(adc_data->handle);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CODEC_DEV_DRV_ERR, TAG, "adc_continuous_start failed");
    } else {
        ret = adc_continuous_stop(adc_data->handle);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CODEC_DEV_DRV_ERR, TAG, "adc_continuous_stop failed");
    }

    adc_data->enable = enable;

    return ret;
}

/**
 * @brief Read ADC data from the continuous ADC interface.
 *
 * This function reads ADC data from the continuous ADC interface and processes the raw values.
 * The data is left-shifted to amplify the audio signal. The function ensures that the requested
 * read size is valid and properly aligned with the ADC data format.
 *
 * @param h     Pointer to the audio codec data interface handle.
 * @param data  Pointer to the buffer where the ADC data will be stored.
 * @param size  Number of bytes to read, must be a multiple of SOC_ADC_DIGI_DATA_BYTES_PER_CONV.
 * @return int  Returns ESP_CODEC_DEV_OK on success, or an error code on failure.
 */
static int _adc_data_read(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    adc_data_t *adc_data = (adc_data_t *) h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "adc_data is NULL");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "adc_data is not open");

    if (size % SOC_ADC_DIGI_DATA_BYTES_PER_CONV != 0) {
        ESP_LOGE(TAG, "Invalid size, must be multiple of %d", SOC_ADC_DIGI_DATA_BYTES_PER_CONV);
    }

    ESP_LOGV(TAG, "adc mic read %d bytes", size);

    // Note: must be 16bit
    uint32_t ret_num = 0;
    int left_size = size;
    int cnt = 0;
#if SOC_ADC_DIGI_RESULT_BYTES == 2
    while (left_size > 0) {
        int request_size = left_size > adc_data->conv_frame_size ? adc_data->conv_frame_size : left_size;
        adc_continuous_read(adc_data->handle, &data[cnt], request_size, &ret_num, portMAX_DELAY);
        adc_digi_output_data_t *buffer = (adc_digi_output_data_t *)&data[cnt];
        uint16_t *p = (uint16_t *)&data[cnt];
        size_t item_count = ret_num / sizeof(adc_digi_output_data_t);
        for (int i = 0; i < item_count; i++) {
            uint16_t raw_value = buffer[i].val;
            // Left shift to amplify audio.
            p[i] = (raw_value << 4) - 32768;
        }

        cnt += ret_num;
        left_size -= ret_num;
    }
#else
    while (left_size > 0) {
        int request_size = left_size > adc_data->conv_frame_size ? adc_data->conv_frame_size : left_size;
        adc_continuous_read(adc_data->handle, (uint8_t *)adc_data->conv_data, request_size * 2, &ret_num, portMAX_DELAY);
        adc_digi_output_data_t *buffer = (adc_digi_output_data_t *)adc_data->conv_data;
        uint16_t *p = (uint16_t *)&data[cnt];
        size_t item_count = ret_num / sizeof(adc_digi_output_data_t);
        for (int i = 0; i < item_count; i++) {
            uint16_t raw_value = buffer[i].val & 0xFFFF;
            // Left shift to amplify audio.
            p[i] = (raw_value << 4) - 32768;
        }
        cnt += ret_num / 2;
        left_size -= ret_num / 2;
    }
#endif

    return ESP_CODEC_DEV_OK;
}

static int _adc_data_close(const audio_codec_data_if_t *h)
{
    adc_data_t *adc_data = (adc_data_t *) h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "adc_data is NULL");
    ESP_RETURN_ON_FALSE(adc_data->enable == false, ESP_CODEC_DEV_WRONG_STATE, TAG, "adc_data is enable, please disable it first");

    if (!adc_data->if_config_by_user) {
        adc_continuous_deinit(adc_data->handle);
    }

#if SOC_ADC_DIGI_RESULT_BYTES != 2
    if (adc_data->conv_data) {
        free(adc_data->conv_data);
    }
#endif
    if (adc_data->adc_channel) {
        free(adc_data->adc_channel);
    }

    // adc_data will be deleted by the esp_code_dev interface.
    return ESP_CODEC_DEV_OK;
}

/**
 * @brief Configure the format of ADC data.
 *
 * This function sets the sample format for the ADC data interface. It ensures that the ADC is properly configured
 * with the expected sample rate, channel count, and bit depth. Before setting the format, the ADC should be stopped.
 *
 * @note adc_data only supports 16-bit samples
 *
 * @param h         Pointer to the audio codec data interface handle.
 * @param dev_type  Type of the codec device (not used in this function).
 * @param fs        Pointer to the sample format structure, specifying sample rate, channels, and bit depth.
 * @return int      Returns ESP_CODEC_DEV_OK on success, or an error code on failure.
 */
static int _adc_data_set_fmt(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, esp_codec_dev_sample_info_t *fs)
{
    adc_data_t *adc_data = (adc_data_t *) h;
    ESP_RETURN_ON_FALSE(adc_data != NULL, ESP_CODEC_DEV_INVALID_ARG, TAG, "adc_data is NULL");
    ESP_RETURN_ON_FALSE(adc_data->is_open, ESP_CODEC_DEV_WRONG_STATE, TAG, "adc_data is not open");
    ESP_RETURN_ON_FALSE(fs->bits_per_sample == 16, ESP_CODEC_DEV_INVALID_ARG, TAG, "adc_data only support 16-bit samples");
    ESP_RETURN_ON_FALSE(fs->channel == adc_data->adc_channel_num, ESP_CODEC_DEV_INVALID_ARG, TAG, "channel num not match");

    esp_err_t ret = adc_channel_config(adc_data, adc_data->adc_channel, adc_data->adc_channel_num, fs->sample_rate, adc_data->atten);
    return ret == ESP_OK ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

const audio_codec_data_if_t *audio_codec_new_adc_data(audio_codec_adc_cfg_t *adc_cfg)
{
    ESP_LOGI(TAG, "ADC MIC Version: %d.%d.%d", ADC_MIC_VER_MAJOR, ADC_MIC_VER_MINOR, ADC_MIC_VER_PATCH);
    ESP_RETURN_ON_FALSE(adc_cfg != NULL, NULL, TAG, "adc_cfg is NULL");
    esp_err_t ret = ESP_OK;

    adc_data_t *adc_data = (adc_data_t *)calloc(1, sizeof(adc_data_t));
    ESP_RETURN_ON_FALSE(adc_data != NULL, NULL, TAG, "calloc failed");

    adc_data->conv_frame_size = adc_cfg->conv_frame_size;
    /**
     * @brief If the conversion result takes up 4 bytes, we need to use a temporary
     *        buffer for the conversion instead of copying it directly into the provided
     *        buffer, as this would result in some efficiency loss.
     */
#if SOC_ADC_DIGI_RESULT_BYTES != 2
    adc_data->conv_data = calloc(1, adc_data->conv_frame_size * 2);
    ESP_GOTO_ON_FALSE(adc_data->conv_data != NULL, ESP_CODEC_DEV_NO_MEM, err, TAG, "calloc failed");
#endif

    adc_data->adc_channel = malloc(adc_cfg->adc_channel_num * sizeof(uint8_t));
    ESP_GOTO_ON_FALSE(adc_data->adc_channel != NULL, ESP_CODEC_DEV_NO_MEM, err, TAG, "malloc failed");
    adc_data->adc_channel_num = adc_cfg->adc_channel_num;

    if (adc_cfg->handle == NULL) {
        adc_continuous_handle_cfg_t adc_config = {
            .max_store_buf_size = adc_cfg->max_store_buf_size,
            .conv_frame_size = adc_cfg->conv_frame_size,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
            .flags.flush_pool = true,
#endif
        };

        ret = adc_continuous_new_handle(&adc_config, &adc_data->handle);
        ESP_GOTO_ON_FALSE(ret == ESP_OK, ESP_CODEC_DEV_DRV_ERR, err, TAG, "adc_continuous_new_handle failed");
        adc_data->if_config_by_user = false;
    } else {
        adc_data->handle = *adc_cfg->handle;
        adc_data->if_config_by_user = true;
    }

    adc_data->unit_id = adc_cfg->unit_id;
    adc_data->atten = adc_cfg->atten;

    memcpy(adc_data->adc_channel, adc_cfg->adc_channel_list, adc_cfg->adc_channel_num * sizeof(uint8_t));
    adc_channel_config(adc_data, adc_data->adc_channel, adc_data->adc_channel_num, adc_cfg->sample_rate_hz, adc_cfg->atten);

    adc_data->base.open = NULL;
    adc_data->base.is_open = _adc_data_is_open;
    adc_data->base.enable = _adc_data_enable;
    adc_data->base.read = _adc_data_read;
    adc_data->base.write = NULL;
    adc_data->base.set_fmt = _adc_data_set_fmt;
    adc_data->base.close = _adc_data_close;

    adc_data->is_open = true;
    return &adc_data->base;

err:
    if (adc_data) {
        free(adc_data);
    }
    return NULL;
}
