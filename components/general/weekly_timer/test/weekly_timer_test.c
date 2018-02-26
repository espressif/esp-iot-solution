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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "iot_weekly_timer.h"
#include "iot_wifi_conn.h"
#include "unity.h"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD
static weekly_timer_handle_t tmr1, tmr2;
void timer_cb(void *timer_name)
{
    ets_printf("timer %s is trigered\n", (char*)timer_name);
    /*if (strcmp(timer_name, "timer0_num0") == 0) {
        iot_weekly_timer_stop(tmr2);
        event_timer_delete(tmr2);
    }*/
    return;
}

void weekly_timer_test()
{
    nvs_flash_init();
    iot_wifi_setup(WIFI_MODE_STA);
    iot_wifi_connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);

    iot_weekly_timer_init();
    vTaskDelay(5000/portTICK_PERIOD_MS);

    event_time_t event_tm[2];
    event_tm[0].hour = 10;
    event_tm[0].minute = 25;
    event_tm[0].second = 0;
    event_tm[0].tm_cb = timer_cb;
    event_tm[0].arg = "timer0_num0";
    event_tm[0].en = true;
    event_tm[1].hour = 10;
    event_tm[1].minute = 26;
    event_tm[1].second = 0;
    event_tm[1].tm_cb = timer_cb;
    event_tm[1].arg = "timer0_num1";
    event_tm[1].en = true;
    weekday_mask_t w_m;
    w_m.en = 0;
    w_m.enable = 1;
    w_m.tuesday = 1;
    tmr1 = iot_weekly_timer_add(true, w_m, 2, event_tm);
    iot_weekly_timer_start(tmr1);

    w_m.en = 0;
    w_m.enable = 1;
    event_tm[0].hour = 10;
    event_tm[0].minute = 26;
    event_tm[0].second = 0;
    event_tm[0].tm_cb = timer_cb;
    event_tm[0].arg = "timer1_num0";
    event_tm[0].en = true;
    tmr2 = iot_weekly_timer_add(false, w_m, 1, event_tm);
    iot_weekly_timer_start(tmr2);

    while(1) {
        time_t time_now = time(NULL);
        printf("time: %s \n", ctime(&time_now));
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

TEST_CASE("Weekly timer test", "[weekly_timer][iot]")
{
    weekly_timer_test();
}
