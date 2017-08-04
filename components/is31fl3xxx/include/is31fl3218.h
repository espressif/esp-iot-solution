/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#ifndef _IOT_IS31FL3218_H_
#define _IOT_IS31FL3218_H_

#include "driver/i2c.h"

#define DBG_TEST(s) printf("[%s %d] "s"\r\n",__func__, __LINE__);

#define IS31FL3218_CH_NUM_MAX 18
#define IS31FL3218_I2C_ID 0xA8                  /*!< I2C Addr*/
#define IS31FL3218_CH_BIT(i) ((uint32_t)0x1<<i) /*!< i == channel : 0 ~ 17*/
#define IS31FL3218_CH_MUM(c, o) do { \
    int i; \
    for (i=0; i<o; i++) { \
        c = c | IS31FL3218_CH_BIT(i); \
        printf("[%d %08x]\r\n", i, c); \
    } \
} while(0)

typedef enum {
    IS31FL3218_MODE_SHUTDOWN = 0, /**< Software shutdown mode */
    IS31FL3218_MODE_NORMAL,       /**< Normal operation */
    IS31FL3218_MODE_MAX,
} is31fl3218_mode_t;

typedef struct {
    i2c_port_t i2c_port; /**< set the i2c port num for is31fl3218*/
    uint32_t ch_bit;     /**< BIT0 ~ BIT17 means PWM1 ~ PWM18 */
} is31fl3218_info_t;

/**
 * @brief The Shutdown Register sets software shutdown mode of IS31FL3218.
 *
 * @param i2c_port  i2s num of used to communicate
 * @param mode  shutdown mode or Normal
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3218_set_mode(i2c_port_t i2c_port, is31fl3218_mode_t mode);

/**
 * @brief IS31FL3218 will reset all registers to default value.
 *
 * @param i2c_port i2s num of used to communicate 
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3218_reset_register(i2c_port_t i2c_port);

/**
 * @brief set I2S port and enable PWM channel
 *
 * @param i2c_port i2s num of used to communicate
 * @param info  init param
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3218_init(is31fl3218_info_t *info);

/**
 * @brief set PWM duty for every channel
 *
 * @param i2c_port i2s num of used to communicate
 * @param ch_bit set the pwm channel bit if you want change duty, BIT0 ~ BIT17 means PWM1 ~ PWM18
 * @param duty duty for each PWM channel, range 0~255
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 */
esp_err_t is31fl3218_channel_set(i2c_port_t i2c_port, uint32_t ch_bit, uint8_t duty);
#endif

