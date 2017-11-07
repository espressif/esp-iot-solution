/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/dac.h"
#include "iot_dac_audio.h"

typedef struct {
    i2s_port_t i2s_num;
    int sample_rate;
    int sample_bits;
    int channel_num;
} dac_audio_t;

dac_audio_handle_t iot_dac_audio_create(i2s_port_t i2s_num, int sample_rate, int sample_bits, i2s_dac_mode_t dac_mode, int dma_size, bool init_i2s)
{
    dac_audio_t *dac = (dac_audio_t*) calloc(sizeof(dac_audio_t), 1);
    if(init_i2s) {
        i2s_config_t i2s_config = {
           .mode = I2S_MODE_MASTER | I2S_MODE_TX |I2S_MODE_DAC_BUILT_IN,
           .sample_rate = sample_rate,
           .bits_per_sample = sample_bits,                                    //8, 16, 24, 32
           .communication_format = I2S_COMM_FORMAT_I2S_MSB,                           //or PDM
           .channel_format = dac_mode == I2S_DAC_CHANNEL_BOTH_EN ? I2S_CHANNEL_FMT_RIGHT_LEFT : I2S_CHANNEL_FMT_ONLY_LEFT,             //format LEFT_RIGHT
           .intr_alloc_flags = 0,
           .dma_buf_count = 2,
           .dma_buf_len = dma_size,
        };
        i2s_driver_install(i2s_num, &i2s_config, 0, NULL);   //install and start i2s driver
    }
    i2s_set_dac_mode(dac_mode);
    return (dac_audio_handle_t) dac;
}

esp_err_t iot_dac_audio_delete(dac_audio_handle_t dac_audio, bool delete_i2s)
{
    dac_audio_t *dac = (dac_audio_t*) dac_audio;
    if (delete_i2s) {
        i2s_driver_uninstall(dac->i2s_num);
    }
    free(dac);
    dac = NULL;
    return ESP_OK;
}

esp_err_t iot_dac_audio_play(dac_audio_handle_t dac_audio, const uint8_t* data, int length, TickType_t ticks_to_wait)
{
    dac_audio_t *dac = (dac_audio_t*) dac_audio;
    i2s_write_bytes(dac->i2s_num, (const char*) data, length, ticks_to_wait);
    return ESP_OK;
}


