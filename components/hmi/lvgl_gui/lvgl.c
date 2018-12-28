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

// wait for execute lv_task_handler and lv_tick_inc to avoid some widget don't refresh.
#define LVGL_INIT_DELAY 100 // unit ms

static void lv_tick_timercb(void *timer)
{
    /* Initialize a Timer for 1 ms period and
     * in its interrupt call
     * lv_tick_inc(1); */
    lv_tick_inc(1);
}

static void lv_task_timercb(void *timer)
{
    /* Periodically call this function.
     * The timing is not critical but should be between 1..10 ms */
    lv_task_handler();
}

void lvgl_init()
{
    /* LittlevGL work fine only when CONFIG_FREERTOS_HZ is 1000HZ */
    assert(CONFIG_FREERTOS_HZ == 1000);

    /* Initialize LittlevGL */
    lv_init();

    esp_timer_create_args_t timer_conf = {
        .callback = lv_tick_timercb,
        .name     = "lv_tick_timer"
    };

    esp_timer_handle_t g_wifi_connect_timer = NULL;
    esp_timer_create(&timer_conf, &g_wifi_connect_timer);

    esp_timer_start_periodic(g_wifi_connect_timer, 1 * 1000U);

    /* Display interface */
    lvgl_lcd_display_init(); /*Initialize your display*/

#if defined(CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE) || defined(CONFIG_LVGL_DRIVER_TOGGLE_ENABLE)
    /* Input device interface */
    lv_indev_drv_t indevdrv = lvgl_indev_init(); /*Initialize your indev*/
#endif

    esp_timer_create_args_t lv_task_timer_conf = {
        .callback = lv_task_timercb,
        .name     = "lv_task_timer"
    };

    esp_timer_handle_t lv_task_timer = NULL;
    esp_timer_create(&lv_task_timer_conf, &lv_task_timer);

    esp_timer_start_periodic(lv_task_timer, 5 * 1000U);

    vTaskDelay(LVGL_INIT_DELAY / portTICK_PERIOD_MS);    // wait for execute lv_task_handler and lv_tick_inc to avoid some widget don't refresh.

#ifdef CONFIG_LVGL_DRIVER_TOUCH_SCREEN_ENABLE
    /* Calibrate touch screen */
    lvgl_calibrate_mouse(indevdrv, false);
#endif
}