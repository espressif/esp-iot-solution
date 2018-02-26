/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LOWPOWER_EVB_WIFI_H_
#define _IOT_LOWPOWER_EVB_WIFI_H_

#include "esp_err.h"
#include "iot_smartconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief initialize button, this button is used to clear wifi config and restart system.
 */
void button_init();

/**
 * @brief start wifi config.
 */
void lowpower_evb_wifi_config();

/**
 * @brief get wifi config.
 *
 * @param config pointer of wifi config.
 *
 * @return
 *     - ESP_OK success
 *     - ESP_FAIL fail
 */
esp_err_t lowpower_evb_get_wifi_config(wifi_config_t *config);

#ifdef __cplusplus
}
#endif

#endif

