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
#ifndef _IOT_LIGHT_H_
#define _IOT_LIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/ledc.h"

typedef void* light_handle_t;

typedef enum {
    LIGHT_SET_DUTY_DIRECTLY = 0,    /*!< set duty directly */
    LIGHT_DUTY_FADE_1S,             /*!< set duty with fade in 1 second */
    LIGHT_DUTY_FADE_2S,             /*!< set duty with fade in 2 second */
    LIGHT_DUTY_FADE_3S,             /*!< set duty with fade in 3 second */
    LIGHT_DUTY_FADE_MAX,            /*!< user shouldn't use this */
} light_duty_mode_t;

typedef enum {
    LIGHT_CH_NUM_1 = 1,             /*!< Light channel number */
    LIGHT_CH_NUM_2 = 2,             /*!< Light channel number */
    LIGHT_CH_NUM_3 = 3,             /*!< Light channel number */
    LIGHT_CH_NUM_4 = 4,             /*!< Light channel number */
    LIGHT_CH_NUM_5 = 5,             /*!< Light channel number */
    LIGHT_CH_NUM_MAX,               /*!< user shouldn't use this */
} light_channel_num_t;
#define LIGHT_MAX_CHANNEL_NUM   (5)
/**
  * @brief  light initialize
  *
  * @param  timer the ledc timer used by light 
  * @param  speed_mode
  * @param  freq_hz frequency of timer
  * @param  channel_num decide how many channels the light contains
  * @param  timer_bit 
  *
  * @return  the handle of light
  */
light_handle_t iot_light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num, ledc_timer_bit_t timer_bit);

/**
  * @brief  add an output channel to light
  *
  * @param  light_handle 
  * @param  channel_idx the id of channel (0 ~ channel_num-1)
  * @param  io_num
  * @param  channel the ledc channel you want to use
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_channel_regist(light_handle_t light_handle, uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel);

/**
  * @brief  free the momery of light
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_delete(light_handle_t light_handle);

/**
  * @brief  set the duty of a channel
  *
  * @param  light_handle
  * @param  channel_id
  * @param  duty
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode);

/**
  * @brief  set a light channel to breath mode
  *
  * @param  light_handle
  * @param  channel_id
  * @param  breath_period
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_breath_write(light_handle_t light_handle, uint8_t channel_id, int breath_period);

/**
  * @brief  set some channels to blink,the others would be turned off
  *
  * @param  light_handle
  * @param  channel_mask the mask to choose blink channels
  * @param  period_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_blink_starte(light_handle_t light_handle, uint32_t channel_mask, uint32_t period_ms);

/**
  * @brief  set some channels to blink,the others would be turned off
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_blink_stop(light_handle_t light_handle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class CControllable
{
public:
    virtual esp_err_t on() = 0;
    virtual esp_err_t off() = 0;
    virtual ~CControllable() = 0;
};

/**
 * class of light with at most 5 pwm channels
 * simple usage:
 * CLight my_light(LIGHT_CH_NUM_3);
 * my_light.red.init(0, LEDC_CHANNEL_0);
 * my_light.green.init(1, LEDC_CHANNEL_1);
 * my_light.blue.init(2, LEDC_CHANNEL_2);
 * my_light.red.duty(my_light.get_full_duty());
 * my_light.green.duty((my_light.get_full_duty() / 3));
 * my_light.blue.duty((my_light.get_full_duty() / 2));
 */
class CLight: public CControllable
{
private:
    /**
     * class of light's pwm channel
     */
    class CLightChannel
    {
    private:
        // gpio number of light channel
        gpio_num_t m_io_num = GPIO_NUM_MAX;
        // ledc channel of light channel
        ledc_channel_t m_ledc_idx = LEDC_CHANNEL_MAX;
        int m_ch_idx;
        uint32_t m_ch_full_duty;
        // pointer to the light which this light channel belongs to
        light_handle_t *mp_channel_handle;
        esp_err_t check_init();
    public:
        uint32_t m_duty = 0;
        /**
         * @brief  constructor of CLightChannel
         *
         * @param  p_light_hanle pointer to the light which this light channel belongs to
         * @param  idx index of channel in a light
         * @param  full_duty
         */
        CLightChannel(light_handle_t *p_light_handle, int idx, uint32_t full_duty);

        /**
         * @brief  set pwm channel to full duty
         *
         * @return
         *     - ESP_OK: succeed
         *     - others: fail
         */
        esp_err_t on();

        /**
         * @brief  set pwm channel to 0 duty
         *
         * @return
         *     - ESP_OK: succeed
         *     - others: fail
         */
        esp_err_t off();

        /**
         * @brief  initialize the pwm channel
         *
         * @param  io_num the gpio number of pwm channel
         * @param  ledc_channel ledc channel to generate pwm
         *
         * @return
         *     - ESP_OK: succeed
         *     - others: fail
         */
        esp_err_t init(gpio_num_t io_num, ledc_channel_t ledc_channel);

        /**
         * @brief  set duty of pwm channel
         *
         * @param  duty_val the target duty
         * @param  duty_mode refer to enum light_duty_mode_t
         *
         * @return
         *     - ESP_OK: succeed
         *     - others: fail
         */
        esp_err_t duty(uint32_t duty_val, light_duty_mode_t duty_mode = LIGHT_SET_DUTY_DIRECTLY);
        
        /**
         * @brief  get current duty of pwm channel
         *
         * @return currnet duty
         */
        uint32_t duty();

        /**
         * @brief  set pwm channel to breath mode
         *
         * @param  breath_period_ms breath period
         *
         * @return
         *     - ESP_OK: succeed
         *     - others: fail
         */
        esp_err_t breath(int breath_period_ms);
    };

    light_handle_t m_light_handle;

    // full duty of the light
    uint32_t m_full_duty;
    uint32_t m_channel_num;

    /**
     * prevent copy construct
     */
    CLight(const CLight&);
    CLight& operator=(const CLight&);

    // pwm channels
    CLightChannel *m_channels[LIGHT_MAX_CHANNEL_NUM] = { &red, &green, &blue, &cw, &ww };
public:
    /**
     * @brief  constructor of CLight
     *
     * @param  channel_num number of pwm channels
     * @param  freq_hz frequency of pwm
     * @param  timer ledc timer
     * @param  timer_bit
     * @param  speed_mode
     */
    CLight(light_channel_num_t channel_num, uint32_t freq_hz = 1000, ledc_timer_t timer = LEDC_TIMER_0,
            ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT, ledc_mode_t speed_mode = LEDC_HIGH_SPEED_MODE);

    // at most 5 pwm channels
    CLightChannel red;
    CLightChannel green;
    CLightChannel blue;
    CLightChannel cw;
    CLightChannel ww;

    /**
     * @brief  set light to blink
     *
     * @param  channel_mask decide which channels should blink, the others would be set to dark
     * @param  period_ms period of blink
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t blink_start(uint32_t channel_mask, uint32_t period_ms);

    /**
     * @brief  stop blink
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t blink_stop();

    /**
     * @brief  get full duty of light
     *
     * @return full duty of light
     */
    uint32_t get_full_duty();

    /**
     * @brief  set all channels to full duty
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t on();

    /**
     * @brief  set all channels to 0 duty
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t off();
    virtual ~CLight();
};
#endif

#endif
