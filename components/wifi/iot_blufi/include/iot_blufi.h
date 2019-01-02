// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _IOT_BLUFI_H_
#define _IOT_BLUFI_H_

#ifdef __cplusplus
extern "C" {
#endif

#define IOT_BLUFI_STATUS_INVAILD  -1

/**
 * @brief  get the status of blufi.
 *
 * @return
 *     - IOT_BLUFI_STATUS_INVAILD  blufi not started
 *     - other  blufi status
 */
esp_blufi_cb_event_t iot_blufi_get_status();

/**
 * @brief  start blufi.
 *
 * @param  release_classic_bt  whether or not release classic bt memory before ble start.
 *                             If classic bt memory is released, user can not use functions related to classic bt unless reboot device.
 * @param  ticks_to_wait  system tick number to wait
 *         - ticks_to_wait is zero, the API will return immediately, users should call iot_blufi_get_status to get the status of blufi.
 *         - ticks_to_wait is not zero, the API will block until wait ticks expired or start blufi failed.
 * @return
 *     - ESP_OK: succeed
 *     - ESP_ERR_TIMEOUT: start blufi timeout
 *     - ESP_FAIL: other errors
 */
esp_err_t iot_blufi_start(bool release_classic_bt, uint32_t ticks_to_wait);

/**
 * @brief stop blufi.
 *
 * @param  release_ble  whether or not release ble memory after ble stop.
 *                      If ble memory is not released, user can repeatly call iot_blufi_start and iot_blufi_stop to use iot_blufi component,
 *                      otherwise, user can not use ble again unless reboot device.
 * @return
 *     - ESP_OK: succeed
 *     - ESP_FAIL: other errors
 */
esp_err_t iot_blufi_stop(bool release_ble);

#ifdef __cplusplus
}
#endif

#endif
