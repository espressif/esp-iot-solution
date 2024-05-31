/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DAC_AUDIO_H_
#define _DAC_AUDIO_H_
#include "esp_err.h"
#include "driver/i2s.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration parameters for dac_audio_init function
 */
typedef struct {
    i2s_port_t              i2s_num;                /*!< I2S_NUM_0, I2S_NUM_1*/
    int                     sample_rate;            /*!< I2S sample rate*/
    i2s_bits_per_sample_t   bits_per_sample;        /*!< I2S bits per sample*/
    i2s_dac_mode_t          dac_mode;               /*!< DAC mode configurations - see i2s_dac_mode_t*/
    int                     dma_buf_count;          /*!< DMA buffer count, number of buffer*/
    int                     dma_buf_len;            /*!< DMA buffer length, length of each buffer*/
    uint32_t                max_data_size;          /*!< one time max write data size */

} dac_audio_config_t;

/**
 * @brief initialize i2s built-in dac to play audio with
 *
 * @attention only support ESP32, because i2s of ESP32S2 not have a built-in dac
 *
 * @param cfg configurations - see dac_audio_config_t struct
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_FAIL            Encounter error
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM      Out of memory
 */
esp_err_t dac_audio_init(dac_audio_config_t *cfg);

/**
 * @brief deinitialize dac
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_FAIL            Encounter error
 */
esp_err_t dac_audio_deinit(void);

/**
 * @brief Start dac to play
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_FAIL            Encounter error
 */
esp_err_t dac_audio_start(void);

/**
 * @brief Stop play
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_FAIL            Encounter error
 */
esp_err_t dac_audio_stop(void);

/**
 * @brief Configuration dac parameter
 *
 * @param rate sample rate (ex: 8000, 44100...)
 * @param bits bit width
 * @param ch channel number
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_FAIL            Encounter error
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t dac_audio_set_param(int rate, int bits, int ch);

/**
 * @brief Set volume
 *
 * @attention Using volume greater than 0 may cause variable overflow and distortion
 *            Usually you should enter a volume less than or equal to 0
 *
 * @param volume Volume to set (-16 ~ 16), see Macro VOLUME_0DB
 *        Set to 0 for original output;
 *        Set to less then 0 for attenuation, and -16 is mute;
 *        Set to more than 0 for enlarge, and 16 is double output
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t dac_audio_set_volume(int8_t volume);

/**
 * @brief Write data to play
 *
 * @param inbuf Pointer source data to write
 * @param len length of data in bytes
 * @param[out] bytes_written Number of bytes written, if timeout, the result will be less than the size passed in.
 * @param ticks_to_wait TX buffer wait timeout in RTOS ticks. If this
 * many ticks pass without space becoming available in the DMA
 * transmit buffer, then the function will return (note that if the
 * data is written to the DMA buffer in pieces, the overall operation
 * may still take longer than this timeout.) Pass portMAX_DELAY for no
 * timeout.
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Write encounter error
 *     - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t dac_audio_write(uint8_t* inbuf, size_t len, size_t *bytes_written, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif
