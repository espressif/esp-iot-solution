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
#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "is31fl3218.h"
#include "i2c_bus.h"

static const char *TAG = "IS31";

#define IS31_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }
#define IS31_ERROR_CHECK(con) if(!(con)) {ESP_LOGE(TAG,"err line: %d", __LINE__); return ESP_FAIL;}
#define IS31_PARAM_CHECK(con) if(!(con)) {ESP_LOGE(TAG,"Parameter error, "); return ESP_FAIL;}

#define IS31FL3218_WRITE_BIT    0x00
#define IS31FL3218_READ_BIT     0x01
#define IS31FL3218_ACK_CHECK_EN 1

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
} is31fl3218_dev_t;

static esp_err_t is31fl3218_write(is31fl3218_handle_t fxled, is31fl3218_reg_t reg_addr, uint8_t *data, uint8_t data_num)
{
    is31fl3218_dev_t *led = (is31fl3218_dev_t *) fxled;
    IS31_PARAM_CHECK(NULL != data);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, IS31FL3218_I2C_ID | IS31FL3218_WRITE_BIT, IS31FL3218_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, IS31FL3218_ACK_CHECK_EN);
    i2c_master_write(cmd, data, data_num, IS31FL3218_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_bus_cmd_begin(led->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "%s:%d (%s):i2c transmit failed [%s]", __FILE__, __LINE__, __FUNCTION__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief set software shutdown mode
 */
esp_err_t is31fl3218_set_mode(is31fl3218_handle_t fxled, is31fl3218_mode_t mode)
{
    int ret = is31fl3218_write(fxled, IS31FL3218_REG_SHUTDOWN, (uint8_t *) &mode, 1);
    return ret;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t is31fl3218_set_channel_duty(is31fl3218_handle_t fxled, uint32_t ch_bit, uint8_t duty)
{
    int ret, i;
    for (i = 0; i < IS31FL3218_CH_NUM_MAX; i++) {
        if ((ch_bit >> i) & 0x1) {
            ret = is31fl3218_write(fxled, IS31FL3218_REG_PWM_1 + i, &duty, 1);
            if (ret != ESP_OK) {
                return ret;
            }
        }
    }
    return ESP_OK;
}

esp_err_t is31fl3218_channel_enable(is31fl3218_handle_t fxled, uint32_t ch_bit)
{
    uint8_t conl[3];
    for (int i = 0; i < 3; i++) {
        conl[i] = (ch_bit >> (i * 6)) & 0x3f;
    }
    return is31fl3218_write(fxled, IS31FL3218_REG_CONL_1, (uint8_t *) (conl), 3);
}

/**
 * @brief Load PWM Register and LED Control Register��s data
 */
esp_err_t is31fl3218_update_register(is31fl3218_handle_t fxled)
{
    uint8_t m = 1;
    return is31fl3218_write(fxled, IS31FL3218_REG_UPDATE, &m, 1);
}

/**
 * @brief reset all register into default
 */
esp_err_t is31fl3218_reset_register(is31fl3218_handle_t fxled)
{
    uint8_t m = 1;
    return is31fl3218_write(fxled, IS31FL3218_REG_UPDATE, &m, 1);
}

esp_err_t is31fl3218_write_pwm_regs(is31fl3218_handle_t fxled, uint8_t *duty, int len)
{
    esp_err_t ret = ESP_OK;
    ret |= is31fl3218_write(fxled, IS31FL3218_REG_PWM_1, duty, len);
    ret |= is31fl3218_update_register(fxled);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

esp_err_t is31fl3218_channel_set(is31fl3218_handle_t fxled, uint32_t ch_bit, uint8_t duty)
{
    esp_err_t ret = ESP_OK;
    ret |= is31fl3218_set_channel_duty(fxled, ch_bit, duty);
    ret |= is31fl3218_update_register(fxled);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

is31fl3218_handle_t is31fl3218_create(i2c_bus_handle_t bus)
{
    esp_err_t ret = ESP_OK;
    is31fl3218_dev_t *led = (is31fl3218_dev_t *) calloc(1, sizeof(is31fl3218_dev_t));
    IS31_CHECK(NULL != led, "Memory for is31fl3218 is not enough", NULL);
    i2c_bus_device_handle_t i2c_dev = i2c_bus_device_create(bus, IS31FL3218_I2C_ID, 0);
    if (NULL == i2c_dev) {
        free(led);
        IS31_CHECK(false, "Create i2c device failed", NULL);
    }
    led->i2c_dev = i2c_dev;
    ret |= is31fl3218_reset_register(led);
    ret |= is31fl3218_set_mode(led, IS31FL3218_MODE_NORMAL);
    ret |= is31fl3218_channel_enable(led, IS31FL3218_CH_NUM_MAX_MASK);
    ret |= is31fl3218_update_register(led);
    if (ESP_OK != ret) {
        is31fl3218_delete(led);
        IS31_CHECK(false, "Initialize is31fl3218 failed", NULL);
    }
    return (is31fl3218_handle_t) led;
}

esp_err_t is31fl3218_delete(is31fl3218_handle_t fxled)
{
    is31fl3218_dev_t *led = (is31fl3218_dev_t *) fxled;
    i2c_bus_device_delete(&led->i2c_dev);
    free(led);
    return ESP_OK;
}
