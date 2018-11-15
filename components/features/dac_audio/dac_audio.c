// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
    esp_err_t ret;
    size_t write_len = 0;
    size_t total_len = length;
    dac_audio_t *dac = (dac_audio_t*) dac_audio;
    while(1) {
        ret = i2s_write(dac->i2s_num, (const char*) data + length - total_len, total_len, &write_len, ticks_to_wait);
        total_len -= write_len;
        if(total_len == 0 || ret != ESP_OK) {
            break;
        }
    }
    return ret;
}


