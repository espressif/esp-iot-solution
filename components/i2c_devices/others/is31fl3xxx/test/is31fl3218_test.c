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
  
#define IS31FL3218_TEST_CODE 1
#if IS31FL3218_TEST_CODE


#include <stdio.h>
#include "driver/i2c.h"
#include "is31fl3218.h"
#include "is31fl3736.h"
#include "i2c_bus.h"
#include "unity.h"

#define I2C_MASTER_SCL_IO    21        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    15        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM     I2C_NUM_1   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000   /*!< I2C master clock frequency */

static i2c_bus_handle_t i2c_bus = NULL;
static is31fl3218_handle_t fxled = NULL;

#define DEBUG_MODE  0
#if DEBUG_MODE
#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"
#endif

/**
 * @brief i2c master initialization
 */
void fxled_is31fl3218_init()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    if(fxled ==NULL) {
        fxled = led_is31fl3218_create(i2c_bus);
    }
}

void is31f13218_test()
{
    fxled_is31fl3218_init();

    uint8_t i=0,j=255/10;
    ESP_ERROR_CHECK( is31fl3218_channel_set(fxled, IS31FL3218_CH_NUM_MAX_MASK, 0) );
    int cnt = 0;
    printf("enter loop\n");
    uint8_t duty_data[18] = {0};
    while(1){
        is31fl3218_write_pwm_regs(fxled, duty_data, sizeof(duty_data));
        vTaskDelay(20 / portTICK_RATE_MS);
#if DEBUG_MODE
        while (1) {
            if (ret == ESP_FAIL) {
                ets_printf("in test: Channel set fail, retry\n");
                printf("status: 0x%08x\n", READ_PERI_REG(I2C_SR_REG(1)));
                printf("ack rec: %d\n", I2C1.status_reg.ack_rec);
                printf("ack arb_lost: %d\n", I2C1.status_reg.arb_lost);
                printf("ack bus_busy: %d\n", I2C1.status_reg.bus_busy);
                printf("ack byte_trans: %d\n", I2C1.status_reg.byte_trans);
                printf("ack rx_fifo_cnt: %d\n", I2C1.status_reg.rx_fifo_cnt);
                printf("ack scl_main_state_last: %d\n", I2C1.status_reg.scl_main_state_last);
                printf("ack scl_state_last: %d\n", I2C1.status_reg.scl_state_last);
                printf("ack slave_addressed: %d\n", I2C1.status_reg.slave_addressed);
                printf("ack slave_rw: %d\n", I2C1.status_reg.slave_rw);
                printf("ack time_out: %d\n", I2C1.status_reg.time_out);
                printf("ack tx_fifo_cnt: %d\n", I2C1.status_reg.tx_fifo_cnt);


//                ret = is31fl3218_channel_set(info.i2c_port, IS31FL3218_CH_BIT(i), j);
                ret = is31fl3218_write_pwm_regs(info.i2c_port, duty_data, sizeof(duty_data));
                vTaskDelay(10 / portTICK_RATE_MS);
            } else if (ret == ESP_ERR_TIMEOUT) {

                printf("status: 0x%08x\n", READ_PERI_REG(I2C_SR_REG(1)));
                printf("ack rec: %d\n", I2C1.status_reg.ack_rec);
                printf("ack arb_lost: %d\n", I2C1.status_reg.arb_lost);
                printf("ack bus_busy: %d\n", I2C1.status_reg.bus_busy);
                printf("ack byte_trans: %d\n", I2C1.status_reg.byte_trans);
                printf("ack rx_fifo_cnt: %d\n", I2C1.status_reg.rx_fifo_cnt);
                printf("ack scl_main_state_last: %d\n", I2C1.status_reg.scl_main_state_last);
                printf("ack scl_state_last: %d\n", I2C1.status_reg.scl_state_last);
                printf("ack slave_addressed: %d\n", I2C1.status_reg.slave_addressed);
                printf("ack slave_rw: %d\n", I2C1.status_reg.slave_rw);
                printf("ack time_out: %d\n", I2C1.status_reg.time_out);
                printf("ack tx_fifo_cnt: %d\n", I2C1.status_reg.tx_fifo_cnt);
//                while(1);
                ets_printf("in test: Channel set timeout, reinit...\n");
                i2c_master_reinit();
                printf("status: 0x%08x\n", READ_PERI_REG(I2C_SR_REG(1)));
                printf("ack rec: %d\n", I2C1.status_reg.ack_rec);
                printf("ack arb_lost: %d\n", I2C1.status_reg.arb_lost);
                printf("ack bus_busy: %d\n", I2C1.status_reg.bus_busy);
                printf("ack byte_trans: %d\n", I2C1.status_reg.byte_trans);
                printf("ack rx_fifo_cnt: %d\n", I2C1.status_reg.rx_fifo_cnt);
                printf("ack scl_main_state_last: %d\n", I2C1.status_reg.scl_main_state_last);
                printf("ack scl_state_last: %d\n", I2C1.status_reg.scl_state_last);
                printf("ack slave_addressed: %d\n", I2C1.status_reg.slave_addressed);
                printf("ack slave_rw: %d\n", I2C1.status_reg.slave_rw);
                printf("ack time_out: %d\n", I2C1.status_reg.time_out);
                printf("ack tx_fifo_cnt: %d\n", I2C1.status_reg.tx_fifo_cnt);
                while(1);
                vTaskDelay(200 / portTICK_PERIOD_MS);
//                ret = is31fl3218_channel_set(info.i2c_port, IS31FL3218_CH_BIT(i), j);
                ret = is31fl3218_write_pwm_regs(info.i2c_port, duty_data, sizeof(duty_data));
                vTaskDelay(10 / portTICK_RATE_MS);
            } else {
                break;
            }
        }
#endif
        if (++i > IS31FL3218_CH_NUM_MAX - 1) {
            i = 0;
            if (j == 0) {
                j = 255 / 10;
                if (cnt++ > 1) {
                    break;
                }
            } else {
                j = 0;
            }
        }
        duty_data[i] = j;
    }

    cnt = 0;
    uint32_t ch_mask = 0x1f;
    int dcnt = 4;
    while (1) {
        vTaskDelay(10 / portTICK_RATE_MS);
        for (int i = 0; i < 18; i++) {
            if ((ch_mask >> i) & 0x1) {
                if (dcnt >= 4) {
                    int duty = 5 - (dcnt - i);
                    is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                } else {
                    int duty = 5 - (dcnt - i);
                    if (duty < 0) {
                        duty += 16;
                    }
                    is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0xff * duty / 5);
                }
            } else {
                is31fl3218_channel_set(fxled, IS31FL3218_CH_BIT(i), 0);
            }
        }
        ch_mask = ch_mask << 1;
        ch_mask |= (ch_mask >> 18);
        dcnt++;
        if (dcnt > 17) {
            dcnt = 0;
            if (cnt++ > 4) {
                is31fl3218_channel_set(fxled, 0xfffff, 0xff * 0 / 5);
                break;
            }
        }
    }

    led_is31fl3218_delete(fxled, true);
    fxled = NULL;
    i2c_bus = NULL;
    printf("heap: %d\n", esp_get_free_heap_size());
}

TEST_CASE("I2C led is31f13218 test", "[is31f13218][iot][fxled]")
{
    is31f13218_test();
}

#endif

