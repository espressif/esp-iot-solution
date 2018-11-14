/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_PARSE_H_
#define _IOT_PARSE_H_

#include "sdkconfig.h"
#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SMART_LIGHT_DEVICE
/**
  * @brief  parse json data received from cloud
  *
  * @param  down_cmd json data received from cloud
  * @param  light_info light status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
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
  *     - ESP_FAIL: fail
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
  *     - ESP_FAIL: fail
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
  *     - ESP_FAIL: fail
  */
esp_err_t cloud_device_pack(char *up_cmd, smart_plug_dev_t *plug_info);
#endif

#ifdef __cplusplus
}
#endif

#endif
