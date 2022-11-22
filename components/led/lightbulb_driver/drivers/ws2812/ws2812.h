// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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

#include "driver/gpio.h"
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Output configuration
 *
 */
typedef struct {
    uint8_t led_num;
    gpio_num_t ctrl_io;
} driver_ws2812_t;

/**
 * @brief Initialize ws2812 output
 *
 * @param config Driver configuration
 * @return esp_err_t
 */
esp_err_t ws2812_init(driver_ws2812_t *config);

/**
 * @brief Deinitialize ws2812 and release resources
 *
 * @return esp_err_t
 */
esp_err_t ws2812_deinit(void);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t ws2812_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b);

#ifdef __cplusplus
}
#endif
