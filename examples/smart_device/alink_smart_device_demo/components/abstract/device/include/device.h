/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_DEVICE_H_
#define _IOT_DEVICE_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_SMART_PLUG_DEVICE
#define SMART_PLUG_DEVICE
#endif
#ifdef CONFIG_SMART_LIGHT_DEVICE
#define SMART_LIGHT_DEVICE
#endif

#ifdef SMART_LIGHT_DEVICE
typedef struct  smart_light_dev {
    uint32_t errorcode;
    uint32_t on_off;
    uint32_t work_mode;
    uint32_t luminance;
    uint32_t color_temp;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} smart_light_dev_t;
#endif

#ifdef SMART_PLUG_DEVICE
typedef struct smart_plug_dev {
    uint32_t errorcode;
    uint32_t on_off;
    uint32_t power;
    uint32_t rms_current;
    uint32_t rms_voltage;
    uint32_t switch_multiple_1;
    uint32_t switch_multiple_2;
    uint32_t switch_multiple_3;
} smart_plug_dev_t;
#endif

typedef enum {
    STA_DISCONNECTED,
    CLOUD_CONNECTED,
    CLOUD_DISCONNECTED, 
} cloud_dev_net_status_t;

/**
  * @brief  cloud device initialize
  *
  * @param  xSemWriteInfo semaphore for control post data
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t cloud_device_init(SemaphoreHandle_t xSemWriteInfo);

/**
  * @brief  set device status
  *
  * @param  down_cmd json data received from cloud
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t cloud_device_write(char *down_cmd);

/**
  * @brief  get device status
  *
  * @param  up_cmd json data post to cloud
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t cloud_device_read(char *up_cmd);

/**
  * @brief  set network status
  *
  * @param  net_status network status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t cloud_device_net_status_write(cloud_dev_net_status_t net_status);

#ifdef __cplusplus
}
#endif

#endif

