// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
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

#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Buzzer initialization
 *
 * This function initializes buzzer by using LEDC peripheral
 *
 * @param buzzer_pin
 */
void buzzer_driver_install(gpio_num_t buzzer_pin);

/**
 * @brief   Buzzer outputs voice configuration
 *
 * @param en    Enable / Disable buzzer output
 *
 * TODO: add voice frequency parameter
 *
 */
void buzzer_set_voice(bool en);

#ifdef __cplusplus
}
#endif
