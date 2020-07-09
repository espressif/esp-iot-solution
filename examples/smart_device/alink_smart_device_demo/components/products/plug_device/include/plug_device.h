/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_PLUG_DEVICE_H_
#define _IOT_PLUG_DEVICE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* plug_handle_t;

typedef enum {
    PLUG_OFF = 0,
    PLUG_ON,
} plug_status_t;

typedef enum {
    PLUG_STA_DISCONNECTED,    
    PLUG_CLOUD_DISCONNECTED,    
    PLUG_CLOUD_CONNECTED,
} plug_net_status_t;

typedef struct {
    gpio_num_t button_io;
    int button_active_level;
    gpio_num_t led_io;
    int led_off_level;
    gpio_num_t relay_ctl_io;
    gpio_num_t relay_clk_io;
    int relay_off_level;
    int relay_ctrl_mode;
    int relay_io_mode;
} plug_uint_def_t;

/**
  * @brief  plug device initilize
  *
  * @param  xSemWriteInfo semaphore for control post data
  *
  * @return the handle of the plug
  */
plug_handle_t plug_init(SemaphoreHandle_t xSemWriteInfo);

/**
  * @brief  get power meter parameters, include power, current, and voltage
  *
  * @param  plug_handle handle of the plug
  * @param  power the power value would be returned here
  * @param  current the current value would be returned here
  * @param  voltage the voltage value would be returned here
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t plug_powermeter_read(plug_handle_t plug_handle, uint32_t *power, uint32_t *current, uint32_t *voltage);

/**
  * @brief  set the state of plug
  *
  * @param  plug_handle handle of the plug
  * @param  state on or off
  * @param  unit_id plug unit id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t plug_state_write(plug_handle_t plug_handle, plug_status_t state, uint8_t unit_id);

/**
  * @brief  get the state of plug
  *
  * @param  plug_handle handle of the plug
  * @param  state_ptr the state would be returned here
  * @param  unit_id plug unit id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t plug_state_read(plug_handle_t plug_handle, plug_status_t* state_ptr, uint8_t unit_id);

/**
  * @brief  set the net status of plug
  *
  * @param  plug_handle handle of the plug
  * @param  net_sta the status of network or server connect
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t plug_net_status_write(plug_handle_t plug_handle, plug_net_status_t net_status);

#ifdef __cplusplus
}
#endif

#endif
