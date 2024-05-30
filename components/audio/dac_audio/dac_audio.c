/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2s.h"
#include "dac_audio.h"

static const char *TAG = "DAC audio";

#define DAC_AUDIO_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define VOLUME_0DB          (16)

static i2s_port_t g_i2s_num = 0;
static int g_bits_per_sample = 0;
static uint8_t *g_i2s_write_buff = NULL;
static uint32_t g_max_data_size = 0;
static int32_t  g_volume = 0;                          /*!< the volume(-VOLUME_0DB ~ VOLUME_0DB) */

esp_err_t dac_audio_set_param(int rate, int bits, int ch)
{
    i2s_set_clk(g_i2s_num, rate, bits, ch);
    g_bits_per_sample = bits;
    return ESP_OK;
}

esp_err_t dac_audio_set_volume(int8_t volume)
{
    if (volume < 0) {
        DAC_AUDIO_CHECK(-volume <= VOLUME_0DB, "Volume is too small", ESP_ERR_INVALID_ARG);
    } else {
        DAC_AUDIO_CHECK(volume <= VOLUME_0DB, "Volume is too large", ESP_ERR_INVALID_ARG);
    }

    g_volume = volume + VOLUME_0DB;
    return ESP_OK;
}

/**
 * @brief Scale data to 16bit/32bit for I2S DMA output.
 *        DAC can only output 8bit data value.
 *        I2S DMA will still send 16 bit or 32bit data, the highest 8bit contains DAC data.
 */
static esp_err_t _i2s_dac_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len, uint32_t *out_len)
{
    uint32_t j = 0;
    switch (g_bits_per_sample) {
    case 8: {
        DAC_AUDIO_CHECK(g_max_data_size > len, "write length exceed max_data_size you set", ESP_ERR_INVALID_ARG);
        uint8_t *b8 = (uint8_t *)s_buff;
        for (int i = 0; i < len; i++) {
            int8_t t = b8[i] ;
            t = t * g_volume / VOLUME_0DB;
            t = t + 0x7f;
            d_buff[j++] = t;
        }
    }
    break;
    case 16: {
        DAC_AUDIO_CHECK(g_max_data_size > (len << 1), "write length exceed max_data_size you set", ESP_ERR_INVALID_ARG);
        len >>= 1;
        uint16_t *b16 = (uint16_t *)s_buff;
        for (int i = 0; i < len; i++) {
            int16_t t = b16[i] ;
            t = t * g_volume / VOLUME_0DB;
            t = t + 0x7fff;
            d_buff[j++] = 0;
            d_buff[j++] = t >> 8;
        }
    }
    break;
    case 32: {
        DAC_AUDIO_CHECK(g_max_data_size > (len << 2), "write length exceed max_data_size you set", ESP_ERR_INVALID_ARG);
        len >>= 2;
        uint32_t *b32 = (uint32_t *)s_buff;
        for (int i = 0; i < len; i++) {
            int32_t t = b32[i] ;
            t = t * g_volume / VOLUME_0DB;
            t = t + 0x7fffffff;
            d_buff[j++] = 0;
            d_buff[j++] = 0;
            d_buff[j++] = 0;
            d_buff[j++] = t >> 8;
        }
    }
    break;

    default:
        break;
    }
    *out_len = j;
    return ESP_OK;
}

esp_err_t dac_audio_start(void)
{
    esp_err_t ret;
    ret = i2s_start(g_i2s_num);
    DAC_AUDIO_CHECK(ESP_OK == ret, "dac start failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t dac_audio_stop(void)
{
    esp_err_t ret = ESP_OK;
    ret |= i2s_zero_dma_buffer(g_i2s_num);
    ret |= i2s_stop(g_i2s_num);
    DAC_AUDIO_CHECK(ESP_OK == ret, "dac stop failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t dac_audio_write(uint8_t *inbuf, size_t len, size_t *bytes_written, TickType_t ticks_to_wait)
{
    DAC_AUDIO_CHECK(NULL != inbuf, "Pointer of buffer is invalid", ESP_ERR_INVALID_ARG);
    DAC_AUDIO_CHECK(0 != len, "Length of data is 0", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ESP_OK;
    size_t i2s_wr_len = 0;
    ret |= _i2s_dac_data_scale(g_i2s_write_buff, inbuf, len, &i2s_wr_len);
    ret |= i2s_write(g_i2s_num, g_i2s_write_buff, i2s_wr_len, bytes_written, ticks_to_wait);
    DAC_AUDIO_CHECK(ESP_OK == ret, "i2s write data failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t dac_audio_init(dac_audio_config_t *cfg)
{
    DAC_AUDIO_CHECK((8 == cfg->bits_per_sample) || (16 == cfg->bits_per_sample) || (32 == cfg->bits_per_sample),
                    "Only support 8bit 16bit 32bit", ESP_ERR_INVALID_ARG);
    DAC_AUDIO_CHECK(cfg->sample_rate <= 48000, "Sample_rate must be less than 48000", ESP_ERR_INVALID_ARG);
    DAC_AUDIO_CHECK((cfg->max_data_size != 0) || (cfg->max_data_size < 48 * 1024), "max_data_size is invalid", ESP_ERR_INVALID_ARG);

    esp_err_t ret = ESP_OK;
    i2s_config_t i2s_config = {0};
    i2s_config.mode = I2S_MODE_MASTER |  I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN;
    i2s_config.sample_rate =  cfg->sample_rate;
    i2s_config.bits_per_sample = cfg->bits_per_sample;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 2, 0))
    i2s_config.communication_format = I2S_COMM_FORMAT_I2S_MSB;
#else
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_MSB;
#endif
    if (I2S_DAC_CHANNEL_RIGHT_EN == cfg->dac_mode) {
        i2s_config.channel_format = I2S_CHANNEL_FMT_ALL_RIGHT;
    } else if (I2S_DAC_CHANNEL_LEFT_EN == cfg->dac_mode) {
        i2s_config.channel_format = I2S_CHANNEL_FMT_ALL_LEFT;
    } else if (I2S_DAC_CHANNEL_BOTH_EN == cfg->dac_mode) {
        i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    } else {
        ESP_LOGE(TAG, "Unsupported DAC mode");
        return ESP_ERR_INVALID_ARG;
    }

    i2s_config.intr_alloc_flags = 0;
    i2s_config.dma_buf_count = cfg->dma_buf_count;
    i2s_config.dma_buf_len = cfg->dma_buf_len;
    i2s_config.use_apll = 0;
    i2s_config.fixed_mclk = 0;

    g_bits_per_sample = cfg->bits_per_sample;
    //install and start i2s driver
    ret = i2s_driver_install(cfg->i2s_num, &i2s_config, 0, NULL);
    DAC_AUDIO_CHECK(ESP_OK == ret, "i2s driver install failed", ESP_FAIL);
    //init DAC pad
    ret = i2s_set_dac_mode(cfg->dac_mode);
    DAC_AUDIO_CHECK(ESP_OK == ret, "i2s set dac mode failed", ESP_FAIL);
    //init ADC pad
    ESP_LOGI(TAG, "DAC audio initialize successfully");

    g_i2s_num = cfg->i2s_num;
    g_max_data_size = cfg->max_data_size;
    g_i2s_write_buff = (uint8_t *) calloc(cfg->max_data_size, sizeof(char));
    DAC_AUDIO_CHECK(NULL != g_i2s_write_buff, "Not enough memory for write_buffer", ESP_ERR_NO_MEM);

    dac_audio_set_volume(0);

    return ESP_OK;
}

esp_err_t dac_audio_deinit(void)
{
    i2s_driver_uninstall(g_i2s_num);
    free(g_i2s_write_buff);
    return ESP_OK;
}

#endif
