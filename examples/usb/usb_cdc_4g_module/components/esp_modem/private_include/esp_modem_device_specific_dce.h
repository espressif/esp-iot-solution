// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_modem.h"

/**
 * @brief Specific init of SIM800 device
 *
 * @param dce Modem DCE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_sim800_specific_init(esp_modem_dce_t *dce);

/**
 * @brief Specific init of SIM7600 device
 *
 * @param dce Modem DCE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_sim7600_specific_init(esp_modem_dce_t *dce);

/**
 * @brief Specific init of BG96 device
 *
 * @param dce Modem DCE object
 *
 * @return ESP_OK on success
 */
esp_err_t esp_modem_bg96_specific_init(esp_modem_dce_t *dce);

#ifdef __cplusplus
}
#endif
