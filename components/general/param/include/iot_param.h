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
#ifndef _IOT_PARAM_H_
#define _IOT_PARAM_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  save param to flash with protect
  *
  * @param  namespace
  * @param  key different param should have different key
  * @param  param
  * @param  len
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_param_save(const char* namespace, const char* key, void* param, uint16_t len);

/**
  * @brief  read param from flash
  *
  * @param  namespace
  * @param  key different param should have different key
  * @param  dest the address to save read param
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_param_load(const char* namespace, const char* key, void* dest);

#ifdef __cplusplus
}
#endif

#endif
