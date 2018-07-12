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
  * @brief  save param to flash with protect.
  *
  * @param  space_name Namespace name. Maximal length is determined by the
  *                    underlying implementation, but is guaranteed to be
  *                    at least 15 characters. Shouldn't be empty.
  * @param  key  Key name of param. Different params should have different keys.
  *              Maximal length is 15 characters. Shouldn't be empty.
  * @param  param pointer of param.
  * @param  len length of param, Maximum length is 1984 bytes
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: refer to nvs.h
  */
esp_err_t iot_param_save(const char* space_name, const char* key, void* param, uint16_t len);

/**
  * @brief  read param from flash.
  *
  * @param  space_name Namespace name. Maximal length is determined by the
  *                    underlying implementation, but is guaranteed to be
  *                    at least 15 characters. Shouldn't be empty.
  * @param  key  Key name of param. Different params should have different keys.
  *              Maximal length is 15 characters. Shouldn't be empty.
  * @param  dest the address to save read param.
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: refer to nvs.h
  */
esp_err_t iot_param_load(const char* space_name, const char* key, void* dest);

/**
  * @brief  erase param saved in flash.
  *
  * @param  space_name Namespace name. Maximal length is determined by the
  *                    underlying implementation, but is guaranteed to be
  *                    at least 15 characters. Shouldn't be empty.
  * @param  key  Key name of param. Different params should have different keys.
  *              Maximal length is 15 characters. Shouldn't be empty.
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: refer to nvs.h
  */
esp_err_t iot_param_erase(const char* space_name, const char* key);

#ifdef __cplusplus
}
#endif

#endif
