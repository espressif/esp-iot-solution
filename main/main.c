/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "wifi.h"

void app_main()
{
    nvs_flash_init();
    wifi_sta_status_t wifi_st = wifi_get_status();
    printf("wifi status:%d\n", wifi_st);
    wifi_setup(WIFI_MODE_STA);
    wifi_connect_start("IOT_DEMO_TEST", "123456789", portMAX_DELAY);
    wifi_st = wifi_get_status();
    printf("wifi status:%d\n", wifi_st);
}
