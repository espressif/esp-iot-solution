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

/* component includes */
#include <stdio.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

/* lvgl includes */
#include "iot_lvgl.h"

/* esp includes */
#include "esp_log.h"

static const char *TAG = "example_lvgl";
static TimerHandle_t lvgl_timer;
static TimerHandle_t lvgl_tick_timer;

static lv_obj_t *chart = NULL;
static lv_obj_t *gauge = NULL;
static lv_chart_series_t *series = NULL;

//lv_task_handler should be called periodically around 10ms
static void IRAM_ATTR lvgl_task_time_callback(TimerHandle_t xTimer)
{
    lv_task_handler();
}

static void IRAM_ATTR lv_tick_task_callback(TimerHandle_t xTimer)
{
    lv_tick_inc(1);
}

static lv_res_t on_led_switch_toggled(lv_obj_t *sw)
{
    ESP_LOGI(TAG, "Hello");
    return LV_RES_OK;
}

static void littlevgl_demo(void)
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);

    lv_theme_t *th = lv_theme_alien_init(100, NULL);
    lv_theme_set_current(th);

    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), NULL);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, SYMBOL_LOOP);
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, SYMBOL_HOME);
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, SYMBOL_SETTINGS);
    lv_tabview_set_tab_act(tabview, 1, false);

    chart = lv_chart_create(tab2, NULL);
    lv_obj_set_size(chart, 300, 150);
    lv_chart_set_point_count(chart, 20);
    lv_obj_align(chart, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart, (lv_chart_type_t)(LV_CHART_TYPE_POINT | LV_CHART_TYPE_LINE));
    lv_chart_set_series_opa(chart, LV_OPA_70);
    lv_chart_set_series_width(chart, 4);
    lv_chart_set_range(chart, 0, 100);
    series = lv_chart_add_series(chart, LV_COLOR_RED);

    static lv_color_t needle_colors[] = {LV_COLOR_RED};
    gauge = lv_gauge_create(tab1, NULL);
    lv_gauge_set_needle_count(gauge,
                              sizeof(needle_colors) / sizeof(needle_colors[0]), needle_colors);
    lv_obj_align(gauge, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_gauge_set_value(gauge, 0, 50);

    char name[10];
    int i;
    lv_obj_t *labels[3];
    lv_obj_t *switches[3];
    for (i = 0; i < 3; i++)
    {
        labels[i] = lv_label_create(tab3, NULL);
        sprintf(name, "LED%d", i + 1);
        lv_label_set_text(labels[i], name);
    }
    lv_obj_align(labels[0], NULL, LV_ALIGN_IN_TOP_MID, -40, 20);
    for (i = 1; i < 3; i++)
    {
        lv_obj_align(labels[i], labels[i - 1], LV_ALIGN_OUT_BOTTOM_MID, 0, 35);
    }
    for (i = 0; i < 3; i++)
    {
        switches[i] = lv_sw_create(tab3, NULL);
        lv_obj_align(switches[i], labels[i], LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_sw_set_action(switches[i], on_led_switch_toggled);
    }
}

static void user_task(void *pvParameter)
{
    uint8_t value = 0;
    while (1)
    {
        value = esp_random() % 100;
        lv_chart_set_next(chart, series, value);
        lv_gauge_set_value(gauge, 0, value);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
extern "C" void app_main()
{
    /* Initialize LittlevGL */
    lv_init();

    /* Tick interface， Initialize a Timer for 1 ms period and in its interrupt call*/
    // esp_register_freertos_tick_hook(lv_tick_task_callback);
    lvgl_tick_timer = xTimerCreate(
        "lv_tickinc_task",
        1 / portTICK_PERIOD_MS,            //period time
        pdTRUE,                            //auto load
        (void *)NULL,                      //timer parameter
        lv_tick_task_callback);            //timer callback
    xTimerStart(lvgl_tick_timer, 0);

    /* Display interface */
    lvgl_lcd_display_init();	           /*Initialize your display*/

    /* Input device interface */
    lvgl_indev_init();                     /*Initialize your indev*/

    lvgl_timer = xTimerCreate(
        "lv_task",
        10 / portTICK_PERIOD_MS,           //period time
        pdTRUE,                            //auto load
        (void *)NULL,                      //timer parameter
        lvgl_task_time_callback);          //timer callback
    xTimerStart(lvgl_timer, 0);

    littlevgl_demo();
    xTaskCreate(
        user_task,   //Task Function
        "user_task", //Task Name
        1024,        //Stack Depth
        NULL,        //Parameters
        1,           //Priority
        NULL);       //Task Handler
}
