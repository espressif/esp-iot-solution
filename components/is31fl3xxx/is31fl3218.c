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
  
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "is31fl3218.h"

static const char *tag = "IS31";

#define IS31_ERROR_CHECK(con) if(!(con)) {ESP_LOGE(tag,"err"); return ESP_FAIL;}
#define IS31_PARAM_CHECK(con) if(!(con)) {ESP_LOGE(tag,"Parameter error, "); return ESP_FAIL;}

#define IS31FL3218_WRITE_BIT    0x00
#define IS31FL3218_READ_BIT     0x01
#define IS31FL3218_ACK_CHECK_EN 1

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

/**
 * @brief set software shutdown mode
 */
esp_err_t is31fl3218_write_reg(i2c_port_t i2c_port, is31fl3218_reg_t reg_addr, uint8_t *data, uint8_t data_num)
{
    int i;
    IS31_PARAM_CHECK(NULL != data);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, IS31FL3218_I2C_ID | IS31FL3218_WRITE_BIT, IS31FL3218_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, IS31FL3218_ACK_CHECK_EN);
    for(i=0; i<data_num; i++) {
        i2c_master_write_byte(cmd, *(data + i), IS31FL3218_ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_port, cmd, 5000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_FAIL) {
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief set software shutdown mode
 */
esp_err_t is31fl3218_set_mode(i2c_port_t i2c_port, is31fl3218_mode_t mode)
{
    //change reg 00h
    return is31fl3218_write_reg(i2c_port, IS31FL3218_REG_SHUTDOWN, (uint8_t *)&mode, 1);
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t is31fl3218_set_channel_duty(i2c_port_t i2c_port, uint32_t ch_bit, uint8_t duty)
{
    //18 channel
    int ret, i;
    for (i=0; i<18; i++) {
        if((ch_bit>>i) & 0x1) {
            ret = is31fl3218_write_reg(i2c_port, IS31FL3218_REG_PWM_1 + i, &duty, 1);
        }
    }
    return ret;
}

esp_err_t is31fl3218_channel_enable(i2c_port_t i2c_port, uint32_t ch_bit)
{
    //18 channel
    int ret, i;
    uint8_t conl[3];
    uint32_t i_ch_bit = ch_bit;
    for (i=0; i<3; i++) {
        conl[i] = (uint8_t)(i_ch_bit&0x3F);
        i_ch_bit = i_ch_bit >> i*6;
    }
    ret = is31fl3218_write_reg(i2c_port, IS31FL3218_REG_CONL_1, (uint8_t *)(conl), 3);
    return ret;
}

/**
 * @brief Load PWM Register and LED Control Register¡¯s data
 */
esp_err_t is31fl3218_update_register(i2c_port_t i2c_port)
{
    //enable reg
    uint8_t m = 1;
    int ret = is31fl3218_write_reg(i2c_port, IS31FL3218_REG_UPDATE, &m, 1);
    return ret;
}

/**
 * @brief reset all register into default
 */
esp_err_t is31fl3218_reset_register(i2c_port_t i2c_port)
{
    //reset reg
    uint8_t m = 1;
    int ret = is31fl3218_write_reg(i2c_port, IS31FL3218_REG_UPDATE, &m, 1);
    return ret;
}

esp_err_t is31fl3218_init(is31fl3218_info_t *info)
{
    IS31_PARAM_CHECK( NULL != info );
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_reset_register(info->i2c_port) );
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_set_mode(info->i2c_port, IS31FL3218_MODE_NORMAL) );
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_channel_enable(info->i2c_port, info->ch_bit) );
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_update_register(info->i2c_port) );
    return ESP_OK;
}

esp_err_t is31fl3218_channel_set(i2c_port_t i2c_port, uint32_t ch_bit, uint8_t duty)
{
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_set_channel_duty(i2c_port, ch_bit, duty) );
    IS31_ERROR_CHECK( ESP_OK == is31fl3218_update_register(i2c_port) );
    return ESP_OK;
}