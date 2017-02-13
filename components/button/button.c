/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "button.h"

typedef enum {
    BUTTON_STATE_IDLE = 0,
    BUTTON_STATE_PUSH = 1,
    BUTTON_STATE_RELEASE = 2,
    BUTTON_STATE_PRESSED = 3,
} button_status_t;

typedef struct button_dev button_dev_t;
typedef struct {
    TickType_t interval;
    key_cb cb;
    void* arg;
    TimerHandle_t tmr;
    uint8_t name[32];
    button_dev_t* pbtn;
} button_cb_t;

struct button_dev{
    uint8_t io_num;
    uint8_t active_level;
    int cb_num;
    button_status_t state;
    button_cb_t tap_cb;
    button_cb_t cb[0];
};

static void key_press_cb(xTimerHandle tmr)
{
    button_cb_t* btn_cb = (button_cb_t*) pvTimerGetTimerID(tmr);
    button_dev_t* btn = btn_cb->pbtn;
    // low, then restart
    if (btn->active_level == gpio_get_level(btn->io_num)) {
        btn->state = BUTTON_STATE_PRESSED;
        if (btn_cb->cb) {
            btn_cb->cb(btn_cb->arg);
        }
    }
}

static void key_tap_cb(xTimerHandle tmr)
{
    button_cb_t* btn_cb = (button_cb_t*) pvTimerGetTimerID(tmr);
    button_dev_t* btn = btn_cb->pbtn;
    xTimerStop(btn->tap_cb.tmr, 10);
    int i;
    if (btn->active_level == gpio_get_level(btn->io_num)) {

    } else {
        // high, then key is up
        for (i = 0; i < btn->cb_num; i++) {
            xTimerStop(btn->cb[i].tmr, 10);
        }
        if (btn->tap_cb.cb && btn->state != BUTTON_STATE_PRESSED) {
            btn->tap_cb.cb(btn->tap_cb.arg);
        }
        btn->state = BUTTON_STATE_IDLE;
    }
}

static void button_gpio_isr_handler(void* arg)
{
    button_dev_t* btn = (button_dev_t*) arg;
    portBASE_TYPE HPTaskAwoken = pdFALSE;
    int i;
    int level = gpio_get_level(btn->io_num);
    if (level == btn->active_level) {
        for (i = 0; i < btn->cb_num; i++) {
            if (btn->cb[i].tmr) {
                xTimerStopFromISR(btn->cb[i].tmr, &HPTaskAwoken);
                xTimerResetFromISR(btn->cb[i].tmr, &HPTaskAwoken);
            }
        }
    } else {
        // 50ms, check if this is a real key up
        if (btn->tap_cb.tmr) {
            xTimerStopFromISR(btn->tap_cb.tmr, &HPTaskAwoken);
            xTimerResetFromISR(btn->tap_cb.tmr, &HPTaskAwoken);
        }
    }
}

esp_err_t button_dev_free(button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    gpio_set_intr_type(btn->io_num, GPIO_INTR_DISABLE);
    gpio_isr_handler_remove(btn->io_num);
    xTimerStop(btn->tap_cb.tmr, 10);
    xTimerDelete(btn->tap_cb.tmr, portMAX_DELAY);
    btn->tap_cb.tmr = NULL;
    int i;
    for(i=0;i< btn->cb_num;i++) {
        if(btn->cb[i].tmr) {
            xTimerStop(btn->cb[i].tmr, portMAX_DELAY);
            xTimerDelete(btn->cb[i].tmr, portMAX_DELAY);
            btn->cb[i].tmr = NULL;
        }
    }
    free(btn);
    return ESP_OK;
}

button_handle_t button_dev_init(gpio_num_t gpio_num, int cb_num, button_active_t active_level)
{
    button_dev_t* btn = (button_dev_t*) calloc(1, sizeof(button_dev_t) + cb_num * sizeof(button_cb_t));
    btn->active_level = active_level;
    btn->cb_num = cb_num;
    btn->io_num = gpio_num;
    btn->state = BUTTON_STATE_IDLE;
    btn->tap_cb.arg = NULL;
    btn->tap_cb.cb = NULL;
    btn->tap_cb.interval = 50 / portTICK_PERIOD_MS;
    sprintf((char* )btn->tap_cb.name, "timer_tap");
    btn->tap_cb.pbtn = btn;
    btn->tap_cb.tmr = xTimerCreate((const char*) btn->tap_cb.name, btn->tap_cb.interval, pdFALSE, &btn->tap_cb,
            key_tap_cb);
    gpio_config_t gpio_conf;
    gpio_install_isr_service(0);
    gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1 << gpio_num);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_conf);
    gpio_isr_handler_add(gpio_num, button_gpio_isr_handler, btn);
    return (button_handle_t) btn;
}

esp_err_t button_dev_add_tap_cb(key_cb cb, void* arg, int interval_tick, button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    btn->tap_cb.arg = arg;
    btn->tap_cb.cb = cb;
    btn->tap_cb.interval = interval_tick;
    btn->tap_cb.pbtn = btn;
    sprintf((char* )btn->tap_cb.name, "btn_tap_tmr");
    xTimerDelete(btn->tap_cb.tmr, 10);
    btn->tap_cb.tmr = xTimerCreate((const char*) btn->tap_cb.name, interval_tick, pdFALSE, &btn->tap_cb, key_tap_cb);
    return ESP_OK;
}

esp_err_t button_dev_add_press_cb(int cb_idx, key_cb cb, void* arg, int interval_tick, button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    btn->cb[cb_idx].arg = arg;
    btn->cb[cb_idx].cb = cb;
    btn->cb[cb_idx].interval = interval_tick;
    btn->cb[cb_idx].pbtn = btn;
    sprintf((char* )btn->cb[cb_idx].name, "btn_tmr_%d", cb_idx);
    btn->cb[cb_idx].tmr = xTimerCreate((const char*) btn->cb[cb_idx].name, interval_tick, pdFALSE, &btn->cb[cb_idx],
            key_press_cb);
    return ESP_OK;
}
