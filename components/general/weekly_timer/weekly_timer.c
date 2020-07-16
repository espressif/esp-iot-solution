// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_weekly_timer.h"
#include "lwip/apps/sntp.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }

#define ERR_ASSERT(tag, param)      IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

#define INIT_BUFF_LEN   5
#define SEC_PER_DAY     86400

typedef struct {
    bool weekly_loop;               /**< whether the timer would loop weekly */
    weekday_mask_t week_mark;
    uint32_t time_num;              /**< how many event_time this timer should contain */
    time_t nearest_tm;      
    uint32_t nearest_idx;
    TimerHandle_t tmr;
    event_time_t time_group[0];
} event_timer_loop_t;

static const char* TAG = "event_timer";
static event_timer_loop_t **g_timer_loops = NULL;
static uint32_t g_max_num = INIT_BUFF_LEN;
static time_t g_now;
static esp_err_t weekly_timer_update(event_timer_loop_t* tm_loop);

static void weekly_timer_cb(TimerHandle_t xTimer)
{
    event_timer_loop_t* tm_loop = (event_timer_loop_t*) pvTimerGetTimerID(xTimer);
    event_time_t* event_tm = &tm_loop->time_group[tm_loop->nearest_idx];
    if (tm_loop->weekly_loop == false) {
        event_tm->en = false;
    }
    event_tm->tm_cb(event_tm->arg);
    weekly_timer_update(tm_loop);
}

static time_t weekly_timer_loop_get_nearest(event_timer_loop_t* timer_loop)
{
    timer_loop->nearest_tm = TIMER_NEAREST_INF;
    struct tm timeinfo, tminfo_loc;
    uint32_t old_idx = timer_loop->nearest_idx;
    time_t time_loc;
    time(&g_now);
    localtime_r(&g_now, &timeinfo);
    if (timer_loop->week_mark.enable == 0) {
        return TIMER_NEAREST_INF;
    }
    for (int i = 0; i < timer_loop->time_num; i++) {
        if (timer_loop->time_group[i].en) {
            event_time_t ev_tm = timer_loop->time_group[i];
            tminfo_loc = timeinfo;
            tminfo_loc.tm_hour = ev_tm.hour;
            tminfo_loc.tm_min = ev_tm.minute;
            tminfo_loc.tm_sec = ev_tm.second;
            time_loc = mktime(&tminfo_loc);
            if (timer_loop->weekly_loop) {
                int week_day = timeinfo.tm_wday;
                int day_diff = 0;
                if (i == old_idx) {
                    week_day = (week_day + 1) % 7;
                    day_diff++;
                }
                while (day_diff <= 7) {
                    if ((timer_loop->week_mark.en >> week_day) & 0x01) {
                        if ((time_loc + SEC_PER_DAY * day_diff) > g_now) {
                            time_loc += (SEC_PER_DAY * day_diff);
                            break;
                        }
                    }
                    week_day = (week_day + 1) % 7;
                    day_diff++;
                }
            } else {
                time_loc = time_loc > g_now ? time_loc : (time_loc + SEC_PER_DAY);
            }
            if (time_loc < timer_loop->nearest_tm) {
                timer_loop->nearest_tm = time_loc;
                timer_loop->nearest_idx = i;
            }
        }
    }
    return timer_loop->nearest_tm;
}

static esp_err_t weekly_timer_update(event_timer_loop_t* tm_loop)
{
    POINT_ASSERT(TAG, g_timer_loops);
    POINT_ASSERT(TAG, tm_loop);
    struct tm timeinfo;
    time(&g_now);
    time_t tm_loc;
    localtime_r(&g_now, &timeinfo);
    if (timeinfo.tm_year <= (2016 -1900))
        return ESP_FAIL;  
    tm_loc = weekly_timer_loop_get_nearest(tm_loop);
    xTimerStop(tm_loop->tmr, portMAX_DELAY);
    if (tm_loc != TIMER_NEAREST_INF && (tm_loc - g_now) > 0) {
        xTimerChangePeriod(tm_loop->tmr, (tm_loc - g_now) * 1000 / portTICK_PERIOD_MS, portMAX_DELAY);
        xTimerStart(tm_loop->tmr, portMAX_DELAY);
        ESP_LOGI(TAG, "the timer at %p will trigger in %ld seconds", tm_loop, tm_loc-g_now);
    }
    return ESP_OK;
}

static void weekly_timer_obtain_time_task()
{
    struct tm timeinfo;
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    localtime_r(&g_now, &timeinfo);
    while (timeinfo.tm_year < (2016 -1900)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        time(&g_now);
        localtime_r(&g_now, &timeinfo);
    }
    setenv("TZ", "GMT-8", 1);
    tzset();
    localtime_r(&g_now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
    for (int i = 0; i < g_max_num; i++) {
        if (g_timer_loops[i] != NULL) {
            weekly_timer_update(g_timer_loops[i]);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t iot_weekly_timer_init()
{
    if (g_timer_loops != NULL) {
        return ESP_FAIL;
    }
    g_timer_loops = (event_timer_loop_t**)calloc(INIT_BUFF_LEN, sizeof(event_timer_loop_t*));
    memset(g_timer_loops, 0, INIT_BUFF_LEN * sizeof(event_timer_loop_t*));
    POINT_ASSERT(TAG, g_timer_loops);
    struct tm timeinfo;
    g_now = 0;
    time(&g_now);
    localtime_r(&g_now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900)) {
        xTaskCreate(weekly_timer_obtain_time_task, "obtain_time", 2048, NULL, 7, NULL);
    }
    return ESP_OK;
}

weekly_timer_handle_t iot_weekly_timer_add(bool weekly_loop, weekday_mask_t week_mark, uint32_t time_num, const event_time_t *time_group)
{
    IOT_CHECK(TAG, g_timer_loops != NULL, NULL);
    IOT_CHECK(TAG, time_group != NULL, NULL);
    int i = 0;
    for (i = 0; i < g_max_num; i++) {
        if (g_timer_loops[i] == NULL) {
            break;
        }
    }
    if (i == g_max_num) {
        g_max_num = g_max_num * 2;
        event_timer_loop_t **new_time_loops = (event_timer_loop_t**)calloc(g_max_num, sizeof(event_timer_loop_t*));
        IOT_CHECK(TAG, new_time_loops != NULL, NULL);
        memset(new_time_loops, 0, g_max_num * sizeof(event_timer_loop_t*));
        memcpy(new_time_loops, g_timer_loops, (g_max_num / 2) * sizeof(event_timer_loop_t*));
        free(g_timer_loops);
        g_timer_loops = new_time_loops;
    }
    event_timer_loop_t* new_loop = (event_timer_loop_t*)calloc(1, sizeof(event_timer_loop_t) + time_num * sizeof(event_time_t));
    IOT_CHECK(TAG, new_loop != NULL, NULL);
    new_loop->weekly_loop = weekly_loop;
    new_loop->week_mark = week_mark;
    new_loop->time_num = time_num;
    new_loop->nearest_tm = TIMER_NEAREST_INF;
    new_loop->nearest_idx = TIMER_NEAREST_INF;
    memcpy(new_loop->time_group, time_group, time_num * sizeof(event_time_t));
    new_loop->tmr = xTimerCreate("event_timer", portMAX_DELAY, pdFALSE, new_loop, weekly_timer_cb);
    g_timer_loops[i] = new_loop;
    struct tm timeinfo;
    localtime_r(&g_now, &timeinfo);
    if (timeinfo.tm_year >= (2016 -1900)) {
        weekly_timer_update(new_loop);
    }
    return (weekly_timer_handle_t)new_loop;
}

esp_err_t iot_weekly_timer_delete(weekly_timer_handle_t timer_handle)
{
    POINT_ASSERT(TAG, timer_handle);
    event_timer_loop_t* tm_loop = (event_timer_loop_t*) timer_handle;
    for (int i = 0; i < g_max_num; i++) {
        if (g_timer_loops[i] == tm_loop) {
            g_timer_loops[i] = NULL;
            xTimerStop(tm_loop->tmr, portMAX_DELAY);
            xTimerDelete(tm_loop->tmr, portMAX_DELAY);
            tm_loop->tmr = NULL;
            free(tm_loop);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t iot_weekly_timer_start(weekly_timer_handle_t timer_handle)
{
    POINT_ASSERT(TAG, timer_handle);
    event_timer_loop_t* tm_loop = (event_timer_loop_t*) timer_handle;
    tm_loop->week_mark.enable = 1;
    tm_loop->nearest_tm = TIMER_NEAREST_INF;
    tm_loop->nearest_idx = TIMER_NEAREST_INF;
    for(int j = 0; j < tm_loop->time_num; j++) {
        tm_loop->time_group[j].en = true;
    }
    weekly_timer_update(tm_loop);
    return ESP_OK;
}

esp_err_t iot_weekly_timer_stop(weekly_timer_handle_t timer_handle)
{
    POINT_ASSERT(TAG, timer_handle);
    event_timer_loop_t* tm_loop = (event_timer_loop_t*) timer_handle;
    tm_loop->week_mark.enable = 0;
    tm_loop->nearest_tm = TIMER_NEAREST_INF;
    tm_loop->nearest_idx = TIMER_NEAREST_INF;
    for(int j = 0; j < tm_loop->time_num; j++) {
        tm_loop->time_group[j].en = false;
    }
    weekly_timer_update(tm_loop);
    return ESP_OK;
}
