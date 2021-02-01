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
#include "driver/i2c.h"
#include "esp_log.h"
#include "is31fl3736.h"

static const char *TAG = "IS31FL3736";

#define IS31_CHECK(a, str, ret) if(!(a)) { \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
        return (ret); \
    }
#define IS31_ERROR_CHECK(con) if(!(con)) {ESP_LOGE(TAG,"error: %s; line: %d",__func__,__LINE__); return ESP_FAIL;}
#define IS31_PARAM_CHECK(con) if(!(con)) {ESP_LOGE(TAG,"Parameter error: %s; line: %d",__func__,__LINE__); assert(0);}

#define IS31FL3736_WRITE_BIT    0x00
#define IS31FL3736_READ_BIT     0x01
#define IS31FL3736_ACK_CHECK_EN 1
#define IS31FL3736_READ_ACK     0x0         /*!< I2C ack value */
#define IS31FL3736_READ_NACK    0x1         /*!< I2C nack value */
#define IS31FL3736_CMD_WRITE_EN 0xC5        /*!< magic num */
#define IS31FL3736_I2C_ID       0xA0        /*!< I2C Addr,up to ADDR1/ADDR2 pin */

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    gpio_num_t rst_io;
    is31fl3736_addr_pin_t addr1;
    is31fl3736_addr_pin_t addr2;
    uint8_t cur_val;
} is31fl3736_dev_t;

esp_err_t is31fl3736_write_page(is31fl3736_handle_t fxled, uint8_t page_num)
{
    esp_err_t ret = ESP_OK;
    IS31_PARAM_CHECK(page_num < 3);
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (led->dev_addr) | IS31FL3736_WRITE_BIT, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, IS31FL3736_RET_CMD_LOCK, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, IS31FL3736_CMD_WRITE_EN, IS31FL3736_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret |= i2c_bus_cmd_begin(led->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (led->dev_addr) | IS31FL3736_WRITE_BIT, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, IS31FL3736_REG_CMD, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, page_num, IS31FL3736_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret |= i2c_bus_cmd_begin(led->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "%s:%d (%s):i2c transmit failed [%s]", __FILE__, __LINE__, __FUNCTION__, esp_err_to_name(ret));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t is31fl3736_write(is31fl3736_handle_t fxled, uint8_t reg_addr, uint8_t *data, uint8_t data_num)
{
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    IS31_PARAM_CHECK(NULL != data);
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (led->dev_addr) | IS31FL3736_WRITE_BIT, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write(cmd, data, data_num, IS31FL3736_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_bus_cmd_begin(led->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "%s:%d (%s):i2c transmit failed [%s]", __FILE__, __LINE__, __FUNCTION__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t is31fl3736_read_reg(is31fl3736_handle_t fxled, uint8_t reg_addr, uint8_t *data)
{
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (led->dev_addr) | IS31FL3736_WRITE_BIT, IS31FL3736_ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, IS31FL3736_ACK_CHECK_EN);
    i2c_master_stop(cmd);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (led->dev_addr) | IS31FL3736_READ_BIT, IS31FL3736_ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, IS31FL3736_READ_NACK);
    i2c_master_stop(cmd);
    ret = i2c_bus_cmd_begin(led->i2c_dev, cmd);
    i2c_cmd_link_delete(cmd);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "%s:%d (%s):i2c transmit failed [%s]", __FILE__, __LINE__, __FUNCTION__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t is31fl3736_set_mode(is31fl3736_handle_t fxled, is31fl3736_mode_t mode)
{
    uint8_t reg_val;
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(3);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_PG3_CONFIG, (uint8_t *)&mode, 1));
    return ESP_OK;
}

esp_err_t is31fl3736_set_global_current(is31fl3736_handle_t fxled, uint8_t curr_value)
{
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    uint8_t reg_val;
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(3);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    reg_val = curr_value;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_PG3_CURR, &reg_val, 1));
    led->cur_val = curr_value;
    return ESP_OK;
}

esp_err_t is31fl3736_set_resistor(is31fl3736_handle_t fxled, is31fl3736_res_type_t type, is31fl3736_res_t res_val)
{
    uint8_t reg_val;
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(3);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    reg_val = res_val;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, type, &reg_val, 1));
    return ESP_OK;
}

/**
 * @brief reset all register into default
 */
esp_err_t is31fl3736_reset_reg(is31fl3736_handle_t fxled)
{
    uint8_t reg_val;
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(3);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    reg_val = 0;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_read_reg(fxled, IS31FL3736_REG_PG3_RESET, &reg_val));
    return ESP_OK;
}

/**
 * @brief change LED channels ON/OFF in LED Control mode
 * current source (CS-X) 1~8;
 * switch scan (SW-Y) 1 ~ 12; all == 8 * 12 == 96
 */
esp_err_t is31fl3736_set_led_matrix(is31fl3736_handle_t fxled, uint16_t cs_x_bit, uint16_t sw_y_bit,
                                        is31fl3736_led_stau_t status)
{
    int i, j, k;
    uint8_t reg, reg_mask, reg_val, temp;
    is31fl3736_write_page(fxled, IS31FL3736_PAGE(0));
    for (i = 0; i < 2; i++) {
        if ((cs_x_bit >> (i * 4)) & 0xf) {
            for (j = 0; j < IS31FL3736_SWY_MAX; j++) {
                if ((sw_y_bit >> j) & 0x1) {
                    reg = IS31FL3736_REG_PG0_SWITCH(j * 2 + i); //find which reg to write
                    reg_mask = 0;
                    temp = 0xF & (cs_x_bit >> ((i) * 4));
                    for (k = 0; k < 4; k++) {
                        if ((temp >> k) & 0x1) {
                            reg_mask |= (0x1 << (2 * k));
                        }
                    }
                    if (status == IS31FL3736_LED_ON) {
                        reg_val = reg_mask;
                    } else {
                        reg_val = 0;
                    }
                    ESP_LOGD(TAG, "reg 0x%02X; val 0x%02x\r\n", reg, reg_val);
                    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, reg, &reg_val, 1));
                }
            }
        }
    }
    return ESP_OK;
}

esp_err_t is31fl3736_fill_buf(is31fl3736_handle_t fxled, uint8_t duty, uint8_t *buf)
{
    /* print the image */
    for (int i = 0; i < IS31FL3736_SWY_MAX; i++) {
        is31fl3736_set_pwm_duty_matrix(fxled, buf[i], IS31FL3736_CH_BIT(i), duty);
    }
    return ESP_OK;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t is31fl3736_set_pwm_duty_matrix(is31fl3736_handle_t fxled, uint16_t cs_x_bit, uint16_t sw_y_bit, uint8_t duty)
{
    int i, j;
    uint8_t reg, reg_val;
    is31fl3736_write_page(fxled, IS31FL3736_PAGE(1));
    for (i = 0; i < IS31FL3736_CSX_MAX; i++) {
        for (j = 0; j < IS31FL3736_SWY_MAX; j++) {
            if (((cs_x_bit >> i) & 0x1) && ((sw_y_bit >> j) & 0x1)) {
                reg = i * 2 + j * 0x10;
                reg_val = duty;
                ESP_LOGD(TAG, "reg %02X; val 0x%02x\r\n", reg, reg_val);
                IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, reg, &reg_val, 1));
            } else if (((sw_y_bit >> j) & 0x1)) {
                reg = i * 2 + j * 0x10;
                reg_val = 0;
                ESP_LOGD(TAG, "reg %02X; val 0x%02x\r\n", reg, reg_val);
                IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, reg, &reg_val, 1));
            }
        }
    }
    return ESP_OK;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t is31fl3736_set_channel_duty(is31fl3736_handle_t fxled, uint8_t pwm_ch, uint8_t duty)
{
    uint8_t reg_val;
    IS31_PARAM_CHECK(pwm_ch <= IS31FL3736_PWM_CHANNEL_MAX);
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(1);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, pwm_ch, &duty, 1));
    return ESP_OK;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t is31fl3736_set_breath_mode(is31fl3736_handle_t fxled, uint8_t pwm_ch, is31fl3736_auto_breath_mode_t mode)
{
    uint8_t reg_val;
    IS31_PARAM_CHECK(pwm_ch <= IS31FL3736_PWM_CHANNEL_MAX);
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(1);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    reg_val = mode;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, pwm_ch, &reg_val, 1));
    return ESP_OK;
}

/**
 * @brief Load PWM Register and LED Control Register��s data
 */
esp_err_t is31fl3736_update_auto_breath(is31fl3736_handle_t fxled)
{
    uint8_t reg_val;
    reg_val = IS31FL3736_CMD_WRITE_EN;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_RET_CMD_LOCK, &reg_val, 1));
    reg_val = IS31FL3736_PAGE(3);
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_CMD, &reg_val, 1));
    reg_val = 0;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, IS31FL3736_REG_PG3_UPDATE, &reg_val, 1));
    return ESP_OK;
}

/**
 * @brief change SDB io status to reset IC
 */
esp_err_t is31fl3736_hw_reset(is31fl3736_handle_t fxled)
{
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    if (led->rst_io < GPIO_NUM_MAX) {
        gpio_set_level(led->rst_io, 0);
        vTaskDelay(10 / portTICK_RATE_MS);
        gpio_set_level(led->rst_io, 1);
    }
    return ESP_OK;
}

static esp_err_t is31fl3736_init(is31fl3736_handle_t fxled)
{
    is31fl3736_mode_t mode;
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_hw_reset(fxled));
    mode.val = 0;
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_set_mode(fxled, mode));
    /* Open all LED */
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_set_led_matrix(fxled, IS31FL3736_CSX_MAX_MASK, IS31FL3736_SWY_MAX_MASK, IS31FL3736_LED_ON));
    /* Set PWM data to 0 */
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_set_pwm_duty_matrix(fxled, IS31FL3736_CSX_MAX_MASK, IS31FL3736_SWY_MAX_MASK, 0));
    mode.normal_en = 1;
    /* Release software shutdown to normal operation */
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_set_mode(fxled, mode));
    /* Set global current */
    IS31_ERROR_CHECK(ESP_OK == is31fl3736_set_global_current(fxled, led->cur_val));

    uint8_t reg, reg_val;
    is31fl3736_write_page(fxled, IS31FL3736_PAGE(1));
    for (int i = 0; i < IS31FL3736_CSX_MAX; i++) {
        for (int j = 0; j < IS31FL3736_SWY_MAX; j++) {
            reg = i * 2 + j * 0x10;
            reg_val = 0;
            ESP_LOGD(TAG, "reg %02X; val 0x%02x\r\n", reg, reg_val);
            IS31_ERROR_CHECK(ESP_OK == is31fl3736_write(fxled, reg, &reg_val, 1));
        }
    }
    return ESP_OK;
}

uint8_t is31fl3736_get_i2c_addr(is31fl3736_addr_pin_t addr1_pin, is31fl3736_addr_pin_t addr2_pin)
{
    uint8_t addr = IS31FL3736_I2C_ID | ((uint8_t) addr1_pin << 1) | ((uint8_t) addr2_pin << 3);
    ESP_LOGI(TAG, "slave ADDR : %02x", (uint8_t )addr);
    return addr;
}

is31fl3736_handle_t is31fl3736_create(i2c_bus_handle_t bus, gpio_num_t rst_io, is31fl3736_addr_pin_t addr1, is31fl3736_addr_pin_t addr2, uint8_t cur_val)
{
    esp_err_t ret = ESP_OK;
    is31fl3736_dev_t *fxled = (is31fl3736_dev_t *) calloc(1, sizeof(is31fl3736_dev_t));
    IS31_CHECK(NULL != fxled, "Memory for is31fl3736 is not enough", NULL);
    i2c_bus_device_handle_t i2c_dev = i2c_bus_device_create(bus, is31fl3736_get_i2c_addr(addr1, addr2), 0);
    if (NULL == i2c_dev) {
        free(fxled);
        IS31_CHECK(false, "Create i2c device failed", NULL);
    }
    fxled->i2c_dev = i2c_dev;
    fxled->dev_addr = is31fl3736_get_i2c_addr(addr1, addr2);
    fxled->rst_io = rst_io;
    fxled->cur_val = cur_val;
    if (rst_io < GPIO_NUM_MAX && rst_io >= 0) {
        gpio_config_t gpio_conf = {0};
        gpio_conf.intr_type = GPIO_INTR_DISABLE,
        gpio_conf.mode = GPIO_MODE_OUTPUT,
        gpio_conf.pin_bit_mask = 1ULL << rst_io,
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE,
        gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE,
        gpio_config(&gpio_conf);
    }
    ret = is31fl3736_init(fxled);
    if (ESP_OK != ret) {
        is31fl3736_delete(fxled);
        IS31_CHECK(false, "Initialize is31fl3736 failed", NULL);
    }
    return (is31fl3736_handle_t) fxled;
}

esp_err_t is31fl3736_delete(is31fl3736_handle_t fxled)
{
    is31fl3736_dev_t *led = (is31fl3736_dev_t *) fxled;
    i2c_bus_device_delete(&led->i2c_dev);
    free(led);
    return ESP_OK;
}

