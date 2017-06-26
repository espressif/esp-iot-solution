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
#ifndef _IOT_WIFI_H_
#define _IOT_WIFI_H_
#include "esp_log.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_CONNECTED_EVT	BIT0
#define WIFI_STOP_REQ_EVT   BIT1

typedef enum {
    WIFI_STATUS_STA_DISCONNECTED,           /**< ESP32 station disconnected */
    WIFI_STATUS_STA_CONNECTING,             /**< ESP32 station connecting */
    WIFI_STATUS_STA_CONNECTED,              /**< ESP32 station connected */
} wifi_sta_status_t;

/**
  * @brief  wifi initialize.
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t wifi_setup(wifi_mode_t);

/**
  * @brief  wifi connect with timeout
  *
  * @param  ssid ssid of the target AP
  * @param  pwd password of the target AP
  * @param  ticks_to_wait system tick number to wait
  *
  * @return
  *     - ESP_OK: connect to AP successfully
  *     - ESP_TIMEOUT: timeout
  *     - ESP_FAIL: fail
  */
esp_err_t wifi_connect_start(const char *ssid, const char *pwd, uint32_t ticks_to_wait);

/**
 *  @brief WiFi stop connecting
 */
void wifi_connect_stop();

/**
  * @brief  get wifi status.
  *
  * @return status of the wifi station
  */
wifi_sta_status_t wifi_get_status();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class wifi
{
public:
    wifi(wifi_mode_t mode);

    esp_err_t connect_start(const char *ssid, const char *pwd, uint32_t ticks_to_wait);
    void connect_stop();
    wifi_sta_status_t get_status();
};
#endif

#endif
