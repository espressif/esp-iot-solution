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
#include "esp_system.h"
#include "iot_wifi_conn.h"

CWiFi* CWiFi::m_instance = NULL;
static xSemaphoreHandle s_iot_wifi_mux = xSemaphoreCreateMutex();

CWiFi* CWiFi::GetInstance(wifi_mode_t mode)
{
    if (m_instance == NULL) {
        xSemaphoreTake(s_iot_wifi_mux, portMAX_DELAY);
        if (m_instance == NULL) {
            m_instance = new CWiFi(mode);
        }
        xSemaphoreGive(s_iot_wifi_mux);
    }
    return m_instance;
}

CWiFi::CWiFi(wifi_mode_t mode)
{
    iot_wifi_setup(mode);
}

esp_err_t CWiFi::Connect(const char *ssid, const char *pwd, uint32_t ticks_to_wait)
{
    return iot_wifi_connect(ssid, pwd, ticks_to_wait);
}

void CWiFi::Disconnect()
{
    iot_wifi_disconnect();
}

wifi_sta_status_t CWiFi::Status()
{
    return iot_wifi_get_status();
}

