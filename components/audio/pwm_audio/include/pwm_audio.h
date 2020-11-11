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


#ifndef _PWM_AUDIO_H_
#define _PWM_AUDIO_H_
#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif



/**
 * @brief Configuration parameters of pwm audio for pwm_audio_init function
 */
typedef struct
{
    timer_group_t tg_num;             /*!< timer group number (0 - 1) */
    timer_idx_t timer_num;            /*!< timer number  (0 - 1) */

    int gpio_num_left;                  /*!< the LEDC output gpio_num, Left channel */
    int gpio_num_right;                  /*!< the LEDC output gpio_num, Right channel */
    ledc_channel_t ledc_channel_left;   /*!< LEDC channel (0 - 7), Corresponding to left channel*/
    ledc_channel_t ledc_channel_right;   /*!< LEDC channel (0 - 7), Corresponding to right channel*/
    ledc_timer_t ledc_timer_sel;         /*!< Select the timer source of channel (0 - 3) */
    ledc_timer_bit_t duty_resolution;   /*!< ledc pwm bits */

    uint32_t ringbuf_len;            /*!< ringbuffer size */

} pwm_audio_config_t;


/**
 * @brief pwm audio status
 */
typedef enum {
    PWM_AUDIO_STATUS_UN_INIT = 0, /*!< pwm audio uninitialized */
    PWM_AUDIO_STATUS_IDLE = 1, /*!< pwm audio idle */
    PWM_AUDIO_STATUS_BUSY = 2, /*!< pwm audio busy */
} pwm_audio_status_t;


/**
 * @brief pwm audio channel.
 *
 */
typedef enum {
    PWM_AUDIO_CH_MONO        = 0,            /*!< 1 channel (mono)*/
    PWM_AUDIO_CH_STEREO      = 1,            /*!< 2 channel (stereo)*/
    PWM_AUDIO_CH_MAX,
} pwm_audio_channel_t;


/**
 * @brief Initializes and configure the pwm audio.
 *        Configure pwm audio with the given source.
 * 
 * @param cfg Pointer of pwm_audio_config_t struct
 * 
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_init(const pwm_audio_config_t *cfg);


/**
 * @brief Start audio play
 * 
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_start(void);

/**
 * @brief Write data
 *
 * @param inbuf 
 * @param len 
 * @param bytes_written 
 * @param ticks_to_wait 
 * 
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_write(uint8_t *inbuf, size_t len, size_t *bytes_written, TickType_t ticks_to_wait);

/**
 * @brief stop audio play
 * 
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_stop(void);

/**
 * @brief Deinit pwm, timer and gpio
 *
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_deinit(void);

/**
 * @brief Set parameter for pwm audio.
 *
 * Similar to pwm_audio_set_sample_rate(), but also sets bit width.
 *
 * @param rate sample rate (ex: 8000, 44100...)
 * @param bits bit width
 * @param ch channel number
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t pwm_audio_set_param(int rate, ledc_timer_bit_t bits, int ch);

/**
 * @brief Set samplerate for pwm audio.
 *
 * @param rate sample rate (ex: 8000, 44100...)
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t pwm_audio_set_sample_rate(int rate);

/**
 * @brief Set volume for pwm audio. 
 *        !!!Using volume greater than 0 may cause variable overflow and distortion!!!
 *        Usually you should enter a volume less than or equal to 0
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
esp_err_t pwm_audio_set_volume(int8_t volume);

/**
 * @brief Get current volume
 * 
 * @param volume Pointer to volume
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t pwm_audio_get_volume(int8_t *volume);

/**
 * @brief Get parameter for pwm audio.
 *
 * @param rate sample rate
 * @param bits bit width
 * @param ch channel number
 *
 * @return
 *     - ESP_OK              Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t pwm_audio_get_param(int *rate, int *bits, int *ch);

/**
 * @brief get pwm audio status
 * 
 * @param status current pwm_audio status
 * 
 * @return
 *     - ESP_OK Success
 */
esp_err_t pwm_audio_get_status(pwm_audio_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
