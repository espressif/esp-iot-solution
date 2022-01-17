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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "tinyusb.h"

/**
 * @brief Forward packets from Wi-Fi to USB.
 *
 * @param buffer - Data pointer
 * 
 * @param len    - Data length
 * 
 * @return esp_err_t
 */
esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb);

/**
 * @brief Initialize NET Device.
 */
void tusb_net_init(void);

void ecm_close(void);

void ecm_open(void);

#ifdef __cplusplus
}
#endif