/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
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

#ifndef _IOT_PARSE_H_
#define _IOT_PARSE_H_

#include "sdkconfig.h"
#include "device.h"

#ifdef SMART_LIGHT_DEVICE
/**
  * @brief  parse json data received from cloud
  *
  * @param  down_cmd json data received from cloud
  * @param  light_info light status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_parse(char *down_cmd, smart_light_dev_t *light_info);

/**
  * @brief  pack device status to json data
  *
  * @param  up_cmd json data postt to cloud
  * @param  light_info light status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_pack(char *up_cmd, smart_light_dev_t *light_info);
#endif

#ifdef SMART_PLUG_DEVICE
/**
  * @brief  parse json data received from cloud
  *
  * @param  down_cmd json data received from cloud
  * @param  plug_info plug status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_parse(char *down_cmd, smart_plug_dev_t *plug_info);

/**
  * @brief  pack device status to json data
  *
  * @param  up_cmd json data postt to cloud
  * @param  plug_info plug status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_pack(char *up_cmd, smart_plug_dev_t *plug_info);
#endif

#endif
