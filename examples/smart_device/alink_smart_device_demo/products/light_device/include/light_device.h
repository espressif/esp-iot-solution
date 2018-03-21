/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LIGHT_DEVICE_H_
#define _IOT_LIGHT_DEVICE_H_
#include "iot_light.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* light_dev_handle_t;

typedef enum {
    LIGHT_STA_DISCONNECTED,
    LIGHT_CLOUD_DISCONNECTED,    
    LIGHT_CLOUD_CONNECTED,
} light_net_status_t;

/**
  * @brief  light device initilize
  *
  * @param  None
  *
  * @return the handle of the light
  */
light_dev_handle_t light_init(void);

/**
  * @brief  set light parameters
  *
  * @param  light_dev handle of the light
  * @param  red red channel value
  * @param  green green channel value
  * @param  blue blue channel value
  * @param  color_temp color temperature value
  * @param  luminance lunminance value
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t light_set(light_dev_handle_t light_dev, uint32_t red, uint32_t green, uint32_t blue, uint32_t color_temp, uint32_t luminance);

/**
  * @brief  get light parameters
  *
  * @param  light_dev handle of the light
  * @param  red pointer of red channel value
  * @param  green pointer of green channel value
  * @param  blue pointer of blue channel value
  * @param  color_temp pointer of color temperature value
  * @param  luminance pointer of lunminance value
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t light_get(light_dev_handle_t light_dev, uint32_t *red, uint32_t *green, uint32_t *blue, uint32_t *color_temp, uint32_t *luminance);

/**
  * @brief  set the net status of light
  *
  * @param  plug_handle handle of the light
  * @param  net_sta the status of network or server connect
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t light_net_status_write(light_dev_handle_t light_dev, light_net_status_t net_status);

#ifdef __cplusplus
}
#endif

#endif

