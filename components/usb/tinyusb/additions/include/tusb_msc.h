// Copyright 2020-2021 Espressif Systems (Shanghai) Co. Ltd.
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

#include <stdint.h>
#include "tusb.h"
#include "tinyusb.h"

/**
 * @brief Configuration structure for MSC
 */
typedef struct {
    uint8_t pdrv;             /* Physical drive nmuber (0..) */
} tinyusb_config_msc_t;

/**
 * @brief Initialize MSC Device.
 *
 * @param cfg - init configuration structure
 * @return esp_err_t
 */
esp_err_t tusb_msc_init(const tinyusb_config_msc_t *cfg);

#ifdef __cplusplus
}
#endif
