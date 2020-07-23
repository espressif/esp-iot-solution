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
#include "iot_light.h"
#include "iot_led.h"
#include "evb.h"

static char *TAG = "evb_rgb_led";

#define CHANNEL_R_IO   ((gpio_num_t)25)
#define CHANNEL_G_IO   ((gpio_num_t)26)
#define CHANNEL_B_IO   ((gpio_num_t)27)

#define CHANNEL_ID_R 0
#define CHANNEL_ID_G 1
#define CHANNEL_ID_B 2

#define LIGHT_FULL_DUTY ((1 << LEDC_TIMER_13_BIT) - 1)

static light_handle_t light = NULL;
static uint8_t g_rgb_color = 0;		    //0:red; 1:green; 2:blue;
static uint8_t g_bright_percent_r = 0;	//0 ~ 100
static uint8_t g_bright_percent_g = 0;	//0 ~ 100
static uint8_t g_bright_percent_b = 0;	//0 ~ 100

/* Clear all RGB color */
void evb_rgb_led_clear()
{
    iot_light_duty_write(light, CHANNEL_ID_R, 0, LIGHT_SET_DUTY_DIRECTLY);
    iot_light_duty_write(light, CHANNEL_ID_G, 0, LIGHT_SET_DUTY_DIRECTLY);
    iot_light_duty_write(light, CHANNEL_ID_B, 0, LIGHT_SET_DUTY_DIRECTLY);
    g_bright_percent_r = 0;
    g_bright_percent_g = 0;
    g_bright_percent_b = 0;
}

/* Set target color bright */
void evb_rgb_led_set(uint8_t color, uint8_t bright_percent)
{
    if (bright_percent > 100) {
        bright_percent = 100;
    }
    if (color > 2) {
        color = 2;
    }
    g_rgb_color = color;
    switch (color) {
        case 0:
            iot_light_duty_write(light, CHANNEL_ID_R, LIGHT_FULL_DUTY / 100 * bright_percent,
                    LIGHT_SET_DUTY_DIRECTLY);
            g_bright_percent_r = bright_percent;
            break;
        case 1:
            iot_light_duty_write(light, CHANNEL_ID_G, LIGHT_FULL_DUTY / 100 * bright_percent,
                    LIGHT_SET_DUTY_DIRECTLY);
            g_bright_percent_g = bright_percent;
            break;
        case 2:
            iot_light_duty_write(light, CHANNEL_ID_B, LIGHT_FULL_DUTY / 100 * bright_percent,
                    LIGHT_SET_DUTY_DIRECTLY);
            g_bright_percent_b = bright_percent;
            break;
    }
}

/* Get target colot bright */
void evb_rgb_led_get(uint8_t color, uint8_t *bright_percent)
{
    if (bright_percent == NULL) {
        return;
    }
    switch (color) {
        case 0:
            *bright_percent = g_bright_percent_r;
            break;
        case 1:
            *bright_percent = g_bright_percent_g;
            break;
        case 2:
            *bright_percent = g_bright_percent_b;
            break;
    }
}

/* Get last changed colot */
uint8_t evb_rgb_led_color_get()
{
    return g_rgb_color;
}

void evb_rgb_led_init()
{
    // Init RGB component.
    if (light == NULL) {
        light = iot_light_create(LEDC_TIMER_0, LEDC_LOW_SPEED_MODE, 1000, 3, LEDC_TIMER_13_BIT);
        if (NULL == light)
        {
            ESP_LOGE(TAG, "light create failed!");
        }
        
    }
    iot_light_channel_regist(light, CHANNEL_ID_R, CHANNEL_R_IO, LEDC_CHANNEL_0);
    iot_light_channel_regist(light, CHANNEL_ID_G, CHANNEL_G_IO, LEDC_CHANNEL_1);
    iot_light_channel_regist(light, CHANNEL_ID_B, CHANNEL_B_IO, LEDC_CHANNEL_2);

    iot_light_duty_write(light, CHANNEL_ID_R, 600, LIGHT_SET_DUTY_DIRECTLY);
    iot_light_duty_write(light, CHANNEL_ID_G, 600, LIGHT_SET_DUTY_DIRECTLY);
    iot_light_duty_write(light, CHANNEL_ID_B, 600, LIGHT_SET_DUTY_DIRECTLY);
}
