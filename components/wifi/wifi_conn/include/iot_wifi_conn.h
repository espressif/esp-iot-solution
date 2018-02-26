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
esp_err_t iot_wifi_setup(wifi_mode_t);

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
esp_err_t iot_wifi_connect(const char *ssid, const char *pwd, uint32_t ticks_to_wait);

/**
 *  @brief WiFi stop connecting
 */
void iot_wifi_disconnect();

/**
  * @brief  get wifi status.
  *
  * @return status of the wifi station
  */
wifi_sta_status_t iot_wifi_get_status();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/**
 * class of wifi
 * simple usage:
 * CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
 * my_wifi->Connect("AP_SSID", "AP_PASSWORD", portMAX_DELAY);
 */
class CWiFi
{
private:
    static CWiFi* m_instance;

    /**
     * prevent constructing in singleton mode
     */
    CWiFi(const CWiFi&);
    CWiFi& operator=(const CWiFi&);

    /**
     * @brief  constructor of CLED
     *
     * @param  mode refer to enum wifi_mode_t
     */
    CWiFi(wifi_mode_t mode = WIFI_MODE_STA);

    /**
     * prevent memory leak of m_instance
     */
    class CGarbo
    {
    public:
        ~CGarbo()
        {
            if (CWiFi::m_instance) {
                delete CWiFi::m_instance;
            }
        }
    };
    static CGarbo garbo;

public:
    /**
     * @brief get the only instance of CWiFi
     *
     * @param product_info the information of product
     *
     * @return pointer to a CWiFi instance
     */
    static CWiFi* GetInstance(wifi_mode_t mode = WIFI_MODE_STA);

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
    esp_err_t Connect(const char *ssid, const char *pwd, uint32_t ticks_to_wait);
    
    /**
     *  @brief WiFi stop connecting
     */
    void Disconnect();

    /**
     * @brief  get wifi status.
     *
     * @return status of the wifi station
     */
    wifi_sta_status_t Status();
};
#endif

#endif
