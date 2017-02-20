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

#ifndef _IOT_TOUCHPAD_H_
#define _IOT_TOUCHPAD_H_
#include "driver/touch_pad.h"
#include "esp_log.h"

typedef void* touchpad_handle_t;
typedef enum {
    TOUCHPAD_SINGLE_TRIGGER,        /**< touchpad event will only trigger once no matter how long you press the touchpad */
    TOUCHPAD_SERIAL_TRIGGER,        /**< number of touchpad evetn ttiggerred will be in direct proportion to the duration of press*/
} touchpad_trigger_t;

/**
  * @brief  create touchpad device
  *
  * @param  touch_pad_num refer to struct touch_pad_t
  * @param  threshold the sample value of a touchpad under which the interrupt will be triggered
  * @param  filter_value filter time of touchpad
  * @param  trigger refer to enum touchpad_trigger_t
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
touchpad_handle_t touchpad_create(touch_pad_t touch_pad_num, uint32_t threshold, uint32_t filter_value, touchpad_trigger_t trigger);

/**
  * @brief  delete touchpad device
  *
  * @param  touchpad_handle 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_delete(touchpad_handle_t touchpad_handle);

/**
  * @brief  get the number of a touchpad
  *
  * @param  touchpad_handle 
  *
  * @return  touchpad number
  */
touch_pad_t touchpad_num_get(const touchpad_handle_t touchpad_handle);

/**
  * @brief  set the threshold of touchpad
  *
  * @param  touchpad_handle 
  * @param  threshold value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_set_threshold(const touchpad_handle_t touchpad_handle, uint32_t threshold);

/**
  * @brief  set the filter value of touchpad
  *
  * @param  touchpad_handle 
  * @param  filter_value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_set_filter(const touchpad_handle_t touchpad_handle, uint32_t filter_value);

/**
  * @brief  set the trigger mode of touchpad
  *
  * @param  touchpad_handle 
  * @param  trigger
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_set_trigger(const touchpad_handle_t touchpad_handle, touchpad_trigger_t trigger);

/**
  * @brief  get sample value of the touchpad
  *
  * @param  touchpad_handle 
  * @param  touch_value_ptr pointer to the value read 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_read(const touchpad_handle_t touchpad_handle, uint16_t *touch_value_ptr);
#endif
