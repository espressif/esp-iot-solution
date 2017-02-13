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
    BUTTON_STATE_PUSH,
    BUTTON_STATE_PRESSED,
} button_status_t;

typedef struct button_dev button_dev_t;
typedef struct {
    TickType_t interval;
    button_cb cb;
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
    button_cb_t tap_short_cb;
    button_cb_t tap_psh_cb;
    button_cb_t tap_rls_cb;
    button_cb_t cb[0];
};

static void button_press_cb(xTimerHandle tmr)
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

static void button_tap_psh_cb(xTimerHandle tmr)
{
    button_cb_t* btn_cb = (button_cb_t*) pvTimerGetTimerID(tmr);
    button_dev_t* btn = btn_cb->pbtn;
    xTimerStop(btn->tap_rls_cb.tmr, 10);
    if (btn->active_level == gpio_get_level(btn->io_num)) {
        // high, then key is up
        btn->state = BUTTON_STATE_PUSH;
        if (btn->tap_psh_cb.cb) {
            btn->tap_psh_cb.cb(btn->tap_psh_cb.arg);
        }
    } else {

    }
}

static void button_tap_rls_cb(xTimerHandle tmr)
{
    button_cb_t* btn_cb = (button_cb_t*) pvTimerGetTimerID(tmr);
    button_dev_t* btn = btn_cb->pbtn;
    xTimerStop(btn->tap_rls_cb.tmr, 10);
    int i;
    if (btn->active_level == gpio_get_level(btn->io_num)) {

    } else {
        // high, then key is up
        for (i = 0; i < btn->cb_num; i++) {
            xTimerStop(btn->cb[i].tmr, 10);
        }
        if (btn->tap_short_cb.cb && btn->state == BUTTON_STATE_PUSH) {
            btn->tap_short_cb.cb(btn->tap_short_cb.arg);
        }
        if(btn->tap_rls_cb.cb) {
            btn->tap_rls_cb.cb(btn->tap_rls_cb.arg);
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
        if (btn->tap_psh_cb.tmr) {
            xTimerStopFromISR(btn->tap_psh_cb.tmr, &HPTaskAwoken);
            xTimerResetFromISR(btn->tap_psh_cb.tmr, &HPTaskAwoken);
        }

        for (i = 0; i < btn->cb_num; i++) {
            if (btn->cb[i].tmr) {
                xTimerStopFromISR(btn->cb[i].tmr, &HPTaskAwoken);
                xTimerResetFromISR(btn->cb[i].tmr, &HPTaskAwoken);
            }
        }
    } else {
        // 50ms, check if this is a real key up
        if (btn->tap_rls_cb.tmr) {
            xTimerStopFromISR(btn->tap_rls_cb.tmr, &HPTaskAwoken);
            xTimerResetFromISR(btn->tap_rls_cb.tmr, &HPTaskAwoken);
        }
    }
}

static void button_free_tmr(xTimerHandle* tmr)
{
    if (tmr && *tmr) {
        xTimerStop(*tmr, portMAX_DELAY);
        xTimerDelete(*tmr, portMAX_DELAY);
        *tmr = NULL;
    }
}

esp_err_t button_dev_free(button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    gpio_set_intr_type(btn->io_num, GPIO_INTR_DISABLE);
    gpio_isr_handler_remove(btn->io_num);

    button_free_tmr(&btn->tap_rls_cb.tmr);
    button_free_tmr(&btn->tap_psh_cb.tmr);
    button_free_tmr(&btn->tap_short_cb.tmr);

    int i;
    for (i = 0; i < btn->cb_num; i++) {
        button_free_tmr(&btn->cb[i].tmr);
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
    btn->tap_rls_cb.arg = NULL;
    btn->tap_rls_cb.cb = NULL;
    btn->tap_rls_cb.interval = 50 / portTICK_PERIOD_MS;
    sprintf((char* )btn->tap_rls_cb.name, "timer_release");
    btn->tap_rls_cb.pbtn = btn;
    btn->tap_rls_cb.tmr = xTimerCreate((const char*) btn->tap_rls_cb.name, btn->tap_rls_cb.interval, pdFALSE,
            &btn->tap_rls_cb, button_tap_rls_cb);
    btn->tap_psh_cb.arg = NULL;
    btn->tap_psh_cb.cb = NULL;
    btn->tap_psh_cb.interval = 50 / portTICK_PERIOD_MS;
    sprintf((char* )btn->tap_psh_cb.name, "timer_push");
    btn->tap_psh_cb.pbtn = btn;
    btn->tap_psh_cb.tmr = xTimerCreate((const char*) btn->tap_psh_cb.name, btn->tap_psh_cb.interval, pdFALSE,
            &btn->tap_psh_cb, button_tap_psh_cb);
    gpio_install_isr_service(0);
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1 << gpio_num);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_conf);
    gpio_isr_handler_add(gpio_num, button_gpio_isr_handler, btn);
    return (button_handle_t) btn;
}

esp_err_t button_dev_rm_tap_cb(button_cb_type_t type, button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    button_cb_t* btn_cb = NULL;
    if (type == BUTTON_PUSH_CB) {
        btn_cb = &btn->tap_psh_cb;
    } else if (type == BUTTON_RELEASE_CB) {
        btn_cb = &btn->tap_rls_cb;
    } else if (type == BUTTON_TAP_CB) {
        btn_cb = &btn->tap_short_cb;
    }
    btn_cb->cb = NULL;
    btn_cb->arg = NULL;
    btn_cb->pbtn = btn;
    button_free_tmr(&btn_cb->tmr);
    return ESP_OK;
}

esp_err_t button_dev_add_tap_cb(button_cb_type_t type, button_cb cb, void* arg, int interval_tick,
        button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    if (type == BUTTON_PUSH_CB) {
        btn->tap_psh_cb.arg = arg;
        btn->tap_psh_cb.cb = cb;
        btn->tap_psh_cb.interval = interval_tick;
        btn->tap_psh_cb.pbtn = btn;
        sprintf((char* )btn->tap_psh_cb.name, "btn_psh_tmr");
        button_free_tmr(&btn->tap_psh_cb.tmr);
        btn->tap_psh_cb.tmr = xTimerCreate((const char*) btn->tap_psh_cb.name, interval_tick, pdFALSE, &btn->tap_psh_cb,
                button_tap_psh_cb);
    } else if (type == BUTTON_RELEASE_CB) {
        btn->tap_rls_cb.arg = arg;
        btn->tap_rls_cb.cb = cb;
        btn->tap_rls_cb.interval = interval_tick;
        btn->tap_rls_cb.pbtn = btn;
        sprintf((char* )btn->tap_rls_cb.name, "btn_rls_tmr");
        button_free_tmr(&btn->tap_rls_cb.tmr);
        btn->tap_rls_cb.tmr = xTimerCreate((const char*) btn->tap_rls_cb.name, interval_tick, pdFALSE, &btn->tap_rls_cb,
                button_tap_rls_cb);
    } else if (type == BUTTON_TAP_CB) {
        btn->tap_short_cb.arg = arg;
        btn->tap_short_cb.cb = cb;
        btn->tap_short_cb.interval = interval_tick;
        btn->tap_short_cb.pbtn = btn;
        sprintf((char* )btn->tap_short_cb.name, "btn_short_tmr");
        button_free_tmr(&btn->tap_short_cb.tmr);
    }
    return ESP_OK;
}

esp_err_t button_dev_rm_press_cb(int cb_idx, button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    btn->cb[cb_idx].arg = NULL;
    btn->cb[cb_idx].cb = NULL;
    btn->cb[cb_idx].interval = 50 / portTICK_PERIOD_MS;
    button_free_tmr(&btn->cb[cb_idx].tmr);
    return ESP_OK;
}

esp_err_t button_dev_add_press_cb(int cb_idx, button_cb cb, void* arg, int interval_tick, button_handle_t btn_handle)
{
    button_dev_t* btn = (button_dev_t*) btn_handle;
    btn->cb[cb_idx].arg = arg;
    btn->cb[cb_idx].cb = cb;
    btn->cb[cb_idx].interval = interval_tick;
    btn->cb[cb_idx].pbtn = btn;
    sprintf((char* )btn->cb[cb_idx].name, "btn_tmr_%d", cb_idx);
    btn->cb[cb_idx].tmr = xTimerCreate((const char*) btn->cb[cb_idx].name, interval_tick, pdFALSE, &btn->cb[cb_idx],
            button_press_cb);
    return ESP_OK;
}
