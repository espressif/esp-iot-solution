#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "basic_func_iot.h"
#include "esp_log.h"
#include "wifi.h"

// Debug tag in esp log
static const char* TAG = "wifi";
// Create an event group to handle different WiFi events.
static EventGroupHandle_t wifi_event_group = NULL;
// Mutex to protect WiFi connect
static xSemaphoreHandle wifi_mux = NULL;

// If this is defined as 1, set the log level to DEBUG mode.
#define DEBUG_EN 1

static wifi_sta_status_t wifi_sta_st = WIFI_STATUS_STA_DISCONNECTED;

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            wifi_sta_st = WIFI_STATUS_STA_CONNECTING;
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START\n");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            wifi_sta_st = WIFI_STATUS_STA_CONNECTED;
            ESP_LOGI(TAG, "SYSREM_EVENT_STA_GOT_IP\n");
            // Set event bit to sync with other tasks.
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            wifi_sta_st = WIFI_STATUS_STA_DISCONNECTED;
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED\n");
            esp_wifi_connect();
            // Clear event bit so WiFi task knows the disconnect-event
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVT);
            break;
        default:
            printf("Get default WiFi event: %d\n", event->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t wifi_setup(wifi_mode_t wifi_mode)
{
#if DEBUG_EN
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    if (wifi_mux == NULL) {
        wifi_mux = xSemaphoreCreateMutex();
        pointer_assert(TAG, wifi_mux);
    }
    tcpip_adapter_init();
    // hoop WiFi event handler
    err_assert(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    // Init WiFi
    err_assert(esp_wifi_init(&cfg));
    err_assert(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_wifi_set_mode(wifi_mode);
    esp_wifi_start();
    // Init event group
    wifi_event_group = xEventGroupCreate();
    pointer_assert(TAG, wifi_event_group);
    return ESP_OK;
}

void wifi_connect_stop()
{
    esp_wifi_stop();
    xEventGroupSetBits(wifi_event_group, WIFI_STOP_REQ_EVT);
}

esp_err_t wifi_connect_start(const char *ssid, const char *pwd, uint32_t ticks_to_wait)
{
    // Take mutex
    BaseType_t res = xSemaphoreTake(wifi_mux, ticks_to_wait);
    res_assert(TAG, res, ESP_ERR_TIMEOUT);
    // Clear stop event bit
    xEventGroupClearBits(wifi_event_group, WIFI_STOP_REQ_EVT);
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
    uxBits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVT | WIFI_STOP_REQ_EVT, false, false, ticks_to_wait);
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
        xEventGroupClearBits(wifi_event_group, WIFI_STOP_REQ_EVT);
        ret = ESP_FAIL;
    }
    // WiFi connect timeout
    else {
        esp_wifi_stop();
        ESP_LOGW(TAG, "WiFi connect fail");
        ret = ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(wifi_mux);
    return ret;
}

wifi_sta_status_t wifi_get_status()
{
    return wifi_sta_st;
}