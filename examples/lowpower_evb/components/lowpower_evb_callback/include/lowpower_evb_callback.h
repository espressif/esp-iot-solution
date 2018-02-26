/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_CALLBACK_H_
#define _IOT_LOWPOWER_EVB_CALLBACK_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ULP_WAKE_UP
#define ULP_WAKE_UP
#endif

#ifdef CONFIG_GPIO_WAKE_UP
#define GPIO_WAKE_UP
#define NOT_ULP_WAKE_UP
#endif

#ifdef CONFIG_TIMER_WAKE_UP
#define TIMER_WAKE_UP
#define NOT_ULP_WAKE_UP
#endif

/**
  * @brief lowpower evb init, called when power on.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t lowpower_evb_init_cb(void);

/**
  * @brief config wifi and connect to AP, called after lowpower evb intialize.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t wifi_connect_cb(void);

/**
  * @brief get data from ULP, called when ULP wakeup.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t get_data_by_cpu_cb(void);

/**
  * @brief initialize ULP program, we need to load program into ULP, called before start deepsleep.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t ulp_program_init_cb(void);

/**
  * @brief get data from ULP, called when ULP wakeup.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t get_data_from_ulp_cb(void);

/**
  * @brief connect to AP and send data to server, called after acquire sensor data.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t send_data_to_server_cb(void);

/**
  * @brief called when send data success.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t send_data_done_cb(void);

/**
  * @brief define wakeup cause and start deepsleep.
  *
  * @return 
  *     - ESP_OK success
  *     - ESP_FAIL fail
  */
esp_err_t start_deep_sleep_cb(void);

#ifdef __cplusplus
}
#endif

#endif

