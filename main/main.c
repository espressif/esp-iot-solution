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

#define WIFI_TEST   1
#define SC_TEST     0
#define TOUCHPAD_TEST   0
#define LED_TEST    0

#if TOUCHPAD_TEST
extern xQueueHandle xQueue_touch_pad;

void touchpad_task(void *pvParameter)
{
    uint8_t recv_value;
    portBASE_TYPE xStatus;
    while(1) {
        xStatus = xQueueReceive(xQueue_touch_pad, &recv_value, portMAX_DELAY);
        if(xStatus == pdPASS){
            ets_printf("the pressed touch pad number:%d\n", recv_value);
        }
    }
}
#endif

#if LED_TEST
#define LED_0_IO    18
#define LED_1_IO    19
#define LED_ID_0    0
#define LED_ID_1    1
#define LED_IO_MASK     (BIT(LED_0_IO) | BIT(LED_1_IO))
#endif

void app_main()
{
    nvs_flash_init();
#if WIFI_TEST
    wifi_sta_status_t wifi_st = wifi_get_status();
    printf("wifi status:%d\n", wifi_st);
    wifi_setup(WIFI_MODE_STA);
    wifi_connect_start("IOT_DEMO_TEST", "123456789", portMAX_DELAY);
    wifi_st = wifi_get_status();
    printf("wifi status:%d\n", wifi_st);
#endif

#if SC_TEST
    smartconfig_setup(SC_TYPE_ESPTOUCH, WIFI_MODE_STA);
    smartconfig_start(portMAX_DELAY);
#endif

#if TOUCHPAD_TEST
     touchpad_setup(TOUCH_PAD_MASK, TOUCH_PAD_THRESHOLD);
     xTaskCreate(&touchpad_task, "touchpad_task", 2048*2, NULL, 5, NULL);
#endif

#if LED_TEST
    led_setup(LED_IO_MASK, 2);
    led_slow_blink(LED_ID_0);
    led_level_set(LED_ID_1, 0);
    ets_delay_us(10*1000*1000);
    led_quick_blink(LED_ID_1);
    led_level_set(LED_ID_0, 0);
    ets_delay_us(10*1000*1000);
    led_level_set(LED_ID_0, 1);
    led_level_set(LED_ID_1, 1);

#endif
}