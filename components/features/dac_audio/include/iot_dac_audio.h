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

#ifndef _IOT_DAC_AUDIO_H_
#define _IOT_DAC_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "driver/i2s.h"

typedef void* dac_audio_handle_t;

/**
 * @brief Create and init DAC object and return a handle
 *
 * @param i2s_num I2S port number
 * @param sample_rate DAC file sample rate
 * @param sample_bits DAC file sample bits
 * @param dac_mode DAC output mode
 * @param dma_size I2S DMA buffer size
 * @param init_i2s whether to initialize I2S driver
 * @return
 *     - NULL Fail
 *     - Others Success
 */
dac_audio_handle_t iot_dac_audio_create(i2s_port_t i2s_num, int sample_rate, int sample_bits, i2s_dac_mode_t dac_mode, int dma_size, bool init_i2s);

/**
 * @brief Delete and release a DAC object
 * @param dac_audio object handle of DAC
 * @param delete_i2s Whether to delete the I2S driver
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t iot_dac_audio_delete(dac_audio_handle_t dac_audio, bool delete_i2s);

/**
 * @brief play audio file
 * @param dac_audio object handle of DAC
 * @param data data to play
 * @param length data length
 * @param ticks_to_wait max block ticks
 * @return
 *     - ESP_OK on success
 */
esp_err_t iot_dac_audio_play(dac_audio_handle_t dac_audio, const uint8_t* data, int length, TickType_t ticks_to_wait);



#ifdef __cplusplus
}
#endif

#endif
