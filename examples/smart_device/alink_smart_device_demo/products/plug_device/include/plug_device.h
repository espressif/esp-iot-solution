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

#ifndef _IOT_PLUG_DEVICE_H_
#define _IOT_PLUG_DEVICE_H_

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
  *
  * @return the handle of the plug
  */
plug_handle_t plug_init();

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
  *     - ESP_FAIL: failed
  */
esp_err_t plug_powermeter_read(plug_handle_t plug_handle, uint32_t *power, uint32_t *current, uint32_t *voltage);

/**
  * @brief  set the state of plug
  *
  * @param  plug_handle handle of the plug
  * @param  state
  * @param  unit_id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t plug_state_write(plug_handle_t plug_handle, plug_status_t state, uint8_t unit_id);

/**
  * @brief  get the state of plug
  *
  * @param  plug_handle handle of the plug
  * @param  state_ptr the state would be returned here
  * @param  unit_id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t plug_state_read(plug_handle_t plug_handle, plug_status_t* state_ptr, uint8_t unit_id);

/**
  * @brief  set the net status of plug
  *
  * @param  plug_handle handle of the plug
  * @param  net_sta the status of net or server connect
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t plug_net_status_write(plug_handle_t plug_handle, plug_net_status_t net_status);
#endif
