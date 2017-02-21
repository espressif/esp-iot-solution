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

typedef enum {
    LED_DARK_LOW = 0,       /**< choose this parameter if the led is dark when the level of gpio is low */
    LED_DARK_HIGH = 1,      /**< choose this parameter if the led is dark when the level of gpio is high */
} led_dark_level_t;

typedef enum {
    LED_NORMAL_OFF,
    LED_NORMAL_ON,
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
  *
  * @param  quick_blink_fre 
  * @param  slow_blink_fre
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t led_setup(uint64_t quick_blink_fre, uint64_t slow_blink_fre);

/**
  * @brief  create new led.
  *
  * @param  io_num
  * @param  dark_level on whick level the led is dark
  *
  * @return the handle of the led
  */
led_handle_t led_create(uint8_t io_num, led_dark_level_t dark_level);

/**
  * @brief  free the memory of led
  *
  * @param  led_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t led_delete(led_handle_t led_handle);

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
esp_err_t led_state_write(led_handle_t led_handle, led_status_t state);

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
esp_err_t led_mode_write(led_handle_t led_handle, led_mode_t mode);

/**
  * @brief  get state of led
  *
  * @param  led_handle
  *
  * @return state of led
  */
led_status_t led_state_read(led_handle_t led_handle);

/**
  * @brief  get mode of led
  *
  * @param  led_handle
  *
  * @return mode of led
  */
led_mode_t led_mode_read(led_handle_t led_handle);

#endif
