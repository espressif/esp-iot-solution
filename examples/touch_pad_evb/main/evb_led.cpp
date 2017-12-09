/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "iot_touchpad.h"
#include "iot_led.h"
#include "evb.h"

CLED *led[4] = { 0 };

void evb_led_init()
{
    led[0] = new CLED(23, LED_DARK_LOW);
    led[1] = new CLED(25, LED_DARK_LOW);
    led[2] = new CLED(26, LED_DARK_LOW);
    led[3] = new CLED(22, LED_DARK_LOW);
}

