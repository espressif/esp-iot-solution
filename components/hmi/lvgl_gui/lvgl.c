// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

/* FreeRTOS includes */
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* LVGL includes */
#include "iot_lvgl.h"

static void IRAM_ATTR lvgl_task_time_callback(TimerHandle_t xTimer)
{
    /* Periodically call this function.
     * The timing is not critical but should be between 1..10 ms */
    lv_task_handler();
}

static void IRAM_ATTR lv_tick_task_callback(TimerHandle_t xTimer)
{
    /* Initialize a Timer for 1 ms period and
     * in its interrupt call
     * lv_tick_inc(1); */
    lv_tick_inc(1);
}

void lvgl_init()
{
    /* LittlevGL work fine only when CONFIG_FREERTOS_HZ is 1000HZ */
    assert(CONFIG_FREERTOS_HZ == 1000);

    /* Initialize LittlevGL */
    lv_init();

    /* Initialize a Timer for 1 ms period and in its interrupt call */
    TimerHandle_t lvgl_tick_timer = xTimerCreate(
                                        "lv_tickinc_task",
                                        1 / portTICK_PERIOD_MS, // period time
                                        pdTRUE,                 // auto load
                                        (void *)NULL,           // timer parameter
                                        lv_tick_task_callback); // timer callback
    xTimerStart(lvgl_tick_timer, 0);

    /* Display interface */
    lvgl_lcd_display_init(); /* Initialize your display */

#if defined(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE) || defined(CONFIG_LVGL_DRIVER_TOGGLE_ENABLE)
    /* Input device interface */
    lv_indev_drv_t indevdrv = lvgl_indev_init(); /* Initialize your indev */
#endif

    /* lv_task_handler should be called periodically around 10ms */
    TimerHandle_t lvgl_timer = xTimerCreate(
                                   "lv_task",
                                   10 / portTICK_PERIOD_MS,  // period time
                                   pdTRUE,                   // auto load
                                   (void *)NULL,             // timer parameter
                                   lvgl_task_time_callback); // timer callback
    xTimerStart(lvgl_timer, 0);

    vTaskDelay(100 / portTICK_PERIOD_MS);

#ifdef CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE
    /* Calibrate touch screen */
    lvgl_calibrate_mouse(indevdrv, false);
#endif
}