/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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
