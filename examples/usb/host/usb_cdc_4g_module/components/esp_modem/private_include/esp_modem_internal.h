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

#include "esp_log.h"
#include "esp_modem.h"

/**
* @brief Macro defined for error checking
*
*/
#define ESP_MODEM_ERR_CHECK(a, str, goto_tag, ...)                                    \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

/**
* @brief common modem delay function
*
*/
static inline void esp_modem_wait_ms(size_t time)
{
    vTaskDelay(pdMS_TO_TICKS(time));
}

#ifdef __cplusplus
}
#endif
