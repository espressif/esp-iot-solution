/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "apps/sntp/sntp.h"
#include "iot_lcd.h"
#include "iot_wifi_conn.h"
#include "aws_iot_demo.h"
#include "image.h"

const char *AWSIOTTAG = "aws_iot";

static void app_sntp_init()
{
    time_t now = 0;  // wait for time to be set
    struct tm timeinfo = { 0 };
    int retry = 0;
    char strftime_buf[64];
    ESP_LOGI("Time", "Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*) "pool.ntp.org");
    sntp_init();

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < 10) {
        ESP_LOGI("Time", "Waiting for system time to be set... (%d/%d)\n",
                retry, 10);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    /*Acquire new time*/
    setenv("TZ", "GMT-8", 1); // Set timezone to Shanghai time
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI("Time", "%s\n", strftime_buf);
}

extern "C" void app_main()
{
    app_lcd_init();
    app_lcd_wifi_connecting();
    CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    printf("connect wifi\n");
    my_wifi->Connect(EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS, portMAX_DELAY);

    app_sntp_init();
#ifdef CONFIG_MBEDTLS_DEBUG
    const size_t stack_size = 36*1024;
#else
    const size_t stack_size = 36 * 1024;
#endif

    /*Start AWS task*/
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", stack_size, NULL, 5,
    NULL, 1);
}
