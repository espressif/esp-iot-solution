// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _IOT_BOARDS_COMMON_H_
#define _IOT_BOARDS_COMMON_H_

#include "i2c_bus.h"
#include "spi_bus.h"

#define ATTR_WEAK __attribute__((weak))
#define BOARD_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }

#ifndef _ENABLE
#define _ENABLE 1
#endif

#ifndef _DISABLE
#define _DISABLE 0
#endif

#ifndef _UNDEFINE
#define _UNDEFINE
#endif

#ifndef _POSITIVE
#define _POSITIVE 1
#endif

#ifndef _NEGATIVE
#define _NEGATIVE 0
#endif

typedef void* board_res_handle_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Board level init.
 *        Peripherals can be chosen through menuconfig, which will be initialized with default configurations during iot_board_init.
 *        After board init, initialized peripherals can be referenced by handles directly.
 * 
 * @return esp_err_t 
 */
esp_err_t iot_board_init(void);

/**
 * @brief Board level deinit.
 *        After board deinit, initialized peripherals will be deinit and related handles will be set to NULL.
 * 
 * @return esp_err_t 
 */
esp_err_t iot_board_deinit(void);

/**
 * @brief Check if board is initialized 
 * 
 * @return true if board is initialized
 * @return false if board is not initialized
 */
bool iot_board_is_init(void);

/**
 * @brief init esp32xx Wi-Fi with configs from menuconfig, deinit currentlly not supported
 * 
 * @return esp_err_t 
 */
esp_err_t iot_board_wifi_init(void);

/**
 * @brief Using resource's ID declared in board_res_id_t to get board level resource's handle
 * 
 * @param id Resource's ID declared in board_res_id_t
 * @return board_res_handle_t Resource's handle
 * if no related handle,NULL will be returned
 */
board_res_handle_t iot_board_get_handle(int id);

/**
 * @brief Get board information
 * 
 * @return String include BOARD_NAME etc. 
 */
char* iot_board_get_info();

#ifdef __cplusplus
}
#endif

#endif