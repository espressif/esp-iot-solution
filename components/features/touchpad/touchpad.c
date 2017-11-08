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

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_attr.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "iot_touchpad.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)
#define TIMER_CALLBACK_MAX_WAIT_TICK    (0)
#define TOUCHPAD_FILTER_PERIOD          10

typedef struct tp_custom_cb tp_custom_cb_t;
typedef enum {
    TOUCHPAD_STATE_IDLE = 0,
    TOUCHPAD_STATE_PUSH,
    TOUCHPAD_STATE_PRESS,
} tp_status_t;

typedef struct {
    tp_cb cb;
    void *arg;
} tp_cb_t;

typedef struct {
    uint32_t last_tick;
    touch_pad_t touch_pad_num;
    uint32_t threshold;
    uint32_t filter_value;
    tp_status_t state;
    uint32_t sum_ms;
    uint32_t serial_thres_sec;
    uint32_t serial_interval_ms;
    xTimerHandle serial_tmr;
    xTimerHandle timer;
    tp_cb_t serial_cb;
    tp_cb_t* cb_group[TOUCHPAD_CB_MAX];
    tp_custom_cb_t *custom_cbs;
    uint16_t thres_percent;
    uint16_t thresh_abs;
    uint16_t adjust_percent;
} tp_dev_t;

typedef struct {
    uint32_t pos_scale;
    tp_handle_t *tp_handles;
    uint8_t tp_num;
    uint8_t slide_pos;
    uint16_t *tp_val;
} tp_slide_t;

struct tp_custom_cb{
    tp_cb cb;
    void *arg;
    TimerHandle_t tmr;
    tp_dev_t *tp_dev;
    tp_custom_cb_t *next_cb;
};

typedef struct tp_matrix_cus_cb tp_matrix_cus_cb_t;
typedef struct {
    tp_matrix_cb cb;
    void *arg;
} tp_matrix_cb_t;

typedef struct tp_matrix_arg tp_matrix_arg_t;

typedef struct {
    tp_handle_t *x_tps;
    tp_handle_t *y_tps;
    tp_matrix_arg_t *matrix_args;
    tp_matrix_cb_t* cb_group[TOUCHPAD_CB_MAX];
    tp_matrix_cus_cb_t *custom_cbs;
    tp_matrix_cb_t serial_cb;
    uint32_t serial_thres_sec;
    uint32_t serial_interval_ms;
    xTimerHandle serial_tmr;
    tp_status_t active_state;
    uint8_t active_idx;
    uint8_t x_num;
    uint8_t y_num;
} tp_matrix_t;

typedef struct tp_matrix_arg {
    tp_matrix_t *tp_matrix;
    uint8_t tp_idx;
} tp_matrix_arg_t;

struct tp_matrix_cus_cb {
    tp_matrix_cb cb;
    void *arg;
    TimerHandle_t tmr;
    tp_matrix_t *tp_matrix;
    tp_matrix_cus_cb_t *next_cb;
};

// Debug tag in esp log
static const char* TAG = "touchpad";
static bool g_init_flag = false;
/// Record time of last touch, used to eliminate jitter
#define SLIDE_POS_INF   ((1 << 8) - 1)
static tp_dev_t* tp_group[TOUCH_PAD_MAX];
static xTimerHandle g_adjust_tmr, g_init_tmr;
static xSemaphoreHandle s_tp_mux = NULL;

static void update_adjust_percent()
{
    int num = 0;
    uint32_t avg = 0;
    uint16_t val[TOUCH_PAD_MAX];
    // Read raw value of each touch sensor, get sum
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (tp_group[i] != NULL) {
            iot_tp_read(tp_group[i], &val[i]);
            // If the raw value is less than threshold, we think the touch is triggered,
            // and calculate the value with the original one, instead of the 'touched' one.
            if (val[i] < tp_group[i]->threshold) {
                if (tp_group[i]->thresh_abs > 0) {
                    val[i] = tp_group[i]->threshold + tp_group[i]->thresh_abs;
                } else {
                    val[i] = tp_group[i]->threshold * 1000 / tp_group[i]->thres_percent;
                }
            }
            avg += val[i];
            ++num;
        }
    }
    // get average value from the sum value, and update the proportion (against the average value)
    if (num != 0 && avg != 0) {
        avg /= num;
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if (tp_group[i] != NULL) {
                tp_group[i]->adjust_percent = val[i] * 1000 / avg;
            }
        }
    }
}

// callback function triggered by 'push', 'tap' and 'release' event
static void tp_slide_pos_cb(void *arg)
{
    tp_slide_t* tp_slide = (tp_slide_t*) arg;
    const uint32_t pos_scale = tp_slide->pos_scale;
    uint16_t val_sum = 0, max_idx = 0;
    uint8_t non0_cnt = 0;
    uint32_t pos = 0;
    uint16_t *tp_val = tp_slide->tp_val; 
    for (int i = 0; i < tp_slide->tp_num; i++) {
        // read filtered value
        iot_tp_read(tp_slide->tp_handles[i], &tp_val[i]);
        tp_dev_t *tp = (tp_dev_t*) tp_slide->tp_handles[i];
        uint16_t norm_val;
        if (tp->thresh_abs > 0) {
            norm_val = tp->threshold + tp->thresh_abs;
        } else {
            norm_val = tp->threshold * 1000 / tp->thres_percent;
        }
        // tp_val is the difference from normal value if read value is  less than threshold, otherwise zero.
        tp_val[i] = (tp_val[i] < tp->threshold) ? (norm_val - tp_val[i]) : 0;
        non0_cnt = tp_val[i] != 0 ? non0_cnt+1 : non0_cnt;
        if (i >= 2) {
            // find the max sum of three continuous values
            uint32_t neb_sum = tp_val[i-2] + tp_val[i-1] + tp_val[i];
            if (neb_sum > val_sum) {
                // val_sum is the max value of neb_sum
                val_sum = neb_sum;
                max_idx = i - 1;
            }
        }
    }
    if (non0_cnt == 2) {
        // if the read value of the first(or the last) sensor is not zero, meanwhile the one next to it
        // is zero, return the position of the first one(or the last one)
        if (tp_val[0] != 0 && tp_val[1] == 0) {
            tp_slide->slide_pos = 0;
            return;
        } else if (tp_val[tp_slide->tp_num-1] != 0 && tp_val[tp_slide->tp_num-2] == 0) {
            tp_slide->slide_pos = (tp_slide->tp_num-1) * pos_scale;
            return;
        }
    }
    if (val_sum == 0) {
        // if the max value of neb_sum is zero, no pad is touched
        tp_slide->slide_pos = SLIDE_POS_INF;
    } else {
        // return the corresponding position.
        pos = ((max_idx-1) * tp_val[max_idx-1] + (max_idx) * tp_val[max_idx] + (max_idx+1) * tp_val[max_idx+1]) * pos_scale;
        tp_slide->slide_pos = (uint8_t) (pos / val_sum);
    }
}

// check and run the hooked callback function
static inline void callback_exec(tp_dev_t* tp_dev, tp_cb_type_t cb_type)
{
    if (tp_dev->cb_group[cb_type] != NULL) {
        tp_cb_t* cb_info = tp_dev->cb_group[cb_type];
        cb_info->cb(cb_info->arg);
    }
}

static void tp_adjust_cb(TimerHandle_t xTimer)
{
    uint16_t val;
    uint32_t avg = 0;
    uint8_t num = 0;
    static int adj_cnt = 0;
    if (xTimer) {
        if (adj_cnt++ == 5) {
            xTimerChangePeriod(xTimer, 10000 / portTICK_PERIOD_MS, portMAX_DELAY);
            xTimerReset(xTimer, portMAX_DELAY);
        }
    }
    // get sum of filtered value of each cap-sensor.
    // If any is less than threshold, return.
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (tp_group[i] != NULL) {
            tp_dev_t* tp_dev = tp_group[i];
            touch_pad_read_filtered(tp_dev->touch_pad_num, &val);
            ESP_LOGD(TAG, "rdtp[%d]: %d\n", i, val);
            if (val < tp_dev->threshold) {
                return;
            }
            avg += val;
            ++num;
        }
    }
    // get average filtered value and adjust the average value one by one
    // then update the new threshold for each cap-sensor
    if (num != 0 && avg != 0) {
        avg /= num;
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if (tp_group[i] != NULL) {
                tp_dev_t* tp_dev = tp_group[i];
                if (tp_dev->thresh_abs > 0) {
                    avg = 0;
                    for (int j = 0; j < TOUCH_PAD_MAX; j++) {
                        uint16_t rd_val;
                        touch_pad_read_filtered(tp_dev->touch_pad_num, &rd_val);
                        if(rd_val <= tp_dev->threshold) {
                            return;
                        }
                        avg += rd_val;
                    }
                    avg /= TOUCH_PAD_MAX;
                    tp_dev->threshold = avg - tp_dev->thresh_abs;
                    ESP_LOGD(TAG, "avg tp[%d]: %d, thresh: %d\n", i, avg, tp_dev->threshold);
                } else {
                    tp_dev->threshold = (avg * tp_dev->adjust_percent / 1000) * tp_dev->thres_percent / 1000;
                }
                ESP_LOGD(TAG, "The new threshold of touchpad %d is %d", tp_dev->touch_pad_num, tp_dev->threshold);
                touch_pad_config(tp_dev->touch_pad_num, tp_dev->threshold);
            }
        }
    }
}

//run init once and delete the timer
static void tp_init_tmr_cb(TimerHandle_t xTimer)
{
    update_adjust_percent();
    tp_adjust_cb(NULL);
    xTimerDelete(g_init_tmr, portMAX_DELAY);
    g_init_tmr = NULL;
}

// check and run the hooked callback function
static void tp_serial_timer_cb(TimerHandle_t xTimer)
{
    tp_dev_t *tp_dev = (tp_dev_t*) pvTimerGetTimerID(xTimer);
    if (tp_dev->serial_cb.cb != NULL) {
        tp_dev->serial_cb.cb(tp_dev->serial_cb.arg);
    }
}

static void tp_custom_timer_cb(TimerHandle_t xTimer)
{
    tp_custom_cb_t* custom_cb = (tp_custom_cb_t*) pvTimerGetTimerID(xTimer);
    custom_cb->tp_dev->state = TOUCHPAD_STATE_PRESS;
    custom_cb->cb(custom_cb->arg);
}

// reset all the customed event timers
static inline void tp_custom_reset_cb_tmrs(tp_dev_t* tp_dev)
{
    tp_custom_cb_t *custom_cb = tp_dev->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerReset(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}


//stop all the customed event timers
static inline void tp_custom_stop_cb_tmrs(tp_dev_t* tp_dev)
{
    tp_custom_cb_t *custom_cb = tp_dev->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerStop(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}

// the timer is started in interrupt
static void tp_timer_callback(TimerHandle_t xTimer)
{
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (tp_group[i] != NULL && tp_group[i]->timer == xTimer) {
            tp_dev_t* tp_dev = tp_group[i];
            uint16_t value;
            // read filterd value
            iot_tp_read(tp_dev, &value);
            if (tp_dev->sum_ms == 0) {
                tp_dev->state = TOUCHPAD_STATE_PUSH;
                // run push event cb, reset custom event cb
                callback_exec(tp_dev, TOUCHPAD_CB_PUSH);
                tp_custom_reset_cb_tmrs(tp_dev);
            }
            // value is less than threshold, trigger the event
            if (value < tp_dev->threshold) {
                // sum_ms is the total time that the read value is under threshold, which means a touch event is on.
                tp_dev->sum_ms += tp_dev->filter_value;
                // whether this is the exact time that a serial event happens
                if (tp_dev->serial_thres_sec > 0
                        && tp_dev->sum_ms - tp_dev->filter_value < tp_dev->serial_thres_sec * 1000
                        && tp_dev->sum_ms >= tp_dev->serial_thres_sec * 1000) {
                    tp_dev->state = TOUCHPAD_STATE_PRESS;
                    tp_dev->serial_cb.cb(tp_dev->serial_cb.arg);
                    xTimerStart(tp_dev->serial_tmr, portMAX_DELAY);
                }
                xTimerReset(tp_dev->timer, portMAX_DELAY);
            } else {
                if (tp_dev->state == TOUCHPAD_STATE_PUSH) {
                    callback_exec(tp_dev, TOUCHPAD_CB_TAP);
                }
                callback_exec(tp_dev, TOUCHPAD_CB_RELEASE);
                tp_custom_stop_cb_tmrs(tp_dev);
                if (tp_dev->serial_tmr) {
                    xTimerStop(tp_dev->serial_tmr, portMAX_DELAY);
                }
                tp_dev->sum_ms = 0;
                tp_dev->state = TOUCHPAD_STATE_IDLE;
            }
        }
    }
}

static void touch_pad_isr_handler(void *arg)
{
    uint32_t pad_intr = touch_pad_get_status();
    int i = 0;
    tp_dev_t* tp_dev;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    touch_pad_clear_status();
    for (i = 0; i < TOUCH_PAD_MAX; ++i) {
        if ((pad_intr >> i) & 0x01) {
            tp_dev = tp_group[i];
            uint16_t val;
            touch_pad_read_filtered(i, &val);
            if (tp_dev != NULL && val < tp_dev->threshold) {
                TickType_t tick = xTaskGetTickCount();
                uint32_t tick_diff = tick - tp_dev->last_tick;
                tp_dev->last_tick = tick;
                if (tick_diff > tp_dev->filter_value / portTICK_PERIOD_MS) {
                    //start timer to trigger touch event
                    xTimerResetFromISR(tp_dev->timer, &xHigherPriorityTaskWoken);
                }
            }
        }
    }
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

tp_handle_t iot_tp_create(touch_pad_t touch_pad_num, uint16_t thres_percent, uint16_t thresh_abs, uint32_t filter_value)
{
    if (g_init_flag == false) {
        // global touch sensor hardware init
        s_tp_mux = xSemaphoreCreateMutex();
        IOT_CHECK(TAG, s_tp_mux != NULL, NULL);
        g_init_flag = true;
        touch_pad_init();
        touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_0V);
        touch_pad_isr_register(touch_pad_isr_handler, NULL);
        touch_pad_intr_enable();
        touch_pad_filter_start(TOUCHPAD_FILTER_PERIOD);
        // to ajust the threshold every 10 seconds
        g_adjust_tmr = xTimerCreate("adjust_tmr", 2000 / portTICK_RATE_MS, pdTRUE, (void*) 0, tp_adjust_cb);
        xTimerStart(g_adjust_tmr, portMAX_DELAY);
    }
    // to call init function in 1 second and delete this timer
    if (g_init_tmr == NULL) {
        g_init_tmr = xTimerCreate("init_tmr", 1000 / portTICK_PERIOD_MS, pdFALSE, (void*) 0, tp_init_tmr_cb);
        xTimerStart(g_init_tmr, portMAX_DELAY);
    }
    IOT_CHECK(TAG, touch_pad_num < TOUCH_PAD_MAX, NULL);
    IOT_CHECK(TAG, thres_percent <= 1000, NULL);
    IOT_CHECK(TAG, filter_value >= portTICK_PERIOD_MS, NULL);
    xSemaphoreTake(s_tp_mux, portMAX_DELAY);
    if (tp_group[touch_pad_num] != NULL) {
        ESP_LOGE(TAG, "touchpad create error! The pad has been used!");
        xSemaphoreGive(s_tp_mux);
        return NULL;
    }
    touch_pad_config(touch_pad_num, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    uint16_t tp_val;
    touch_pad_read_filtered(touch_pad_num, &tp_val);
    ESP_LOGD(TAG, "tp[%d]: %d\n", touch_pad_num, tp_val);
    tp_dev_t* tp_dev = (tp_dev_t*) calloc(1, sizeof(tp_dev_t));
    tp_dev->last_tick = 0;
    tp_dev->thres_percent = thres_percent;


    tp_dev->thresh_abs = thresh_abs;
    ESP_LOGD(TAG, "thresh abs: %d\n", thresh_abs);
    tp_dev->threshold =  (tp_dev->thresh_abs > 0 ? (tp_val - tp_dev->thresh_abs) : (tp_val * thres_percent / 1000));
    ESP_LOGD(TAG, "init thresh[%d]: %d\n", touch_pad_num, tp_dev->threshold);
    tp_dev->filter_value = filter_value;
    tp_dev->touch_pad_num = touch_pad_num;
    tp_dev->adjust_percent = 1000;
    tp_dev->sum_ms = 0;
    tp_dev->serial_thres_sec = 0;
    tp_dev->serial_interval_ms = 0;
    tp_dev->state = TOUCHPAD_STATE_IDLE;
    tp_dev->timer = xTimerCreate("tp_timer", filter_value / portTICK_RATE_MS, pdFALSE, (void*) 0, tp_timer_callback);
    touch_pad_config(touch_pad_num, tp_dev->threshold);
    tp_group[touch_pad_num] = tp_dev;
    update_adjust_percent();
    tp_adjust_cb(NULL);
    xSemaphoreGive(s_tp_mux);
    return (tp_handle_t) tp_dev;
}

esp_err_t iot_tp_delete(tp_handle_t tp_handle)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    tp_group[tp_dev->touch_pad_num] = NULL;
    for (int i = 0; i < TOUCHPAD_CB_MAX; i++) {
        if (tp_dev->cb_group[i] != NULL) {
            free(tp_dev->cb_group[i]);
            tp_dev->cb_group[i] = NULL;
        }
    }
    tp_custom_cb_t *custom_cb = tp_dev->custom_cbs;
    while (custom_cb != NULL) {
        tp_custom_cb_t *cb_next = custom_cb->next_cb;
        xTimerStop(custom_cb->tmr, portMAX_DELAY);
        xTimerDelete(custom_cb->tmr, portMAX_DELAY);
        custom_cb->tmr = NULL;
        free(custom_cb);
        custom_cb = cb_next;
    }
    tp_dev->custom_cbs = NULL;
    if (tp_dev->timer != NULL) {
        xTimerStop(tp_dev->timer, portMAX_DELAY);
        xTimerDelete(tp_dev->timer, portMAX_DELAY);
        tp_dev->timer = NULL;
    }
    if (tp_dev->serial_tmr != NULL) {
        xTimerStop(tp_dev->serial_tmr, portMAX_DELAY);
        xTimerDelete(tp_dev->serial_tmr, portMAX_DELAY);
        tp_dev->serial_tmr = NULL;
    }
    free(tp_handle);
    update_adjust_percent();
    return ESP_OK;
}

esp_err_t iot_tp_add_cb(tp_handle_t tp_handle, tp_cb_type_t cb_type, tp_cb cb, void  *arg)
{
    POINT_ASSERT(TAG, tp_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, cb_type < TOUCHPAD_CB_MAX, ESP_FAIL);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    if (tp_dev->cb_group[cb_type] != NULL) {
        ESP_LOGW(TAG, "This type of touchpad callback function has already been added!");
        return ESP_FAIL;
    }
    tp_cb_t* cb_info = (tp_cb_t*) calloc(1, sizeof(tp_cb_t));
    POINT_ASSERT(TAG, cb_info);
    cb_info->cb = cb;
    cb_info->arg = arg;
    tp_dev->cb_group[cb_type] = cb_info;
    return ESP_OK;
}

esp_err_t iot_tp_set_serial_trigger(tp_handle_t tp_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, trigger_thres_sec != 0, ESP_FAIL);
    IOT_CHECK(TAG, interval_ms > portTICK_RATE_MS, ESP_FAIL);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    if (tp_dev->serial_tmr == NULL) {
        tp_dev->serial_tmr = xTimerCreate("serial_tmr", interval_ms / portTICK_RATE_MS, pdTRUE, tp_dev, tp_serial_timer_cb);
        POINT_ASSERT(TAG, tp_dev->serial_tmr);
    } else {
        xTimerChangePeriod(tp_dev->serial_tmr, interval_ms / portTICK_RATE_MS, portMAX_DELAY);
    }
    tp_dev->serial_thres_sec = trigger_thres_sec;
    tp_dev->serial_interval_ms = interval_ms;
    tp_dev->serial_cb.cb = cb;
    tp_dev->serial_cb.arg = arg;
    return ESP_OK;
}

esp_err_t iot_tp_add_custom_cb(tp_handle_t tp_handle, uint32_t press_sec, tp_cb cb, void  *arg)
{
    POINT_ASSERT(TAG, tp_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, press_sec != 0, ESP_FAIL);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    tp_custom_cb_t* cb_new = (tp_custom_cb_t*) calloc(1, sizeof(tp_custom_cb_t));
    POINT_ASSERT(TAG, cb_new);
    cb_new->cb = cb;
    cb_new->arg = arg;
    cb_new->tp_dev = tp_dev;
    cb_new->tmr = xTimerCreate("custom_cb_tmr", press_sec * 1000 / portTICK_RATE_MS, pdFALSE, cb_new, tp_custom_timer_cb);
    if (cb_new->tmr == NULL) {
        ESP_LOGE(TAG, "timer create fail! %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);
        free(cb_new);
        return ESP_FAIL;
    }
    cb_new->next_cb = tp_dev->custom_cbs;
    tp_dev->custom_cbs = cb_new;
    return ESP_OK;
}

touch_pad_t iot_tp_num_get(const tp_handle_t tp_handle)
{
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    return tp_dev->touch_pad_num;
}

esp_err_t iot_tp_set_threshold(const tp_handle_t tp_handle, uint32_t threshold)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    ERR_ASSERT(TAG, touch_pad_config(tp_dev->touch_pad_num, threshold));
    tp_dev->threshold = threshold;
    return ESP_OK;
}

esp_err_t tp_get_threshold(const tp_handle_t tp_handle, uint32_t *threshold)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    *threshold = tp_dev->threshold;
    return ESP_OK;
}

esp_err_t iot_tp_set_filter(const tp_handle_t tp_handle, uint32_t filter_value)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    tp_dev->filter_value= filter_value;
    return ESP_OK;
}

esp_err_t iot_tp_read(const tp_handle_t tp_handle, uint16_t *touch_value_ptr)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    return touch_pad_read_filtered(tp_dev->touch_pad_num, touch_value_ptr);
}

static esp_err_t tp_read_raw(const tp_handle_t tp_handle, uint16_t *touch_value_ptr)
{
    POINT_ASSERT(TAG, tp_handle);
    tp_dev_t* tp_dev = (tp_dev_t*) tp_handle;
    return touch_pad_read(tp_dev->touch_pad_num, touch_value_ptr);
}

tp_slide_handle_t iot_tp_slide_create(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, const uint16_t *thresh_abs, uint32_t filter_value)
{
    IOT_CHECK(TAG, tps != NULL, NULL);
    IOT_CHECK(TAG, pos_scale != 0, NULL);
    tp_slide_t* tp_slide = (tp_slide_t*) calloc(1, sizeof(tp_slide_t));
    IOT_CHECK(TAG, tp_slide != NULL, NULL);
    tp_slide->tp_num = num;
    tp_slide->pos_scale = pos_scale;
    tp_slide->slide_pos = SLIDE_POS_INF;
    tp_slide->tp_handles = (tp_handle_t*) calloc(num, sizeof(tp_handle_t));
    if (tp_slide->tp_handles == NULL) {
        ESP_LOGE(TAG, "touchpad slide calloc error!");
        free(tp_slide);
        return NULL;
    }
    tp_slide->tp_val = (uint16_t*) calloc(num, sizeof(uint16_t));
    if (tp_slide->tp_val == NULL) {
        ESP_LOGE(TAG, "touchpad slide calloc error!");
        free(tp_slide->tp_handles);
        free(tp_slide);
        return NULL;
    }
    for (int i = 0; i < num; i++) {
        if (tp_group[tps[i]] != NULL) {
            tp_slide->tp_handles[i] = tp_group[tps[i]];
        } else {
            if (thresh_abs) {
                tp_slide->tp_handles[i] = iot_tp_create(tps[i], thres_percent, thresh_abs[i], filter_value);
            } else {
                tp_slide->tp_handles[i] = iot_tp_create(tps[i], thres_percent, 0, filter_value);
            }
            // in each event callback function, it will calculate the related position
            iot_tp_add_cb(tp_slide->tp_handles[i], TOUCHPAD_CB_PUSH, tp_slide_pos_cb, tp_slide);
            iot_tp_set_serial_trigger(tp_slide->tp_handles[i], 1, filter_value, tp_slide_pos_cb, tp_slide);
        }
    }
    return (tp_slide_handle_t*) tp_slide;
}

esp_err_t iot_tp_slide_delete(tp_slide_handle_t tp_slide_handle)
{
    POINT_ASSERT(TAG, tp_slide_handle);
    tp_slide_t *tp_slide = (tp_slide_t*) tp_slide_handle;
    for (int i = 0; i < tp_slide->tp_num; i++) {
        if (tp_slide->tp_handles[i]) {
            iot_tp_delete(tp_slide->tp_handles[i]);
            for (int j = i + 1; j < tp_slide->tp_num; j++) {
                if (tp_slide->tp_handles[i] == tp_slide->tp_handles[j])
                    tp_slide->tp_handles[j] = NULL;
            }
            tp_slide->tp_handles[i] = NULL;
        }
    }
    free(tp_slide->tp_handles);
    free(tp_slide->tp_val);
    free(tp_slide);
    return ESP_OK;
}

uint8_t iot_tp_slide_position(tp_slide_handle_t tp_slide_handle)
{
    IOT_CHECK(TAG, tp_slide_handle != NULL, SLIDE_POS_INF);
    tp_slide_t *tp_slide = (tp_slide_t*) tp_slide_handle;
    return tp_slide->slide_pos;
}

// reset all the cumstom timers of matrix object
static inline void matrix_reset_cb_tmrs(tp_matrix_t* tp_matrix)
{
    tp_matrix_cus_cb_t *custom_cb = tp_matrix->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerReset(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}

// stop all the cumstom timers of matrix object
static inline void matrix_stop_cb_tmrs(tp_matrix_t* tp_matrix)
{
    tp_matrix_cus_cb_t *custom_cb = tp_matrix->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerStop(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}

static void tp_matrix_push_cb(void *arg)
{
    tp_matrix_arg_t *matrix_arg = (tp_matrix_arg_t*) arg;
    tp_matrix_t *tp_matrix = matrix_arg->tp_matrix;
    // this is the 'x' index of pad(only 'x' pads have event callback functions)
    int x_idx = matrix_arg->tp_idx;
    tp_dev_t *tp_dev;
    int idx = -1;
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        return;
    }
    uint16_t val;
    for (int j = 0; j < tp_matrix->y_num; j++) {
        tp_read_raw(tp_matrix->y_tps[j], &val);
        tp_dev = (tp_dev_t*) tp_matrix->y_tps[j];
        ESP_LOGD(TAG, "y[%d] tp[%d] rd: %d; thresh: %d\n", j, tp_dev->touch_pad_num, val, tp_dev->threshold);
        tp_dev_t *tp_x = tp_matrix->x_tps[x_idx];
        uint16_t x_val;
        iot_tp_read(tp_x, &x_val);
        ESP_LOGD(TAG, "x[%d] tp[%d] rd: %d; thresh: %d\n", x_idx, tp_x->touch_pad_num, x_val, tp_x->threshold);
        if (val <= tp_dev->threshold) {
            if (idx < 0) {
                // this is the 'y' index
                idx = x_idx * tp_matrix->y_num + j;
                ESP_LOGD(TAG, "matrix idx: %d\n", idx);
            } else {
                // to make sure only one sensor is touched.
                return;
            }
        }
    }
    // find only one active pad
    if (idx >= 0) {
        tp_matrix->active_state = TOUCHPAD_STATE_PUSH;
        // this is the active 'y' index
        tp_matrix->active_idx = idx;
        if (tp_matrix->cb_group[TOUCHPAD_CB_PUSH] != NULL) {
            tp_matrix_cb_t* cb_info = tp_matrix->cb_group[TOUCHPAD_CB_PUSH];
            cb_info->cb(cb_info->arg, x_idx, idx % tp_matrix->y_num);
        }
        matrix_reset_cb_tmrs(tp_matrix);
        if (tp_matrix->serial_tmr != NULL) {
            xTimerChangePeriod(tp_matrix->serial_tmr, tp_matrix->serial_thres_sec*1000 / portTICK_RATE_MS, portMAX_DELAY);
        }
    }
}

static void tp_matrix_release_cb(void *arg)
{
    tp_matrix_arg_t *matrix_arg = (tp_matrix_arg_t*) arg;
    tp_matrix_t *tp_matrix = matrix_arg->tp_matrix;
    // tp_idx is the 'x' index of pad(only 'x' pads have event callback functions)
    // active_idx = x_idx * tp_matrix->y_num + j;
    if (matrix_arg->tp_idx != tp_matrix->active_idx / tp_matrix->y_num) {
        return;
    }
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        tp_matrix->active_state = TOUCHPAD_STATE_IDLE;
        uint8_t idx = tp_matrix->active_idx;
        if (tp_matrix->cb_group[TOUCHPAD_CB_RELEASE] != NULL) {
            tp_matrix_cb_t* cb_info = tp_matrix->cb_group[TOUCHPAD_CB_RELEASE];
            cb_info->cb(cb_info->arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
        }
        matrix_stop_cb_tmrs(tp_matrix);
        if (tp_matrix->serial_tmr != NULL) {
            xTimerStop(tp_matrix->serial_tmr, portMAX_DELAY);
        }
    }
}

static void tp_matrix_tap_cb(void *arg)
{
    tp_matrix_arg_t *matrix_arg = (tp_matrix_arg_t*) arg;
    tp_matrix_t *tp_matrix = matrix_arg->tp_matrix;
    // make sure this is the same active sensor
    if (matrix_arg->tp_idx != tp_matrix->active_idx / tp_matrix->y_num) {
        return;
    }
    if (tp_matrix->active_state == TOUCHPAD_STATE_PUSH) {
        uint8_t idx = tp_matrix->active_idx;
        if (tp_matrix->cb_group[TOUCHPAD_CB_TAP] != NULL) {
            tp_matrix_cb_t* cb_info = tp_matrix->cb_group[TOUCHPAD_CB_TAP];
            cb_info->cb(cb_info->arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
        }
    }
}

static void tp_matrix_cus_tmr_cb(TimerHandle_t xTimer)
{
    tp_matrix_cus_cb_t *tp_matrix_cb = (tp_matrix_cus_cb_t*) pvTimerGetTimerID(xTimer);
    tp_matrix_t *tp_matrix = tp_matrix_cb->tp_matrix;
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        tp_matrix->active_state = TOUCHPAD_STATE_PRESS;
        uint8_t idx = tp_matrix->active_idx;
        tp_matrix_cb->cb(tp_matrix_cb->arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
    }
}

static void tp_matrix_serial_trigger_cb(TimerHandle_t xTimer)
{
    tp_matrix_t *tp_matrix = (tp_matrix_t*) pvTimerGetTimerID(xTimer);
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        tp_matrix->active_state = TOUCHPAD_STATE_PRESS;
        uint8_t idx = tp_matrix->active_idx;
        tp_matrix->serial_cb.cb(tp_matrix->serial_cb.arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
        xTimerChangePeriod(tp_matrix->serial_tmr, tp_matrix->serial_interval_ms / portTICK_RATE_MS, portMAX_DELAY);
    }
}

tp_matrix_handle_t iot_tp_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, const uint16_t *thresh_abs, uint32_t filter_value)
{
    IOT_CHECK(TAG, x_num != 0 && x_num < TOUCH_PAD_MAX, NULL);
    IOT_CHECK(TAG, y_num != 0 && y_num < TOUCH_PAD_MAX, NULL);
    tp_matrix_t *tp_matrix = (tp_matrix_t*) calloc(1, sizeof(tp_matrix_t));
    IOT_CHECK(TAG, tp_matrix != NULL, NULL);
    tp_matrix->x_tps = (tp_handle_t*) calloc(x_num, sizeof(tp_handle_t));
    if (tp_matrix->x_tps == NULL) {
        ESP_LOGE(TAG, "create touchpad matrix error! no available memory!");
        free(tp_matrix);
        return NULL;
    }
    tp_matrix->y_tps = (tp_handle_t*) calloc(y_num, sizeof(tp_handle_t));
    if (tp_matrix->y_tps == NULL) {
        ESP_LOGE(TAG, "create touchpad matrix error! no available memory!");
        free(tp_matrix->x_tps);
        free(tp_matrix);
        return NULL;
    }
    tp_matrix->matrix_args = (tp_matrix_arg_t*) calloc(x_num, sizeof(tp_matrix_arg_t));
    if (tp_matrix->matrix_args == NULL) {
        ESP_LOGE(TAG, "create touchpad matrix error! no available memory!");
        free(tp_matrix->x_tps);
        free(tp_matrix->y_tps);
        free(tp_matrix);
        return NULL;
    }
    tp_matrix->x_num = x_num;
    tp_matrix->y_num = y_num;
    for (int i = 0; i < x_num; i++) {
        if (thresh_abs) {
            tp_matrix->x_tps[i] = iot_tp_create(x_tps[i], thres_percent, thresh_abs[i], filter_value);
        } else {
            tp_matrix->x_tps[i] = iot_tp_create(x_tps[i], thres_percent, 0, filter_value);
        }
        if (tp_matrix->x_tps[i] == NULL) {
            goto CREATE_ERR;
        }
        tp_matrix->matrix_args[i].tp_matrix = tp_matrix;
        tp_matrix->matrix_args[i].tp_idx = i;
        // only add callback event to 'x' pads
        iot_tp_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_PUSH, tp_matrix_push_cb, tp_matrix->matrix_args + i);
        iot_tp_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_RELEASE, tp_matrix_release_cb, tp_matrix->matrix_args + i);
        iot_tp_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_TAP, tp_matrix_tap_cb, tp_matrix->matrix_args + i);
    }
    for (int i = 0; i < y_num; i++) {
        if (thresh_abs) {
            tp_matrix->y_tps[i] = iot_tp_create(y_tps[i], thres_percent, thresh_abs[x_num + i], filter_value);
        } else {
            tp_matrix->y_tps[i] = iot_tp_create(y_tps[i], thres_percent, 0, filter_value);
        }
        if (tp_matrix->y_tps[i] == NULL) {
            goto CREATE_ERR;
        }
    }
    tp_matrix->active_state = TOUCHPAD_STATE_IDLE;
    return (tp_matrix_handle_t)tp_matrix;

CREATE_ERR:
    ESP_LOGE(TAG, "touchpad matrix creaete error!");
    for (int i = 0; i < x_num; i++) {
        if (tp_matrix->x_tps[i] != NULL) {
            iot_tp_delete(tp_matrix->x_tps[i]);
            tp_matrix->x_tps[i] = NULL;
        }
    }
    for (int i = 0; i < y_num; i++) {
        if (tp_matrix->y_tps[i] != NULL) {
            iot_tp_delete(tp_matrix->y_tps[i]);
            tp_matrix->y_tps[i] = NULL;
        }
    }
    free(tp_matrix->x_tps);
    free(tp_matrix->y_tps);
    free(tp_matrix->matrix_args);
    free(tp_matrix);
    tp_matrix = NULL;
    return NULL;
}

esp_err_t iot_tp_matrix_delete(tp_matrix_handle_t tp_matrix_hd)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    tp_matrix_t *tp_matrix = (tp_matrix_t*) tp_matrix_hd;
    for (int i = 0; i < tp_matrix->x_num; i++) {
        if (tp_matrix->x_tps[i] != NULL) {
            iot_tp_delete(tp_matrix->x_tps[i]);
            tp_matrix->x_tps[i] = NULL;
        }
    }
    for (int i = 0; i < tp_matrix->y_num; i++) {
        if (tp_matrix->y_tps[i] != NULL) {
            iot_tp_delete(tp_matrix->y_tps[i]);
            tp_matrix->y_tps[i] = NULL;
        }
    }
    for (int i = 0; i < TOUCHPAD_CB_MAX; i++) {
        if (tp_matrix->cb_group[i] != NULL) {
            free(tp_matrix->cb_group[i]);
            tp_matrix->cb_group[i] = NULL;
        }
    }
    tp_matrix_cus_cb_t *custom_cb = tp_matrix->custom_cbs;
    while (custom_cb != NULL) {
        tp_matrix_cus_cb_t *cb_next = custom_cb->next_cb;
        xTimerStop(custom_cb->tmr, portMAX_DELAY);
        xTimerDelete(custom_cb->tmr, portMAX_DELAY);
        custom_cb->tmr = NULL;
        free(custom_cb);
        custom_cb = cb_next;
    }
    if (tp_matrix->serial_tmr != NULL) {
        xTimerStop(tp_matrix->serial_tmr, portMAX_DELAY);
        xTimerDelete(tp_matrix->serial_tmr, portMAX_DELAY);
        tp_matrix->serial_tmr = NULL;
    }
    tp_matrix->custom_cbs = NULL;
    free(tp_matrix->x_tps);
    free(tp_matrix->y_tps);
    free(tp_matrix->matrix_args);
    free(tp_matrix);
    return ESP_OK;
}

esp_err_t iot_tp_matrix_add_cb(tp_matrix_handle_t tp_matrix_hd, tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, cb_type < TOUCHPAD_CB_MAX, ESP_FAIL);
    tp_matrix_t *tp_matrix = (tp_matrix_t*) tp_matrix_hd;
    if (tp_matrix->cb_group[cb_type] != NULL) {
        ESP_LOGW(TAG, "This type of touchpad callback function has already been added!");
        return ESP_FAIL;
    }
    tp_matrix_cb_t* cb_info = (tp_matrix_cb_t*) calloc(1, sizeof(tp_matrix_cb_t));
    POINT_ASSERT(TAG, cb_info);
    cb_info->cb = cb;
    cb_info->arg = arg;
    tp_matrix->cb_group[cb_type] = cb_info;
    return ESP_OK;
}

esp_err_t iot_tp_matrix_add_custom_cb(tp_matrix_handle_t tp_matrix_hd, uint32_t press_sec, tp_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, press_sec != 0, ESP_FAIL);
    tp_matrix_t *tp_matrix = (tp_matrix_t*) tp_matrix_hd;
    tp_matrix_cus_cb_t *cb_new = (tp_matrix_cus_cb_t*) calloc(1, sizeof(tp_matrix_cus_cb_t));
    POINT_ASSERT(TAG, cb_new);
    cb_new->tmr = xTimerCreate("custom_tmr", press_sec * 1000 / portTICK_RATE_MS, pdFALSE, cb_new, tp_matrix_cus_tmr_cb);
    if (cb_new->tmr == NULL) {
        ESP_LOGE(TAG, "timer create fail! %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);
        free(cb_new);
        return ESP_FAIL;
    }
    cb_new->cb = cb;
    cb_new->arg = arg;
    cb_new->tp_matrix = tp_matrix;
    cb_new->next_cb = tp_matrix->custom_cbs;
    tp_matrix->custom_cbs = cb_new;
    return ESP_OK;
}

esp_err_t iot_tp_matrix_set_serial_trigger(tp_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, trigger_thres_sec != 0, ESP_FAIL);
    IOT_CHECK(TAG, interval_ms >= portTICK_PERIOD_MS, ESP_FAIL);
    tp_matrix_t *tp_matrix = (tp_matrix_t*) tp_matrix_hd;
    tp_matrix->serial_cb.cb = cb;
    tp_matrix->serial_cb.arg = arg;
    tp_matrix->serial_thres_sec = trigger_thres_sec;
    tp_matrix->serial_interval_ms = interval_ms;
    tp_matrix->serial_tmr = xTimerCreate("serial_tmr", trigger_thres_sec*1000 / portTICK_RATE_MS, pdFALSE, tp_matrix, tp_matrix_serial_trigger_cb);
    POINT_ASSERT(TAG, tp_matrix->serial_tmr);
    return ESP_OK;
}

