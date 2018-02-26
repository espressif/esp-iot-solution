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

