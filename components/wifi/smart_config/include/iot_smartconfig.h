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
#ifndef _IOT_SMARTCONFIG_H_
#define _IOT_SMARTCONFIG_H_

#include "esp_smartconfig.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get smartconfig status
 * @return Smartconfig status
 */
// smartconfig_status_t iot_sc_get_status();

/**
  * @brief  SmartConfig initialize.
  * @param sc_type smartconfig protocol type
  * @param wifi_mode 
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_sc_setup(smartconfig_type_t sc_type, wifi_mode_t wifi_mode, bool fast_mode_en);

/**
  * @brief  Start SmartConfig .
  * @param  ticks_to_wait system tick number to wait
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  *     - ESP_TIMEOUT: timeout
  */
esp_err_t iot_sc_start(uint32_t ticks_to_wait);

/**
 * @brief Smartconfig stop
 */
void sc_stop();

#ifdef __cplusplus
}
#endif

#endif
