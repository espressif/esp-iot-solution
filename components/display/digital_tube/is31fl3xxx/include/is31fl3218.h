// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#ifndef _IS31FL3218_H_
#define _IS31FL3218_H_

#include "i2c_bus.h"


#ifdef __cplusplus
extern "C" {
#endif

#define IS31FL3218_CH_NUM_MAX 18
#define IS31FL3218_CH_NUM_MAX_MASK   (0x3ffff)
#define IS31FL3218_I2C_ID 0xA8                  /*!< I2C Addr*/
#define IS31FL3218_CH_BIT(i) ((uint32_t)0x1<<i) /*!< i == channel : 0 ~ 17*/
#define IS31FL3218_CH_MASK(c, o) do { \
    int i; \
    for (i=0; i<o; i++) { \
        c = c | IS31FL3218_CH_BIT(i); \
        printf("[%d %08x]\r\n", i, c); \
    } \
} while(0)

/**
 * @brief IS31FL3218 mode
 * 
 */
typedef enum {
    IS31FL3218_MODE_SHUTDOWN = 0, /**< Software shutdown mode */
    IS31FL3218_MODE_NORMAL,       /**< Normal operation */
    IS31FL3218_MODE_MAX,
} is31fl3218_mode_t;

/**
 * @brief Register of IS31FL3218
 * 
 */
typedef enum {
    IS31FL3218_REG_SHUTDOWN = 0x00,                         /*0 First state or BR/EDR scan 1*/
    IS31FL3218_REG_PWM_1 = 0x01,
    IS31FL3218_REG_PWM_2,
    IS31FL3218_REG_PWM_3,
    IS31FL3218_REG_PWM_4,
    IS31FL3218_REG_PWM_5,
    IS31FL3218_REG_PWM_6,
    IS31FL3218_REG_PWM_7,
    IS31FL3218_REG_PWM_8,
    IS31FL3218_REG_PWM_9,
    IS31FL3218_REG_PWM_10,
    IS31FL3218_REG_PWM_11,
    IS31FL3218_REG_PWM_12,
    IS31FL3218_REG_PWM_13,
    IS31FL3218_REG_PWM_14,
    IS31FL3218_REG_PWM_15,
    IS31FL3218_REG_PWM_16,
    IS31FL3218_REG_PWM_17,
    IS31FL3218_REG_PWM_18 = 0x12,
    IS31FL3218_REG_CONL_1,
    IS31FL3218_REG_CONL_2,
    IS31FL3218_REG_CONL_3,
    IS31FL3218_REG_UPDATE,
    IS31FL3218_REG_RESET,
    IS31FL3218_REG_MAX,
} is31fl3218_reg_t;


typedef void* is31fl3218_handle_t;

/**
 * @brief Create and init sensor object and return a led handle
 *
 * @param bus I2C bus object handle
 * @return
 *     - NULL Fail
 *     - Others Success
 */
is31fl3218_handle_t is31fl3218_create(i2c_bus_handle_t bus);

/**
 * @brief Delete and release a LED object
 * @param sensor object handle of is31fl3218
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t is31fl3218_delete(is31fl3218_handle_t fxled);

/**
 * @brief The Shutdown Register sets software shutdown mode of IS31FL3218.
 * @param fxled  led dev handle
 * @param mode  shutdown mode or Normal
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t is31fl3218_set_mode(is31fl3218_handle_t fxled, is31fl3218_mode_t mode);

/**
 * @brief IS31FL3218 will reset all registers to default value.
 * @param fxled  led dev handle
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t is31fl3218_reset_register(is31fl3218_handle_t fxled);

/**
 * @brief set PWM duty for every channel
 * @param fxled  led dev handle
 * @param ch_bit set the pwm channel bit if you want change duty, BIT0 ~ BIT17 means PWM1 ~ PWM18
 * @param duty duty for each PWM channel, range 0~255
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t is31fl3218_channel_set(is31fl3218_handle_t fxled, uint32_t ch_bit, uint8_t duty);

/**
 * @brief Update PWM duty value with values in a buffer
 * @param fxled  led dev handle
 * @param duty duty array for each PWM channel, range 0~255
 * @param len duty array length
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t is31fl3218_write_pwm_regs(is31fl3218_handle_t fxled, uint8_t* duty, int len);

#ifdef __cplusplus
}
#endif

#endif

