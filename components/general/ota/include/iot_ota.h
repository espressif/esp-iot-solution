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
#endif
