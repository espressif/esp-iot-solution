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
#include "iot_wifi_conn.h"
#include "nvs_flash.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                             \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                                   \
        }

#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define RES_ASSERT(tag, res, ret)   IOT_CHECK(tag, (res) != pdFALSE, ret)

// Debug tag in esp log
static const char* TAG = "wifi";
// Create an event group to handle different WiFi events.
static EventGroupHandle_t s_wifi_event_group = NULL;
// Mutex to protect WiFi connect
static xSemaphoreHandle s_wifi_mux = NULL;
//static bool wifi_sta_auto_reconnect = false;
static wifi_sta_status_t s_wifi_sta_st = WIFI_STATUS_STA_DISCONNECTED;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            s_wifi_sta_st = WIFI_STATUS_STA_CONNECTING;
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START\n");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            s_wifi_sta_st = WIFI_STATUS_STA_CONNECTED;
            ESP_LOGI(TAG, "SYSREM_EVENT_STA_GOT_IP\n");
            // Set event bit to sync with other tasks.
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_EVT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            s_wifi_sta_st = WIFI_STATUS_STA_DISCONNECTED;
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED\n");
            // Clear event bit so WiFi task knows the disconnect-event
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_EVT);
            break;
        default:
            printf("Get default WiFi event: %d\n", event->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t iot_wifi_setup(wifi_mode_t wifi_mode)
{
    nvs_flash_init();
#if DEBUG_EN
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    if (s_wifi_mux == NULL) {
        s_wifi_mux = xSemaphoreCreateMutex();
        POINT_ASSERT(TAG, s_wifi_mux);
    }
    tcpip_adapter_init();
    // hoop WiFi event handler
    ERR_ASSERT(TAG, esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    // Init WiFi
    ERR_ASSERT(TAG, esp_wifi_init(&cfg));
    ERR_ASSERT(TAG, esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_wifi_set_mode(wifi_mode);
    esp_wifi_start();
    // Init event group
    s_wifi_event_group = xEventGroupCreate();
    POINT_ASSERT(TAG, s_wifi_event_group);
    return ESP_OK;
}

void iot_wifi_disconnect()
{
    esp_wifi_disconnect();
    xEventGroupSetBits(s_wifi_event_group, WIFI_STOP_REQ_EVT);
}

esp_err_t iot_wifi_connect(const char *ssid, const char *pwd, uint32_t ticks_to_wait)
{
    // Take mutex
    BaseType_t res = xSemaphoreTake(s_wifi_mux, ticks_to_wait);
    RES_ASSERT(TAG, res, ESP_ERR_TIMEOUT);
    // Clear stop event bit
    esp_wifi_disconnect();
    xEventGroupClearBits(s_wifi_event_group, WIFI_STOP_REQ_EVT | WIFI_CONNECTED_EVT);
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    // Connect router
    strcpy((char * )wifi_config.sta.ssid, ssid);
    strcpy((char * )wifi_config.sta.password, pwd);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
    // Wait event bits
    EventBits_t uxBits;
    uxBits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_EVT | WIFI_STOP_REQ_EVT, false, false, ticks_to_wait);
    esp_err_t ret;
    // WiFi connected event
    if (uxBits & WIFI_CONNECTED_EVT) {
        ESP_LOGI(TAG, "WiFi connected");
        ret = ESP_OK;
    }
    // WiFi stop connecting event
    else if (uxBits & WIFI_STOP_REQ_EVT) {
        ESP_LOGI(TAG, "WiFi connecting stop.");
        // Clear stop event bit
        xEventGroupClearBits(s_wifi_event_group, WIFI_STOP_REQ_EVT);
        ret = ESP_FAIL;
    }
    // WiFi connect timeout
    else {
        iot_wifi_disconnect();
        ESP_LOGW(TAG, "WiFi connect fail");
        ret = ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(s_wifi_mux);
    return ret;
}

wifi_sta_status_t iot_wifi_get_status()
{
    return s_wifi_sta_st;
}
