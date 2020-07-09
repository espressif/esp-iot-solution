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
#ifndef _IOT_LED_H_
#define _IOT_LED_H_
#include "sdkconfig.h"
#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    LED_DARK_LOW = 0,       /**< choose this parameter if the led is dark when the level of gpio is low */
    LED_DARK_HIGH = 1,      /**< choose this parameter if the led is dark when the level of gpio is high */
} led_dark_level_t;

typedef enum {
    LED_OFF,
    LED_ON,
    LED_QUICK_BLINK,
    LED_SLOW_BLINK,
} led_status_t;

typedef enum {
    LED_NORMAL_MODE,        /**< led normal */
    LED_NIGHT_MODE,         /**< led night mode, always turn off led */ 
} led_mode_t;

typedef void* led_handle_t;


/**
  * @brief  led initialize.
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_led_setup();

/**
 * @brief led blink frequency update
 * @param quick_blink_freq quick blink frequency
 * @param slow_blink_freq slow blink frequency
 * @return
 *     - ESP_OK: success
 *     - others: fail
 */
esp_err_t iot_led_update_blink_freq(int quick_blink_freq, int slow_blink_freq);

/**
  * @brief  create new led.
  *
  * @param  io_num
  * @param  dark_level on whick level the led is dark
  *
  * @return the handle of the led
  */
led_handle_t iot_led_create(uint8_t io_num, led_dark_level_t dark_level);

/**
  * @brief  free the memory of led
  *
  * @param  led_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_led_delete(led_handle_t led_handle);

/**
  * @brief  set state of led
  *
  * @param  led_handle
  * @param  state refer to enum led_status_t
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_led_state_write(led_handle_t led_handle, led_status_t state);

/**
  * @brief  set mode of led
  *
  * @param  led_handle
  * @param  mode refer to enum led_mode_t
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_led_mode_write(led_handle_t led_handle, led_mode_t mode);

/**
  * @brief  set duty of night mode, all the leds share the same duty
  *
  * @param  duty value from 0~100
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_led_night_duty_write(uint8_t duty);

/**
  * @brief  get state of led
  *
  * @param  led_handle
  *
  * @return state of led
  */
led_status_t iot_led_state_read(led_handle_t led_handle);

/**
  * @brief  get mode of led
  *
  * @param  led_handle
  *
  * @return mode of led
  */
led_mode_t iot_led_mode_read(led_handle_t led_handle);

/**
  * @brief  get duty of night mode
  *
  * @return duty of night mode
  */
uint8_t iot_led_night_duty_read();

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
 * class of status led
 * simple usage:
 * CLED* my_led = new CLED(17, LED_DARK_LOW);
 * my_led->off();
 * my_led->slow_blink();
 * CLED::blink_freq_write(10, 2);
 * delete my_led;
 */
class CLED: public CControllable
{
private:
    led_handle_t m_led_handle;

    // the quick blink frequency of all status leds
    static uint64_t m_quick_blink_freq;

    // the slow blink frequency of all status leds
    static uint64_t m_slow_blink_freq;

    /**
     * prevent copy construct
     */
    CLED(const CLED&);
    CLED& operator=(const CLED&);
public:
    /**
     * @brief  constructor of CLED
     *
     * @param  io_num
     * @param  dark_level on whick level the led is dark
     */
    CLED(uint8_t io_num, led_dark_level_t dark_level);

    /**
     * @brief  set state of led
     *
     * @param  state refer to enum led_status_t
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t state_write(led_status_t state);

    /**
     * @brief  get state of led
     *
     * @return state of led
     */
    led_status_t state_read();

    /**
     * @brief toggle the led output
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t toggle();

    /**
     * @brief  set mode of led
     *
     * @param  mode refer to enum led_mode_t
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t mode_write(led_mode_t mode);

    /**
     * @brief  get mode of led
     *
     * @return mode of led
     */
    led_mode_t mode_read();

    /**
     * @brief  set led to quick blink
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t quick_blink();

    /**
     * @brief  set led to slow blink
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t slow_blink();

    /**
     * @brief  set led to night mode
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t night_mode();

    /**
     * @brief  set led to normal mode
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    esp_err_t normal_mode();

    /**
     * @brief set blink frequency of all status leds
     *
     * @param quick_freq quick blink frequency
     * @param slow_freq slow blink frequency
     *
     * @return
     *     - ESP_OK: success
     *     - others: fail
     */
    static esp_err_t blink_freq_write(uint64_t quick_freq, uint64_t slow_freq);

    /**
     * @brief set duty in night mode of all status leds
     *
     * @param duty 0~100
     *
     * @return
     *     - ESP_OK: success
     *     - others: fail
     */
    static esp_err_t night_duty_write(uint8_t duty);

    /**
     * @brief get duty in night mode of all status leds
     *
     * @return duty of night mode
     */
    static uint8_t night_duty_read();

    /**
     * @brief  set led to bright
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t on();

    /**
     * @brief  set led to dark
     *
     * @return
     *     - ESP_OK: succeed
     *     - others: fail
     */
    virtual esp_err_t off();

    virtual ~CLED();
};
#endif
#endif
