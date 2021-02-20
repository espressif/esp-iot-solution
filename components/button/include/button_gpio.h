// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
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
#ifndef _IOT_BUTTON_GPIO_H_
#define _IOT_BUTTON_GPIO_H_

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief gpio button configuration
 * 
 */
typedef struct {
    int32_t gpio_num;
    uint8_t active_level;
} button_gpio_config_t;

/**
 * @brief Initialize gpio button
 * 
 * @param config pointer of configuration struct
 * 
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is NULL.
 */
esp_err_t button_gpio_init(const button_gpio_config_t *config);

/**
 * @brief Deinitialize gpio button
 * 
 * @param gpio_num gpio number of button
 * 
 * @return Always return ESP_OK
 */
esp_err_t button_gpio_deinit(int gpio_num);

/**
 * @brief Get current level on button gpio
 * 
 * @param gpio_num gpio number of button, it will be treated as a uint32_t variable.
 * 
 * @return Level on gpio
 */
uint8_t button_gpio_get_key_level(void *gpio_num);

#ifdef __cplusplus
}
#endif

#endif