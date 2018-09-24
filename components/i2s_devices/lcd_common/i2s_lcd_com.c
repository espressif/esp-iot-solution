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
#include "esp_system.h"
#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "driver/gpio.h"
#include "i2s_lcd_com.h"
#include "iot_i2s_lcd.h"
#include "sdkconfig.h"

#define I2S0_FIFO_ADD 0x6000f000
#define I2S1_FIFO_ADD 0x6002d000

static i2s_dev_t *I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};
static uint32_t I2S_FIFO_ADD[I2S_NUM_MAX] = {I2S0_FIFO_ADD, I2S1_FIFO_ADD};

#ifdef CONFIG_BIT_MODE_8BIT

void iot_i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd_handle, uint16_t data)
{
    i2s_lcd_write_data(i2s_lcd_handle, (char *)&data, 2, 100, true);
}

void iot_i2s_lcd_write_cmd(i2s_lcd_handle_t i2s_lcd_handle, uint16_t cmd)
{
    i2s_lcd_t *i2s_lcd = (i2s_lcd_t *)i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    GPIO.out_w1tc = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);
    REG_WRITE(I2S_FIFO_ADD[i2s_num], (cmd >> 8));
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;

    REG_WRITE(I2S_FIFO_ADD[i2s_num], (cmd << 8));
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
    GPIO.out_w1ts = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);
}

void iot_i2s_lcd_write_reg(i2s_lcd_handle_t i2s_lcd_handle, uint16_t cmd, uint16_t data)
{
    i2s_lcd_t *i2s_lcd = (i2s_lcd_t *)i2s_lcd_handle;
    gpio_set_level(i2s_lcd->i2s_lcd_conf.rs_io_num, 0);
    i2s_lcd_write_data(i2s_lcd_handle, (char *)&cmd, 2, 100, true);
    gpio_set_level(i2s_lcd->i2s_lcd_conf.rs_io_num, 1);
    i2s_lcd_write_data(i2s_lcd_handle, (char *)&data, 2, 100, true);
}

void iot_i2s_lcd_write(i2s_lcd_handle_t i2s_lcd_handle, uint16_t *data, uint32_t len)
{
    i2s_lcd_write_data(i2s_lcd_handle, (char *)data, len, 100, true);
}

#else // CONFIG_BIT_MODE_16BIT

void iot_i2s_lcd_write_data(i2s_lcd_handle_t i2s_lcd_handle, uint16_t data)
{
    i2s_lcd_t *i2s_lcd = (i2s_lcd_t *)i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    REG_WRITE(I2S_FIFO_ADD[i2s_num], data);
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
}

void iot_i2s_lcd_write_cmd(i2s_lcd_handle_t i2s_lcd_handle, uint16_t cmd)
{
    i2s_lcd_t *i2s_lcd = (i2s_lcd_t *)i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    GPIO.out_w1tc = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);
    REG_WRITE(I2S_FIFO_ADD[i2s_num], cmd);
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
    GPIO.out_w1ts = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);
}

void iot_i2s_lcd_write_reg(i2s_lcd_handle_t i2s_lcd_handle, uint16_t cmd, uint16_t data)
{
    i2s_lcd_t *i2s_lcd = (i2s_lcd_t *)i2s_lcd_handle;
    i2s_port_t i2s_num = i2s_lcd->i2s_port;
    GPIO.out_w1tc = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);
    REG_WRITE(I2S_FIFO_ADD[i2s_num], cmd);
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
    GPIO.out_w1ts = (1 << i2s_lcd->i2s_lcd_conf.rs_io_num);

    REG_WRITE(I2S_FIFO_ADD[i2s_num], data);
    I2S[i2s_num]->conf.tx_start = 1;
    while (!(I2S[i2s_num]->state.tx_idle)) {
        // vTaskDelay(20);
        ;
    }
    I2S[i2s_num]->conf.tx_start = 0;
    I2S[i2s_num]->conf.tx_reset = 1;
    I2S[i2s_num]->conf.tx_reset = 0;
    I2S[i2s_num]->conf.tx_fifo_reset = 1;
    I2S[i2s_num]->conf.tx_fifo_reset = 0;
}

void iot_i2s_lcd_write(i2s_lcd_handle_t i2s_lcd_handle, uint16_t *data, uint32_t len)
{
    i2s_lcd_write_data(i2s_lcd_handle, (char *)data, len, 100, false);
}

#endif // CONFIG_BIT_MODE_16BIT

i2s_lcd_handle_t iot_i2s_lcd_pin_cfg(i2s_port_t i2s_port, i2s_lcd_config_t *i2s_lcd_pin_conf)
{
    i2s_lcd_handle_t i2s_lcd_handle;
    i2s_lcd_handle = i2s_lcd_create(i2s_port, i2s_lcd_pin_conf);
    gpio_set_direction(i2s_lcd_pin_conf->rs_io_num, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(i2s_lcd_pin_conf->rs_io_num, GPIO_PULLUP_ONLY);
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[i2s_lcd_pin_conf->rs_io_num], PIN_FUNC_GPIO);
    return i2s_lcd_handle;
}
