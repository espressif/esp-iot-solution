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
#include "touchpad.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)
#define TIMER_CALLBACK_MAX_WAIT_TICK    (0)
#define TOUCHPAD_FILTER_PERIOD          10

typedef struct tp_custom_cb touchpad_custom_cb_t;
typedef enum {
    TOUCHPAD_STATE_IDLE = 0,
    TOUCHPAD_STATE_PUSH,
    TOUCHPAD_STATE_PRESS,
} touchpad_status_t;

typedef struct {
    touchpad_cb cb;
    void *arg;
} touchpad_cb_t;

typedef struct {
    uint32_t last_tick;
    touch_pad_t touch_pad_num;
    uint32_t threshold;
    uint32_t filter_value;
    touchpad_status_t state;
    uint32_t sum_ms;
    uint32_t serial_thres_sec;
    uint32_t serial_interval_ms;
    xTimerHandle serial_tmr;
    xTimerHandle timer;
    touchpad_cb_t serial_cb;
    touchpad_cb_t* cb_group[TOUCHPAD_CB_MAX];
    touchpad_custom_cb_t *custom_cbs;
    uint16_t thres_percent;
    uint16_t adjust_percent;
} touchpad_dev_t;

typedef struct {
    uint32_t pos_scale;
    touchpad_handle_t *tp_handles;
    uint8_t tp_num;
    uint8_t slide_pos;
    uint16_t *tp_val;
} touchpad_slide_t;

struct tp_custom_cb{
    touchpad_cb cb;
    void *arg;
    TimerHandle_t tmr;
    touchpad_dev_t *tp_dev;
    touchpad_custom_cb_t *next_cb;
};

typedef struct tp_matrix_cus_cb tp_matrix_cus_cb_t;
typedef struct {
    touchpad_matrix_cb cb;
    void *arg;
} tp_matrix_cb_t;

typedef struct tp_matrix_arg tp_matrix_arg_t;

typedef struct {
    touchpad_handle_t *x_tps;
    touchpad_handle_t *y_tps;
    tp_matrix_arg_t *matrix_args;
    tp_matrix_cb_t* cb_group[TOUCHPAD_CB_MAX];
    tp_matrix_cus_cb_t *custom_cbs;
    tp_matrix_cb_t serial_cb;
    uint32_t serial_thres_sec;
    uint32_t serial_interval_ms;
    xTimerHandle serial_tmr;
    touchpad_status_t active_state;
    uint8_t active_idx;
    uint8_t x_num;
    uint8_t y_num;
} touchpad_matrix_t;

typedef struct tp_matrix_arg {
    touchpad_matrix_t *tp_matrix;
    uint8_t tp_idx;
} tp_matrix_arg_t;

struct tp_matrix_cus_cb {
    touchpad_matrix_cb cb;
    void *arg;
    TimerHandle_t tmr;
    touchpad_matrix_t *tp_matrix;
    tp_matrix_cus_cb_t *next_cb;
};

#define PROXIMITY_HISTORY_NUM   4
typedef struct {
    touchpad_handle_t tp_hd;
    proximity_cb cb;
    void *arg;
    proximity_status_t state;
    TimerHandle_t sta_tmr;
    uint32_t head_idx;
    uint16_t his_val[PROXIMITY_HISTORY_NUM];
} proximity_sensor_t;

typedef enum {
    GESTURE_SENSOR_TOP = 0,
    GESTURE_SENSOR_BOTTOM,
    GESTURE_SENSOR_LEFT,
    GESTURE_SENSOR_RIGHT,
    GESTURE_SENSOR_MAX,
} gesture_num_t;

typedef struct gesture_sensor gesture_sensor_t;

typedef struct gesture_cb_arg{
    gesture_sensor_t *gest_sen;
    gesture_num_t idx;
} gesture_cb_arg_t;

typedef enum {
    GESTURE_STATE_IDLE = 0,
    GESTURE_STATE_START,
    GESTURE_STATE_FIRST,
    GESTURE_STATE_SECOND,
    GESTURE_STATE_END,
} gesture_state_t;

struct gesture_sensor{
    proximity_sensor_handle_t prox_hds[GESTURE_SENSOR_MAX];
    proximity_status_t prox_stas[GESTURE_SENSOR_MAX];
    gesture_cb_arg_t cb_args[GESTURE_SENSOR_MAX];
    gesture_state_t gest_stas[GESTURE_TYPE_MAX];
    gesture_cb cb;
    void *arg;
};



// Debug tag in esp log
static const char* TAG = "touchpad";
static bool g_init_flag = false;
/// Record time of last touch, used to eliminate jitter
#define SLIDE_POS_INF   ((1 << 8) - 1)
static touchpad_dev_t* touchpad_group[TOUCH_PAD_MAX];
static xTimerHandle g_adjust_tmr, g_init_tmr;
static xSemaphoreHandle s_tp_mux = NULL;

static void update_adjust_percent()
{
    int num = 0;
    uint32_t avg = 0;
    uint16_t val[TOUCH_PAD_MAX];
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (touchpad_group[i] != NULL) {
            touchpad_read(touchpad_group[i], &val[i]);
            if (val[i] < touchpad_group[i]->threshold) {
                val[i] = touchpad_group[i]->threshold * 1000 / touchpad_group[i]->thres_percent;
            }
            avg += val[i];
            ++num;
        }
    }
    if (num != 0 && avg != 0) {
        avg /= num;
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if (touchpad_group[i] != NULL) {
                touchpad_group[i]->adjust_percent = val[i] * 1000 / avg;
            }
        }
    }
}

static void tp_slide_pos_cb(void *arg)
{
    touchpad_slide_t* tp_slide = (touchpad_slide_t*) arg;
    const uint32_t pos_scale = tp_slide->pos_scale;
    uint16_t val_sum = 0, max_idx = 0;
    uint8_t non0_cnt = 0;
    uint32_t pos = 0;
    uint16_t *tp_val = tp_slide->tp_val; 
    for (int i = 0; i < tp_slide->tp_num; i++) {
        touchpad_read(tp_slide->tp_handles[i], &tp_val[i]);
        touchpad_dev_t *tp = (touchpad_dev_t*) tp_slide->tp_handles[i];
        uint16_t norm_val = tp->threshold * 1000 / tp->thres_percent;
        tp_val[i] = (tp_val[i] < tp->threshold) ? (norm_val - tp_val[i]) : 0;
        non0_cnt = tp_val[i] != 0 ? non0_cnt+1 : non0_cnt;
        if (i >= 2) {
            uint32_t neb_sum = tp_val[i-2] + tp_val[i-1] + tp_val[i];
            if (neb_sum > val_sum) {
                val_sum = neb_sum;
                max_idx = i - 1;
            }
        }
    }
    if (non0_cnt == 2) {
        if (tp_val[0] != 0 && tp_val[1] == 0) {
            tp_slide->slide_pos = 0;
            return;
        } else if (tp_val[tp_slide->tp_num-1] != 0 && tp_val[tp_slide->tp_num-2] == 0) {
            tp_slide->slide_pos = (tp_slide->tp_num-1) * pos_scale;
            return;
        }
    }
    if (val_sum == 0) {
        tp_slide->slide_pos = SLIDE_POS_INF;
    } else {
        pos = ((max_idx-1) * tp_val[max_idx-1] + (max_idx) * tp_val[max_idx] + (max_idx+1) * tp_val[max_idx+1]) * pos_scale;
        tp_slide->slide_pos = (uint8_t) (pos / val_sum);
    }
}

static inline void callback_exec(touchpad_dev_t* touchpad_dev, touchpad_cb_type_t cb_type)
{
    if (touchpad_dev->cb_group[cb_type] != NULL) {
        touchpad_cb_t* cb_info = touchpad_dev->cb_group[cb_type];
        cb_info->cb(cb_info->arg);
    }
}

static void tp_adjust_cb(TimerHandle_t xTimer)
{
    uint16_t val;
    uint32_t avg = 0;
    uint8_t num = 0;
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (touchpad_group[i] != NULL) {
            touchpad_dev_t* touchpad_dev = touchpad_group[i];
            touch_pad_read_filtered(touchpad_dev->touch_pad_num, &val);
            if (val < touchpad_dev->threshold) {
                return;
            }
            avg += val;
            ++num;
        }
    }
    if (num != 0 && avg != 0) {
        avg /= num;
        for (int i = 0; i < TOUCH_PAD_MAX; i++) {
            if (touchpad_group[i] != NULL) {
                touchpad_dev_t* tp_dev = touchpad_group[i];
                tp_dev->threshold = (tp_dev->adjust_percent * avg / 1000) * tp_dev->thres_percent / 1000;
                ESP_LOGI(TAG, "The new threshold of touchpad %d is %d", tp_dev->touch_pad_num, tp_dev->threshold);
                touch_pad_config(tp_dev->touch_pad_num, tp_dev->threshold);
            }
        }
    }
}

static void tp_init_tmr_cb(TimerHandle_t xTimer)
{
    update_adjust_percent();
    tp_adjust_cb(NULL);
    xTimerDelete(g_init_tmr, portMAX_DELAY);
    g_init_tmr = NULL;
}

static void tp_serial_timer_cb(TimerHandle_t xTimer)
{
    touchpad_dev_t *tp_dev = (touchpad_dev_t*) pvTimerGetTimerID(xTimer);
    if (tp_dev->serial_cb.cb != NULL) {
        tp_dev->serial_cb.cb(tp_dev->serial_cb.arg);
    }
}

static void tp_custom_timer_cb(TimerHandle_t xTimer)
{
    touchpad_custom_cb_t* custom_cb = (touchpad_custom_cb_t*) pvTimerGetTimerID(xTimer);
    custom_cb->tp_dev->state = TOUCHPAD_STATE_PRESS;
    custom_cb->cb(custom_cb->arg);
}

static inline void reset_cb_tmrs(touchpad_dev_t* touchpad_dev)
{
    touchpad_custom_cb_t *custom_cb = touchpad_dev->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerReset(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}

static inline void stop_cb_tmrs(touchpad_dev_t* touchpad_dev)
{
    touchpad_custom_cb_t *custom_cb = touchpad_dev->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerStop(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}

static void tp_timer_callback(TimerHandle_t xTimer)
{
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if (touchpad_group[i] != NULL && touchpad_group[i]->timer == xTimer) {
            touchpad_dev_t* touchpad_dev = touchpad_group[i];
            uint16_t value;
            touchpad_read(touchpad_dev, &value);
            if (touchpad_dev->sum_ms == 0) {
                touchpad_dev->state = TOUCHPAD_STATE_PUSH;
                callback_exec(touchpad_dev, TOUCHPAD_CB_PUSH);
                reset_cb_tmrs(touchpad_dev);
            }
            if (value < touchpad_dev->threshold) {
                touchpad_dev->sum_ms += touchpad_dev->filter_value;
                if (touchpad_dev->serial_thres_sec > 0  
                        && touchpad_dev->sum_ms - touchpad_dev->filter_value < touchpad_dev->serial_thres_sec * 1000 
                        && touchpad_dev->sum_ms >= touchpad_dev->serial_thres_sec * 1000) {
                    touchpad_dev->state = TOUCHPAD_STATE_PRESS;
                    touchpad_dev->serial_cb.cb(touchpad_dev->serial_cb.arg);
                    xTimerStart(touchpad_dev->serial_tmr, portMAX_DELAY);
                }
                xTimerReset(touchpad_dev->timer, portMAX_DELAY);
            }
            else {
                if (touchpad_dev->state == TOUCHPAD_STATE_PUSH) {
                    callback_exec(touchpad_dev, TOUCHPAD_CB_TAP);
                }
                callback_exec(touchpad_dev, TOUCHPAD_CB_RELEASE);
                stop_cb_tmrs(touchpad_dev);
                if (touchpad_dev->serial_tmr) {
                    xTimerStop(touchpad_dev->serial_tmr, portMAX_DELAY);
                }
                touchpad_dev->sum_ms = 0;
                touchpad_dev->state = TOUCHPAD_STATE_IDLE;
            }
        }
    }
        
}

static void touch_pad_handler(void *arg)
{
    uint32_t pad_intr = READ_PERI_REG(SENS_SAR_TOUCH_CTRL2_REG) & 0x3ff;
    int i = 0;
    touchpad_dev_t* touchpad_dev;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
    SET_PERI_REG_MASK(SENS_SAR_TOUCH_CTRL2_REG, SENS_TOUCH_MEAS_EN_CLR);
    if (rtc_intr & RTC_CNTL_TOUCH_INT_ST) {
        for (i = 0; i < TOUCH_PAD_MAX; ++i) {
            if ((pad_intr >> i) & 0x01) {
                touchpad_dev = touchpad_group[i];
                uint16_t val;
                touch_pad_read_filtered(i, &val);
                if (touchpad_dev != NULL && val < touchpad_dev->threshold) {
                    TickType_t tick = xTaskGetTickCount();
                    uint32_t tick_diff = tick - touchpad_dev->last_tick;
                    touchpad_dev->last_tick = tick;
                    if (tick_diff > touchpad_dev->filter_value / portTICK_PERIOD_MS) {
                        xTimerResetFromISR(touchpad_dev->timer, &xHigherPriorityTaskWoken);
                    }
                }
            }
        }
    }
}

touchpad_handle_t touchpad_create(touch_pad_t touch_pad_num, uint16_t thres_percent, uint32_t filter_value)
{
    if (g_init_flag == false) {
        s_tp_mux = xSemaphoreCreateMutex();
        IOT_CHECK(TAG, s_tp_mux != NULL, NULL);
        g_init_flag = true;
        touch_pad_init();
        touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5);
        touch_pad_isr_register(touch_pad_handler, NULL);
        touch_pad_intr_enable();
        touch_pad_filter_start(TOUCHPAD_FILTER_PERIOD);
        g_adjust_tmr = xTimerCreate("adjust_tmr", 10000 / portTICK_RATE_MS, pdTRUE, (void*) 0, tp_adjust_cb);
        xTimerStart(g_adjust_tmr, portMAX_DELAY);
        g_init_tmr = xTimerCreate("init_tmr", 1000 / portTICK_PERIOD_MS, pdFALSE, (void*) 0, tp_init_tmr_cb);
        xTimerStart(g_init_tmr, portMAX_DELAY);
    }
    IOT_CHECK(TAG, touch_pad_num < TOUCH_PAD_MAX, NULL);
    IOT_CHECK(TAG, thres_percent <= 1000, NULL);
    IOT_CHECK(TAG, filter_value >= portTICK_PERIOD_MS, NULL);
    xSemaphoreTake(s_tp_mux, portMAX_DELAY);
    if (touchpad_group[touch_pad_num] != NULL) {
        ESP_LOGE(TAG, "touchpad create error! The pad has been used!");
        xSemaphoreGive(s_tp_mux);
        return NULL;
    }
    uint16_t tp_val;
    touch_pad_read_filtered(touch_pad_num, &tp_val);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) calloc(1, sizeof(touchpad_dev_t));
    touchpad_dev->last_tick = 0;
    touchpad_dev->thres_percent = thres_percent;
    touchpad_dev->threshold = tp_val * thres_percent / 1000;
    touchpad_dev->filter_value = filter_value;
    touchpad_dev->touch_pad_num = touch_pad_num;
    touchpad_dev->adjust_percent = 1000;
    touchpad_dev->sum_ms = 0;
    touchpad_dev->serial_thres_sec = 0;
    touchpad_dev->serial_interval_ms = 0;
    touchpad_dev->state = TOUCHPAD_STATE_IDLE;
    touchpad_dev->timer = xTimerCreate("tp_timer", filter_value / portTICK_RATE_MS, pdFALSE, (void*) 0, tp_timer_callback);
    touch_pad_config(touch_pad_num, touchpad_dev->threshold);
    touchpad_group[touch_pad_num] = touchpad_dev;
    update_adjust_percent();
    tp_adjust_cb(NULL);
    xSemaphoreGive(s_tp_mux);
    return (touchpad_handle_t) touchpad_dev;
}

esp_err_t touchpad_delete(touchpad_handle_t touchpad_handle)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    touchpad_group[touchpad_dev->touch_pad_num] = NULL;
    for (int i = 0; i < TOUCHPAD_CB_MAX; i++) {
        if (touchpad_dev->cb_group[i] != NULL) {
            free(touchpad_dev->cb_group[i]);
            touchpad_dev->cb_group[i] = NULL;
        }
    }
    touchpad_custom_cb_t *custom_cb = touchpad_dev->custom_cbs;
    while (custom_cb != NULL) {
        touchpad_custom_cb_t *cb_next = custom_cb->next_cb;
        xTimerStop(custom_cb->tmr, portMAX_DELAY);
        xTimerDelete(custom_cb->tmr, portMAX_DELAY);
        custom_cb->tmr = NULL;
        free(custom_cb);
        custom_cb = cb_next;
    }
    touchpad_dev->custom_cbs = NULL;
    if (touchpad_dev->timer != NULL) {
        xTimerStop(touchpad_dev->timer, portMAX_DELAY);
        xTimerDelete(touchpad_dev->timer, portMAX_DELAY);
        touchpad_dev->timer = NULL;
    }
    if (touchpad_dev->serial_tmr != NULL) {
        xTimerStop(touchpad_dev->serial_tmr, portMAX_DELAY);
        xTimerDelete(touchpad_dev->serial_tmr, portMAX_DELAY);
        touchpad_dev->serial_tmr = NULL;
    }
    free(touchpad_handle);
    update_adjust_percent();
    return ESP_OK;
}

esp_err_t touchpad_add_cb(touchpad_handle_t touchpad_handle, touchpad_cb_type_t cb_type, touchpad_cb cb, void  *arg)
{
    POINT_ASSERT(TAG, touchpad_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, cb_type < TOUCHPAD_CB_MAX, ESP_FAIL);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    if (touchpad_dev->cb_group[cb_type] != NULL) {
        ESP_LOGW(TAG, "This type of touchpad callback function has already been added!");
        return ESP_FAIL;
    }
    touchpad_cb_t* cb_info = (touchpad_cb_t*) calloc(1, sizeof(touchpad_cb_t));
    POINT_ASSERT(TAG, cb_info);
    cb_info->cb = cb;
    cb_info->arg = arg;
    touchpad_dev->cb_group[cb_type] = cb_info;
    return ESP_OK;
}

esp_err_t touchpad_set_serial_trigger(touchpad_handle_t touchpad_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_cb cb, void *arg)
{
    POINT_ASSERT(TAG, touchpad_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, trigger_thres_sec != 0, ESP_FAIL);
    IOT_CHECK(TAG, interval_ms > portTICK_RATE_MS, ESP_FAIL);
    touchpad_dev_t* tp_dev = (touchpad_dev_t*) touchpad_handle;
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

esp_err_t touchpad_add_custom_cb(touchpad_handle_t touchpad_handle, uint32_t press_sec, touchpad_cb cb, void  *arg)
{
    POINT_ASSERT(TAG, touchpad_handle);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, press_sec != 0, ESP_FAIL);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    touchpad_custom_cb_t* cb_new = (touchpad_custom_cb_t*) calloc(1, sizeof(touchpad_custom_cb_t));
    POINT_ASSERT(TAG, cb_new);
    cb_new->cb = cb;
    cb_new->arg = arg;
    cb_new->tp_dev = touchpad_dev;
    cb_new->tmr = xTimerCreate("custom_cb_tmr", press_sec * 1000 / portTICK_RATE_MS, pdFALSE, cb_new, tp_custom_timer_cb);
    if (cb_new->tmr == NULL) {
        ESP_LOGE(TAG, "timer create fail! %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);
        free(cb_new);
        return ESP_FAIL;
    }
    cb_new->next_cb = touchpad_dev->custom_cbs;
    touchpad_dev->custom_cbs = cb_new;
    return ESP_OK;
}

touch_pad_t touchpad_num_get(const touchpad_handle_t touchpad_handle)
{
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    return touchpad_dev->touch_pad_num;
}

esp_err_t touchpad_set_threshold(const touchpad_handle_t touchpad_handle, uint32_t threshold)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    ERR_ASSERT(TAG, touch_pad_config(touchpad_dev->touch_pad_num, threshold));
    touchpad_dev->threshold = threshold;
    return ESP_OK;
}

esp_err_t touchpad_get_threshold(const touchpad_handle_t touchpad_handle, uint32_t *threshold)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    *threshold = touchpad_dev->threshold;
    return ESP_OK;
}

esp_err_t touchpad_set_filter(const touchpad_handle_t touchpad_handle, uint32_t filter_value)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    touchpad_dev->filter_value= filter_value;
    return ESP_OK;
}

esp_err_t touchpad_read(const touchpad_handle_t touchpad_handle, uint16_t *touch_value_ptr)
{
    POINT_ASSERT(TAG, touchpad_handle);
    touchpad_dev_t* touchpad_dev = (touchpad_dev_t*) touchpad_handle;
    return touch_pad_read_filtered(touchpad_dev->touch_pad_num, touch_value_ptr);
}

touchpad_slide_handle_t touchpad_slide_create(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, uint32_t filter_value)
{
    IOT_CHECK(TAG, tps != NULL, NULL);
    IOT_CHECK(TAG, pos_scale != 0, NULL);
    touchpad_slide_t* tp_slide = (touchpad_slide_t*) calloc(1, sizeof(touchpad_slide_t));
    IOT_CHECK(TAG, tp_slide != NULL, NULL);
    tp_slide->tp_num = num;
    tp_slide->pos_scale = pos_scale;
    tp_slide->slide_pos = SLIDE_POS_INF;
    tp_slide->tp_handles = (touchpad_handle_t*) calloc(num, sizeof(touchpad_handle_t));
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
        if (touchpad_group[tps[i]] != NULL) {
            tp_slide->tp_handles[i] = touchpad_group[tps[i]];
        } else {
            tp_slide->tp_handles[i] = touchpad_create(tps[i], thres_percent, filter_value);
            touchpad_add_cb(tp_slide->tp_handles[i], TOUCHPAD_CB_PUSH, tp_slide_pos_cb, tp_slide);
            touchpad_add_cb(tp_slide->tp_handles[i], TOUCHPAD_CB_RELEASE, tp_slide_pos_cb, tp_slide);
            touchpad_set_serial_trigger(tp_slide->tp_handles[i], 1, filter_value, tp_slide_pos_cb, tp_slide);
        }
    }
    return (touchpad_slide_handle_t*) tp_slide;
}

esp_err_t touchpad_slide_delete(touchpad_slide_handle_t tp_slide_handle)
{
    POINT_ASSERT(TAG, tp_slide_handle);
    touchpad_slide_t *tp_slide = (touchpad_slide_t*) tp_slide_handle;
    for (int i = 0; i < tp_slide->tp_num; i++) {
        if (tp_slide->tp_handles[i]) {
            touchpad_delete(tp_slide->tp_handles[i]);
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

uint8_t touchpad_slide_position(touchpad_slide_handle_t tp_slide_handle)
{
    IOT_CHECK(TAG, tp_slide_handle != NULL, SLIDE_POS_INF);
    touchpad_slide_t *tp_slide = (touchpad_slide_t*) tp_slide_handle;
    return tp_slide->slide_pos;
}

static inline void matrix_reset_cb_tmrs(touchpad_matrix_t* tp_matrix)
{
    tp_matrix_cus_cb_t *custom_cb = tp_matrix->custom_cbs;
    while (custom_cb != NULL) {
        if (custom_cb->tmr != NULL) {
            xTimerReset(custom_cb->tmr, portMAX_DELAY);
        }
        custom_cb = custom_cb->next_cb;
    }
}
static inline void matrix_stop_cb_tmrs(touchpad_matrix_t* tp_matrix)
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
    touchpad_matrix_t *tp_matrix = matrix_arg->tp_matrix;
    int x_idx = matrix_arg->tp_idx;
    touchpad_dev_t *tp_dev;
    int idx = -1;
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        return;
    }
    uint16_t val;
    for (int j = 0; j < tp_matrix->y_num; j++) {
        touchpad_read(tp_matrix->y_tps[j], &val);
        tp_dev = (touchpad_dev_t*) tp_matrix->y_tps[j];
        if (val < tp_dev->threshold) {
            if (idx < 0) {
                idx = x_idx * tp_matrix->y_num + j;
            } else {
                return;
            }
        }
    }
    if (idx >= 0) {
        tp_matrix->active_state = TOUCHPAD_STATE_PUSH;
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
    touchpad_matrix_t *tp_matrix = matrix_arg->tp_matrix;
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
    touchpad_matrix_t *tp_matrix = matrix_arg->tp_matrix;
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
    touchpad_matrix_t *tp_matrix = tp_matrix_cb->tp_matrix;
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        tp_matrix->active_state = TOUCHPAD_STATE_PRESS;
        uint8_t idx = tp_matrix->active_idx;
        tp_matrix_cb->cb(tp_matrix_cb->arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
    }
}

static void tp_matrix_trigger_cb(TimerHandle_t xTimer)
{
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) pvTimerGetTimerID(xTimer);
    if (tp_matrix->active_state != TOUCHPAD_STATE_IDLE) {
        tp_matrix->active_state = TOUCHPAD_STATE_PRESS;
        uint8_t idx = tp_matrix->active_idx;
        tp_matrix->serial_cb.cb(tp_matrix->serial_cb.arg, idx / tp_matrix->y_num, idx % tp_matrix->y_num);
        xTimerChangePeriod(tp_matrix->serial_tmr, tp_matrix->serial_interval_ms / portTICK_RATE_MS, portMAX_DELAY);
    }
}

touchpad_matrix_handle_t touchpad_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, uint32_t filter_value)
{
    IOT_CHECK(TAG, x_num != 0 && x_num < TOUCH_PAD_MAX, NULL);
    IOT_CHECK(TAG, y_num != 0 && y_num < TOUCH_PAD_MAX, NULL);
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) calloc(1, sizeof(touchpad_matrix_t));
    IOT_CHECK(TAG, tp_matrix != NULL, NULL);
    tp_matrix->x_tps = (touchpad_handle_t*) calloc(x_num, sizeof(touchpad_handle_t));
    if (tp_matrix->x_tps == NULL) {
        ESP_LOGE(TAG, "create touchpad matrix error! no available memory!");
        free(tp_matrix);
        return NULL;
    }
    tp_matrix->y_tps = (touchpad_handle_t*) calloc(y_num, sizeof(touchpad_handle_t));
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
        tp_matrix->x_tps[i] = touchpad_create(x_tps[i], thres_percent, filter_value);
        if (tp_matrix->x_tps[i] == NULL) {
            goto CREATE_ERR;
        }
        tp_matrix->matrix_args[i].tp_matrix = tp_matrix;
        tp_matrix->matrix_args[i].tp_idx = i;
        touchpad_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_PUSH, tp_matrix_push_cb, tp_matrix->matrix_args + i);
        touchpad_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_RELEASE, tp_matrix_release_cb, tp_matrix->matrix_args + i);
        touchpad_add_cb(tp_matrix->x_tps[i], TOUCHPAD_CB_TAP, tp_matrix_tap_cb, tp_matrix->matrix_args + i);
    }
    for (int i = 0; i < y_num; i++) {
        tp_matrix->y_tps[i] = touchpad_create(y_tps[i], thres_percent, filter_value);
        if (tp_matrix->y_tps[i] == NULL) {
            goto CREATE_ERR;
        }
    }
    tp_matrix->active_state = TOUCHPAD_STATE_IDLE;
    return (touchpad_matrix_handle_t)tp_matrix;

CREATE_ERR:
    ESP_LOGE(TAG, "touchpad matrix creaete error!");
    for (int i = 0; i < x_num; i++) {
        if (tp_matrix->x_tps[i] != NULL) {
            touchpad_delete(tp_matrix->x_tps[i]);
            tp_matrix->x_tps[i] = NULL;
        }
    }
    for (int i = 0; i < y_num; i++) {
        if (tp_matrix->y_tps[i] != NULL) {
            touchpad_delete(tp_matrix->y_tps[i]);
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

esp_err_t touchpad_matrix_delete(touchpad_matrix_handle_t tp_matrix_hd)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) tp_matrix_hd;
    for (int i = 0; i < tp_matrix->x_num; i++) {
        if (tp_matrix->x_tps[i] != NULL) {
            touchpad_delete(tp_matrix->x_tps[i]);
            tp_matrix->x_tps[i] = NULL;
        }
    }
    for (int i = 0; i < tp_matrix->y_num; i++) {
        if (tp_matrix->y_tps[i] != NULL) {
            touchpad_delete(tp_matrix->y_tps[i]);
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

esp_err_t touchpad_matrix_add_cb(touchpad_matrix_handle_t tp_matrix_hd, touchpad_cb_type_t cb_type, touchpad_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, cb_type < TOUCHPAD_CB_MAX, ESP_FAIL);
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) tp_matrix_hd;
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

esp_err_t touchpad_matrix_add_custom_cb(touchpad_matrix_handle_t tp_matrix_hd, uint32_t press_sec, touchpad_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, press_sec != 0, ESP_FAIL);
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) tp_matrix_hd;
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

esp_err_t touchpad_matrix_set_serial_trigger(touchpad_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_matrix_cb cb, void *arg)
{
    POINT_ASSERT(TAG, tp_matrix_hd);
    POINT_ASSERT(TAG, cb);
    IOT_CHECK(TAG, trigger_thres_sec != 0, ESP_FAIL);
    IOT_CHECK(TAG, interval_ms >= portTICK_PERIOD_MS, ESP_FAIL);
    touchpad_matrix_t *tp_matrix = (touchpad_matrix_t*) tp_matrix_hd;
    tp_matrix->serial_cb.cb = cb;
    tp_matrix->serial_cb.arg = arg;
    tp_matrix->serial_thres_sec = trigger_thres_sec;
    tp_matrix->serial_interval_ms = interval_ms;
    tp_matrix->serial_tmr = xTimerCreate("serial_tmr", trigger_thres_sec*1000 / portTICK_RATE_MS, pdFALSE, tp_matrix, tp_matrix_trigger_cb);
    POINT_ASSERT(TAG, tp_matrix->serial_tmr);
    return ESP_OK;
}

static inline void prox_change_sta(proximity_sensor_t *prox_sen, proximity_status_t sta)
{
    prox_sen->state = sta;
    if (prox_sen->cb) {
        prox_sen->cb(prox_sen->arg, prox_sen->state);
    }
}

static void proximity_sta_tmr_cb(TimerHandle_t xTimer)
{
    uint16_t val;
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) pvTimerGetTimerID(xTimer);
    touchpad_read(prox_sen->tp_hd, &val);
    prox_sen->head_idx = (prox_sen->head_idx + 1) % PROXIMITY_HISTORY_NUM;
    prox_sen->his_val[prox_sen->head_idx] = val;
    const uint16_t *val_arr = prox_sen->his_val;
    bool sta_change = true;
    if (prox_sen->state == PROXIMITY_APPROACHING) {
        for (int i = prox_sen->head_idx + PROXIMITY_HISTORY_NUM; i > prox_sen->head_idx+1; i--) {
            if (val_arr[i % PROXIMITY_HISTORY_NUM] <= val_arr[(i-1) %PROXIMITY_HISTORY_NUM] || 
                val_arr[(i-1) %PROXIMITY_HISTORY_NUM] == 0) {
                sta_change = false;
                break;
            }
        }
        if (sta_change) {
            prox_change_sta(prox_sen, PROXIMITY_LEAVING);
        }
    } else if (prox_sen->state == PROXIMITY_LEAVING) {
        for (int i = prox_sen->head_idx + PROXIMITY_HISTORY_NUM; i > prox_sen->head_idx+1; i--) {
            if (val_arr[i % PROXIMITY_HISTORY_NUM] >= val_arr[(i-1) %PROXIMITY_HISTORY_NUM] || 
                val_arr[(i-1) %PROXIMITY_HISTORY_NUM] == 0) {
                sta_change = false;
                break;
            }
        }
        if (sta_change) {
            prox_change_sta(prox_sen, PROXIMITY_APPROACHING);
        }
    }
}

static void proximity_approach_cb(void *arg)
{
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) arg;
    prox_change_sta(prox_sen, PROXIMITY_APPROACHING);
    prox_sen->head_idx = 0;
    memset(prox_sen->his_val, 0, sizeof(prox_sen->his_val[0])*PROXIMITY_HISTORY_NUM);
    touchpad_read(prox_sen->tp_hd, prox_sen->his_val + prox_sen->head_idx); 
    xTimerStart(prox_sen->sta_tmr, portMAX_DELAY);
}

static void proximity_left_cb(void *arg)
{
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) arg;
    prox_change_sta(prox_sen, PROXIMITY_IDLE);
    xTimerStop(prox_sen->sta_tmr, portMAX_DELAY);
}

proximity_sensor_handle_t proximity_sensor_create(touch_pad_t tp, uint16_t thres_percent, uint32_t filter_value)
{
    IOT_CHECK(TAG, tp < TOUCH_PAD_MAX, NULL);
    IOT_CHECK(TAG, thres_percent < 1000, NULL);
    IOT_CHECK(TAG, filter_value >= portTICK_PERIOD_MS, NULL);
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) calloc(1, sizeof(proximity_sensor_t));
    IOT_CHECK(TAG, prox_sen != NULL, NULL);
    prox_sen->tp_hd = touchpad_create(tp, thres_percent, filter_value);
    if (prox_sen->tp_hd == NULL) {
        ESP_LOGE(TAG, "proximity sensor create error!");
        goto TP_CREATE_ERR;
    }
    prox_sen->sta_tmr = xTimerCreate("prox_tmr", filter_value / portTICK_RATE_MS, pdTRUE, prox_sen, proximity_sta_tmr_cb);
    if (prox_sen->sta_tmr == NULL) {
        ESP_LOGE(TAG, "proximity sensor create error!");
        goto TM_CREATE_ERR;
    }
    touchpad_add_cb(prox_sen->tp_hd, TOUCHPAD_CB_PUSH, proximity_approach_cb, prox_sen);
    touchpad_add_cb(prox_sen->tp_hd, TOUCHPAD_CB_RELEASE, proximity_left_cb, prox_sen);
    prox_sen->state = PROXIMITY_IDLE;
    return (proximity_sensor_handle_t)prox_sen; 

TM_CREATE_ERR:
    touchpad_delete(prox_sen->tp_hd);
    prox_sen->tp_hd = NULL;

TP_CREATE_ERR:
    free(prox_sen);
    return NULL;
}

esp_err_t proximity_sensor_delete(proximity_sensor_handle_t prox_hd)
{
    IOT_CHECK(TAG, prox_hd != NULL, ESP_ERR_INVALID_ARG);
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) prox_hd;
    touchpad_delete(prox_sen->tp_hd);
    prox_sen->tp_hd = NULL;
    xTimerStop(prox_sen->sta_tmr, portMAX_DELAY);
    xTimerDelete(prox_sen->sta_tmr, portMAX_DELAY);
    prox_sen->sta_tmr = NULL;
    free(prox_sen);
    return ESP_OK;
}

esp_err_t proximity_sensor_add_cb(proximity_sensor_handle_t prox_hd, proximity_cb cb, void *arg)
{
    IOT_CHECK(TAG, prox_hd != NULL, ESP_ERR_INVALID_ARG);
    IOT_CHECK(TAG, cb != NULL, ESP_ERR_INVALID_ARG);
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) prox_hd;
    prox_sen->cb = cb;
    prox_sen->arg = arg;
    return ESP_OK;  
}

esp_err_t proximity_sensor_read(proximity_sensor_handle_t prox_hd, uint16_t *pval)
{
    IOT_CHECK(TAG, prox_hd != NULL, ESP_ERR_INVALID_ARG);
    IOT_CHECK(TAG, pval != NULL, ESP_ERR_INVALID_ARG);
    proximity_sensor_t *prox_sen = (proximity_sensor_t*) prox_hd;
    uint16_t v;
    uint32_t thres;
    touchpad_read(prox_sen->tp_hd, &v);
    touchpad_get_threshold(prox_sen->tp_hd, &thres);
    *pval = v < thres ? thres - v : 0;
    return ESP_OK;
}

static void gest_state_machine(gesture_sensor_t *gest_sen, gesture_type_t state_idx, gesture_num_t first_idx, gesture_num_t second_idx)
{
    const proximity_status_t *prox_stas = gest_sen->prox_stas;
    gesture_state_t *gest_stas = gest_sen->gest_stas;
    switch (gest_stas[state_idx]) {
        case GESTURE_STATE_IDLE:
            if (prox_stas[first_idx] == PROXIMITY_APPROACHING && prox_stas[second_idx] == PROXIMITY_IDLE) {
                gest_stas[state_idx] = GESTURE_STATE_START;
            }
            break;
        case GESTURE_STATE_START:
            if (prox_stas[first_idx] != PROXIMITY_IDLE && prox_stas[second_idx] == PROXIMITY_APPROACHING) {
                gest_stas[state_idx] = GESTURE_STATE_FIRST;
            } else {
                gest_stas[state_idx] = GESTURE_STATE_IDLE;
            }
            break;
        case GESTURE_STATE_FIRST:
            if (prox_stas[first_idx] == PROXIMITY_IDLE && prox_stas[second_idx] != PROXIMITY_IDLE) {
                gest_stas[state_idx] = GESTURE_STATE_SECOND;
            } else {
                gest_stas[state_idx] = GESTURE_STATE_IDLE;
            }
            break;
        case GESTURE_STATE_SECOND:
            if (prox_stas[first_idx] == PROXIMITY_IDLE && prox_stas[second_idx] == PROXIMITY_IDLE) {
                if (gest_sen->cb != NULL) {
                    gest_sen->cb(gest_sen->arg, state_idx);
                }
                gest_stas[state_idx] = GESTURE_STATE_IDLE;
            } else {
                gest_stas[state_idx] = GESTURE_STATE_IDLE;
            }
            break;
        default:
            break;
    }
}

static void gest_proximity_cb(void *arg, proximity_status_t sta)
{
    gesture_cb_arg_t *gest_arg = (gesture_cb_arg_t*) arg;
    gesture_sensor_t *gest_sen = gest_arg->gest_sen;
    gesture_num_t sen_idx = gest_arg->idx;
    gest_sen->prox_stas[sen_idx] = sta;
    printf("%d %d\n", sen_idx, sta);
    if (sen_idx == GESTURE_SENSOR_TOP || sen_idx == GESTURE_SENSOR_BOTTOM) {
        gest_state_machine(gest_sen, GESTURE_TOP_TO_BOTTOM, GESTURE_SENSOR_TOP, GESTURE_SENSOR_BOTTOM);
        gest_state_machine(gest_sen, GESTURE_BOTTOM_TO_TOP, GESTURE_SENSOR_BOTTOM, GESTURE_SENSOR_TOP);
    } else {
        gest_state_machine(gest_sen, GESTURE_LEFT_TO_RIGHT, GESTURE_SENSOR_LEFT, GESTURE_SENSOR_RIGHT);
        gest_state_machine(gest_sen, GESTURE_RIGHT_TO_LEFT, GESTURE_SENSOR_RIGHT, GESTURE_SENSOR_LEFT);
    }
}

gesture_sensor_handle_t gesture_sensor_create(const touch_pad_t *tps, uint16_t thres_percent, uint32_t filter_value)
{
    IOT_CHECK(TAG, tps != NULL, NULL);
    IOT_CHECK(TAG, thres_percent < 1000, NULL);
    IOT_CHECK(TAG, filter_value >= portTICK_RATE_MS, NULL);
    gesture_sensor_t *gest_sen = (gesture_sensor_t*) calloc(1, sizeof(gesture_sensor_t));
    IOT_CHECK(TAG, gest_sen != NULL, NULL);
    for (int i = 0; i < GESTURE_TYPE_MAX; i++) {
        gest_sen->gest_stas[i] = GESTURE_STATE_IDLE;
    }
    for (int i = 0; i < GESTURE_SENSOR_MAX; i++) {
        gest_sen->prox_hds[i] = proximity_sensor_create(tps[i], thres_percent, filter_value);
        if (gest_sen->prox_hds[i] == NULL) {
            ESP_LOGE(TAG, "create gesture sensor error!\n");
            goto GESTURE_ERR;
        }
        gest_sen->prox_stas[i] = PROXIMITY_IDLE;
        gest_sen->cb_args[i].gest_sen = gest_sen;
        gest_sen->cb_args[i].idx = i;
        proximity_sensor_add_cb(gest_sen->prox_hds[i], gest_proximity_cb, (void*)(gest_sen->cb_args + i));
    }
    return (gesture_sensor_handle_t) gest_sen;

GESTURE_ERR:
    for (int i = 0; i < GESTURE_SENSOR_MAX; i++) {
        if (gest_sen->prox_hds[i] != NULL) {
            proximity_sensor_delete(gest_sen->prox_hds[i]);
            gest_sen->prox_hds[i] = NULL;
        }
    }
    free(gest_sen);
    return NULL;
}

esp_err_t gesture_sensor_delete(gesture_sensor_handle_t gest_hd)
{
    IOT_CHECK(TAG, gest_hd != NULL, ESP_ERR_INVALID_ARG);
    gesture_sensor_t *gest_sen = (gesture_sensor_t*) gest_hd;
    for (int i = 0; i < GESTURE_SENSOR_MAX; i++) {
        proximity_sensor_delete(gest_sen->prox_hds[i]);
        gest_sen->prox_hds[i] = NULL;
    }
    free(gest_sen);
    return ESP_OK;
}

esp_err_t gesture_sensor_add_cb(gesture_sensor_handle_t gest_hd, gesture_cb cb, void *arg)
{
    IOT_CHECK(TAG, gest_hd != NULL, ESP_ERR_INVALID_ARG);
    IOT_CHECK(TAG, cb != NULL, ESP_ERR_INVALID_ARG);
    gesture_sensor_t *gest_sen = (gesture_sensor_t*) gest_hd;
    gest_sen->cb = cb;
    gest_sen->arg = arg;
    return ESP_OK;
}