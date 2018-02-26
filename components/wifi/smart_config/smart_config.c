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
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "iot_smartconfig.h"

#define SC_DONE_EVT        BIT0
#define SC_STOP_REQ_EVT    BIT1

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

//a mutex to protect esptouch process.
static xSemaphoreHandle s_sc_mux = NULL;
static EventGroupHandle_t s_sc_event_group = NULL;

static const char* TAG = "SC";
static smartconfig_status_t s_sc_status = SC_STATUS_WAIT;

smartconfig_status_t iot_sc_get_status()
{
    return s_sc_status;
}

static void sc_callback(smartconfig_status_t status, void *pdata)
{
    switch (status) {
        case SC_STATUS_WAIT:
            s_sc_status = SC_STATUS_WAIT;
            ESP_LOGI(TAG, "SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            s_sc_status = SC_STATUS_FIND_CHANNEL;
            ESP_LOGI(TAG, "SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            s_sc_status = SC_STATUS_GETTING_SSID_PSWD;
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD\n");
            break;
        case SC_STATUS_LINK:
            s_sc_status = SC_STATUS_LINK;
            ESP_LOGI(TAG, "SC_STATUS_LINK\n");
            wifi_config_t *wifi_config = pdata;
            ESP_LOGI(TAG, "SSID:%s\n", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s\n", wifi_config->sta.password);
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config);
            esp_wifi_connect();
            break;
        case SC_STATUS_LINK_OVER:
            s_sc_status = SC_STATUS_LINK_OVER;
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                MEMCPY(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d\n", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(s_sc_event_group, SC_DONE_EVT);
            break;
        default:
            break;
    }
}

esp_err_t iot_sc_setup(smartconfig_type_t sc_type, wifi_mode_t wifi_mode, bool fast_mode_en)
{
    tcpip_adapter_init();
    
    IOT_CHECK(TAG ,wifi_mode != WIFI_MODE_AP, ESP_FAIL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Init WiFi
    esp_event_loop_init(NULL, NULL);
    ERR_ASSERT(TAG, esp_wifi_init(&cfg));
    ERR_ASSERT(TAG, esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_wifi_set_mode(wifi_mode);
    
    if (s_sc_event_group == NULL) {
        s_sc_event_group = xEventGroupCreate();
        POINT_ASSERT(TAG, s_sc_event_group);
    }
    ERR_ASSERT(TAG, esp_smartconfig_set_type(sc_type));
    ERR_ASSERT(TAG, esp_smartconfig_fast_mode(fast_mode_en));
    if (s_sc_mux == NULL) {
        s_sc_mux = xSemaphoreCreateMutex();
        POINT_ASSERT(TAG, s_sc_mux);
    }
    return ESP_OK;
}

void sc_stop()
{
    s_sc_status = SC_STATUS_WAIT;
    if (s_sc_event_group) {
        xEventGroupSetBits(s_sc_event_group, SC_STOP_REQ_EVT);
    }
}

esp_err_t iot_sc_start(uint32_t ticks_to_wait)
{
    portBASE_TYPE res = xSemaphoreTake(s_sc_mux, ticks_to_wait);
    if (res != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
    xEventGroupClearBits(s_sc_event_group, SC_STOP_REQ_EVT);
    s_sc_status = SC_STATUS_WAIT;
    esp_wifi_start();
    ESP_ERROR_CHECK(esp_smartconfig_start(sc_callback));
    // Wait event bits
    EventBits_t uxBits;
    uxBits = xEventGroupWaitBits(s_sc_event_group, SC_DONE_EVT | SC_STOP_REQ_EVT, true, false, ticks_to_wait);
    esp_err_t ret;
    // WiFi connected event
    if (uxBits & SC_DONE_EVT) {
        ESP_LOGI(TAG, "WiFi connected");
        ret = ESP_OK;
    }
    // WiFi stop connecting event
    else if (uxBits & SC_STOP_REQ_EVT) {
        ESP_LOGI(TAG, "Smartconfig stop.");
        ret = ESP_FAIL;
    }
    // WiFi connect timeout
    else {
        esp_wifi_stop();
        ESP_LOGW(TAG, "WiFi connect fail");
        ret = ESP_ERR_TIMEOUT;
    }
    esp_smartconfig_stop();
    xSemaphoreGive(s_sc_mux);
    return ret;
}
