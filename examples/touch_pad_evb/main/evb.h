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
#ifndef _TOUCH_EVB_
#define _TOUCH_EVB_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "iot_touchpad.h"
#include "driver/adc.h"
#include "iot_led.h"

#define COVER_DEBUG  0
#if COVER_DEBUG
#define TOUCHPAD_THRES_PERCENT  990
#else
#define TOUCHPAD_THRES_PERCENT  900
#endif
#define TOUCHPAD_FILTER_VALUE   10

#ifdef __cplusplus
extern QueueHandle_t q_touch;
extern "C" {
#endif

#define V_REF   1100
#define ADC1_TEST_CHANNEL (ADC1_CHANNEL_7)

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_SCL_IO GPIO_NUM_19
#define I2C_MASTER_FREQ_HZ 100000

enum {
    TOUCH_EVB_MODE_MATRIX,
    TOUCH_EVB_MODE_SLIDE,
    TOUCH_EVB_MODE_SPRING,
    TOUCH_EVB_MODE_CIRCLE,
};

typedef enum {
    TOUCH_EVT_TYPE_SINGLE,
    TOUCH_EVT_TYPE_SINGLE_PUSH,
    TOUCH_EVT_TYPE_SINGLE_RELEASE,
    TOUCH_EVT_TYPE_SPRING_PUSH,
    TOUCH_EVT_TYPE_SPRING_RELEASE,
    TOUCH_EVT_TYPE_MATRIX,
    TOUCH_EVT_TYPE_MATRIX_RELEASE,
    TOUCH_EVT_TYPE_MATRIX_SERIAL,
    TOUCH_EVT_TYPE_SLIDE,
} touch_evt_type_t;

typedef struct {
    touch_evt_type_t type;
    union {
        struct {
            int idx;
        } single;
        struct {
            int x;
            int y;
        } matrix;
    };
} touch_evt_t;

void ch450_write_led(int mask, int on_off);
void ch450_write_dig(int idx, int val);
void ch450_dev_init();
void evb_adc_init();
int  evb_adc_get_mode();

void evb_led_init();
void evb_touch_button_init();
void evb_touch_button_handle(int idx, int type);

void evb_touch_matrix_init();
void evb_touch_matrix_handle(int x, int y, int type);

void evb_touch_spring_handle(int idx, int type);
void evb_touch_spring_init();
void evb_touch_slide_init_then_run();
void evb_touch_circle_init_then_run();

#ifdef __cplusplus
}
#endif

#endif
