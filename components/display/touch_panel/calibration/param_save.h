/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PARAMETER_SAVE_H_
#define _PARAMETER_SAVE_H_

#include "esp_err.h"

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
esp_err_t param_save(const char* space_name, const char* key, void* param, uint16_t len);

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
esp_err_t param_load(const char* space_name, const char* key, void* dest);

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
esp_err_t param_erase(const char* space_name, const char* key);

#ifdef __cplusplus
}
#endif

#endif
