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

#include "sdkconfig.h"

#if CONFIG_RELAY_ENABLE

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "iot_relay.h"

#define DFLIPFLOP_DELAY_US      CONFIG_DFLIPFLOP_CLK_PERIOD_US
#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)

typedef struct {
    relay_status_t state;
    relay_io_t relay_io;
    relay_close_level_t close_level;
    relay_ctl_mode_t ctl_mode;
    relay_io_mode_t io_mode;
} relay_dev_t;
// Debug tag in esp log
static const char* TAG = "relay";

relay_handle_t iot_relay_create(relay_io_t relay_io, relay_close_level_t close_level, relay_ctl_mode_t ctl_mode, relay_io_mode_t io_mode)
{   
    relay_dev_t* relay_p = (relay_dev_t*) calloc(1, sizeof(relay_dev_t));
    relay_p->close_level = close_level;
    relay_p->relay_io = relay_io;
    relay_p->state = RELAY_STATUS_OPEN;
    relay_p->ctl_mode = ctl_mode;
    relay_p->io_mode = io_mode;
    if (io_mode == RELAY_IO_NORMAL) {
        uint64_t bit_mask;
        if (ctl_mode == RELAY_DFLIP_CONTROL) {
            bit_mask = BIT(relay_io.flip_io.d_io_num) | BIT(relay_io.flip_io.cp_io_num);
        }
        else {
            bit_mask = BIT(relay_io.single_io.ctl_io_num);
        }
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = bit_mask;
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        IOT_CHECK(TAG, gpio_config(&io_conf) == ESP_OK, NULL);
    }
    else {
        if (ctl_mode == RELAY_DFLIP_CONTROL) {
            IOT_CHECK(TAG, rtc_gpio_init(relay_io.flip_io.d_io_num) == ESP_OK, NULL);
            IOT_CHECK(TAG, rtc_gpio_init(relay_io.flip_io.cp_io_num) == ESP_OK, NULL);
            rtc_gpio_set_direction(relay_io.flip_io.d_io_num, RTC_GPIO_MODE_OUTPUT_ONLY);
            rtc_gpio_set_direction(relay_io.flip_io.cp_io_num, RTC_GPIO_MODE_OUTPUT_ONLY);
        }
        else {
            IOT_CHECK(TAG, rtc_gpio_init(relay_io.single_io.ctl_io_num) == ESP_OK, NULL);
            rtc_gpio_set_direction(relay_io.single_io.ctl_io_num, RTC_GPIO_MODE_OUTPUT_ONLY);
        }
    }
    return (relay_handle_t) relay_p;
}

esp_err_t iot_relay_state_write(relay_handle_t relay_handle, relay_status_t state)
{
    relay_dev_t* relay_dev = (relay_dev_t*) relay_handle;
    POINT_ASSERT(TAG, relay_handle);
    if (relay_dev->ctl_mode == RELAY_DFLIP_CONTROL) {
        if (relay_dev->io_mode == RELAY_IO_NORMAL) {
            gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 0);
            gpio_set_level(relay_dev->relay_io.flip_io.d_io_num, (0x01 & state) ^ relay_dev->close_level);
            ets_delay_us(DFLIPFLOP_DELAY_US);
            gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 1);
            ets_delay_us(DFLIPFLOP_DELAY_US);
            gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 0);
        }
        else {
            rtc_gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 0);
            rtc_gpio_set_level(relay_dev->relay_io.flip_io.d_io_num, (0x01 & state) ^ relay_dev->close_level);
            ets_delay_us(DFLIPFLOP_DELAY_US);
            rtc_gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 1);
            ets_delay_us(DFLIPFLOP_DELAY_US);
            rtc_gpio_set_level(relay_dev->relay_io.flip_io.cp_io_num, 0);
        }
    }
    else {
        if (relay_dev->io_mode == RELAY_IO_NORMAL) {
            gpio_set_level(relay_dev->relay_io.single_io.ctl_io_num, (0x01 & state) ^ relay_dev->close_level);
        }
        else {
            rtc_gpio_set_level(relay_dev->relay_io.single_io.ctl_io_num, (0x01 & state) ^ relay_dev->close_level);
        }
    }
    relay_dev->state = state;
    return ESP_OK;
}

relay_status_t iot_relay_state_read(relay_handle_t relay_handle)
{
    relay_dev_t* relay_dev = (relay_dev_t*) relay_handle;
    return relay_dev->state;
}

esp_err_t iot_relay_delete(relay_handle_t relay_handle)
{
    POINT_ASSERT(TAG, relay_handle);
    free(relay_handle);
    return ESP_OK;
}

#endif