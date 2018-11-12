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
#ifndef _IOT_OTA_H_
#define _IOT_OTA_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_system.h"


/**
  * @brief  start ota, you have call esp_restart to run the new app
  *
  * @param  server_ip
  * @param  server_port
  * @param  file_dir the directory of target bin
  * @param  ticks_to_wait ota would stop after appointed ticks
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_ota_start(const char *server_ip, uint16_t server_port, const char *file_dir, uint32_t ticks_to_wait);

/**
 * @brief start OTA via the given URL to the file
 * @param url the URL string point to the file address
 * @param ticks_to_wait set timeout
 *     - ESP_OK: succeed
 *     - others: fail
 */
esp_err_t iot_ota_start_url(const char *url, uint32_t ticks_to_wait);


/**
 * @brief get OTA progress status
 * return
 *     - -1 OTA not started
 *     - others(>0) OTA progress (0 - 100 %)
 */
int iot_ota_get_ratio();

#ifdef __cplusplus
}
#endif
#endif
