/* OLED screen Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "app_oled.h"
#include "iot_ssd1306.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define OLED_SHOW_LEFT_TOUCH        9
#define OLED_SHOW_RIGHT_TOUCH       8
#define TOUCHPAD_THRES_PERCENT      950
#define TOUCHPAD_FILTER_INTERVAL    10

void COled::init()
{
    show_signs();
}

void COled::clean()
{
    clear_screen(0);
    show_signs();
}

void COled::show_signs()
{
    draw_bitmap(0, 2, &c_chSingal816[0], 16, 8);
    draw_bitmap(24, 2, &c_chBluetooth88[0], 8, 8);
    draw_bitmap(40, 2, &c_chMsg816[0], 16, 8);
    draw_bitmap(64, 2, &c_chGPRS88[0], 8, 8);
    draw_bitmap(90, 2, &c_chAlarm88[0], 8, 8);
    draw_bitmap(112, 2, &c_chBat816[0], 16, 8);
}

esp_err_t COled::show_temp(float temprature)
{
    char tempraturestr[6];
    sprintf(tempraturestr, "%4.1f", temprature);
    tempraturestr[4] = '\0';
    printf("tempraturestr %s\n", tempraturestr);

    draw_string(0, 16, (const uint8_t *) "TEM:", 16, 1);
    draw_3216char(36, 16, tempraturestr[0]);
    draw_3216char(52, 16, tempraturestr[1]);
    draw_char(70, 30, tempraturestr[2], 16, 1);
    draw_3216char(75, 16, tempraturestr[3]);

    draw_string(0, 52, (const uint8_t *) "HUM", 12, 0);
    draw_string(52, 52, (const uint8_t *) "MENU", 12, 0);
    draw_string(98, 52, (const uint8_t *) "TEM", 12, 0);

    return refresh_gram();
}

esp_err_t COled::show_humidity(float humidity)
{
    char humiditystr[6];
    sprintf(humiditystr, "%4.1f", humidity);
    humiditystr[4] = '\0';
    printf("humiditystr %s\n", humiditystr);

    draw_string(0, 16, (const uint8_t *) "HUM:", 16, 1);
    draw_3216char(36, 16, humiditystr[0]);
    draw_3216char(52, 16, humiditystr[1]);
    draw_char(70, 30, humiditystr[2], 16, 1);
    draw_3216char(75, 16, humiditystr[3]);

    draw_string(0, 52, (const uint8_t *) "HUM", 12, 0);
    draw_string(52, 52, (const uint8_t *) "MENU", 12, 0);
    draw_string(98, 52, (const uint8_t *) "TEM", 12, 0);
    return refresh_gram();
}

esp_err_t COled::show_time()
{
    struct tm timeinfo;
    char strftime_buf[64];
    time_t t_now;
    time(&t_now);
    setenv("TZ", "GMT-8", 1);
    tzset();
    localtime_r(&t_now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    draw_3216char(0, 16, strftime_buf[11]);
    draw_3216char(16, 16, strftime_buf[12]);
    draw_3216char(32, 16, strftime_buf[13]);
    draw_3216char(48, 16, strftime_buf[14]);
    draw_3216char(64, 16, strftime_buf[15]);
    draw_1616char(80, 32, strftime_buf[16]);
    draw_1616char(96, 32, strftime_buf[17]);
    draw_1616char(112, 32, strftime_buf[18]);
    char *day = strftime_buf;
    day[3] = '\0';
    draw_string(87, 16, (const uint8_t *) day, 14, 1);
    draw_string(0, 52, (const uint8_t *) "HUM", 12, 0);
    draw_string(52, 52, (const uint8_t *) "MENU", 12, 0);
    draw_string(98, 52, (const uint8_t *) "TEM", 12, 0);
    return refresh_gram();
}

