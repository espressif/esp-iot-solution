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

#ifndef _IOT_EVENT_TIMER_H_
#define _IOT_EVENT_TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TIMER_NEAREST_INF     2147483647
typedef void (*timer_cb_t)(void *);

typedef struct {
    int hour;              
    int minute;
    int second;
    timer_cb_t tm_cb;       /**< pointer to the callback function */
    void* arg;              /**< pointer to the argument */
    bool en;                /**< set to 1 if you want this time work */
} event_time_t;

typedef union {
    struct {
        uint8_t sunday    : 1;      /**< set the according day to 1 if the timer should work on that day */
        uint8_t monday    : 1;
        uint8_t tuesday   : 1;
        uint8_t wednesday : 1;
        uint8_t thursday  : 1;
        uint8_t friday    : 1;
        uint8_t saturday  : 1;
        uint8_t enable    : 1;      /**< set to 1 if you want to start the timer immediately */
    };
    uint8_t en;
} weekday_mask_t;
typedef void* weekly_timer_handle_t;

/**
  * @brief  event_timer initialize
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_weekly_timer_init();

/**
  * @brief  add an event_timer
  *
  * @param  weekly_loop whether the timer sould trigger weekly
  * @param  week_mark decide on which days the timer should trigger
  * @param  time_num how many point-in-time the timer should trigger one day
  * @param  time_group the point-in-times and corresponding callback functions and arguments 
  *
  * @return the handle to the timer
  */
weekly_timer_handle_t iot_weekly_timer_add(bool weekly_loop, weekday_mask_t week_mark, uint32_t time_num, const event_time_t *time_group);

/**
  * @brief  delete a timer
  *
  * @param  timer_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_weekly_timer_delete(weekly_timer_handle_t timer_handle);

/**
  * @brief  start a timer
  *
  * @param  timer_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_weekly_timer_start(weekly_timer_handle_t timer_handle);

/**
  * @brief  stop a timer
  *
  * @param  timer_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_weekly_timer_stop(weekly_timer_handle_t timer_handle);

#ifdef __cplusplus
}
#endif

#endif
