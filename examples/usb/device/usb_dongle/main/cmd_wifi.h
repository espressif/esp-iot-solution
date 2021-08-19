/**
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 * 
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#ifndef __CMD_WIFI_H
#define __CMD_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

void initialise_wifi(void);

esp_err_t wifi_cmd_sta_join(const char* ssid, const char* pass);

esp_err_t wifi_cmd_ap_set(const char* ssid, const char* pass);

esp_err_t wifi_cmd_sta_scan(const char* ssid);

esp_err_t wifi_cmd_query(void);

esp_err_t wifi_cmd_set_mode(char* mode);

esp_err_t wifi_cmd_start_smart_config(void);

esp_err_t wifi_cmd_stop_smart_config(void);

esp_err_t wif_cmd_disconnect_wifi(void);

#ifdef __cplusplus
}
#endif

#endif // __CMD_WIFI_H