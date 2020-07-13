/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "iot_button.h"
#include "iot_smartconfig.h"
#include "iot_param.h"
#include "lowpower_evb_wifi.h"

/* Wifi config start button */
#define WIFI_CONFIG_BUTTON_IO       ((gpio_num_t) 34)
#define BUTTON_ACTIVE_LEVEL         (BUTTON_ACTIVE_LOW)

/* Param save */
#define BATTERY_EB_PARAM  "battery_eb"
#define WIFI_CONFIG_PARAM "wifi_config"

static const char *TAG = "lowpower_evb_wifi";

static button_handle_t btn_handle = NULL;

static void button_tap_cb(void* arg)
{
    ESP_LOGI(TAG, "clear wifi config and restart");
    iot_param_erase(BATTERY_EB_PARAM, WIFI_CONFIG_PARAM);
    esp_restart();
}

void button_init()
{
    btn_handle = iot_button_create(WIFI_CONFIG_BUTTON_IO, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, button_tap_cb, NULL);
}

// static void sc_check_status(void* arg)
// {
//     while(1) {
//         ESP_LOGI(TAG, "sc status: %d", iot_sc_get_status());
//         vTaskDelay(300/portTICK_PERIOD_MS);
//         if(iot_sc_get_status() == SC_STATUS_LINK_OVER) {
//             break;
//         }
//     }
//     vTaskDelete(NULL);
// }

void lowpower_evb_wifi_config()
{
    esp_err_t res;
    iot_sc_setup(SC_TYPE_ESPTOUCH, WIFI_MODE_STA, 0);
    // xTaskCreate(sc_check_status, "sc_check_status", 1024*2, NULL, 5, NULL);
    
    res = iot_sc_start(60*1000 / portTICK_PERIOD_MS);

    if (res == ESP_OK) {
        ESP_LOGI(TAG, "connected");
        wifi_config_t wifi_config;
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        ESP_LOGI(TAG, "save wifi config into flash, SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        iot_param_save(BATTERY_EB_PARAM, WIFI_CONFIG_PARAM, &wifi_config, sizeof(wifi_config_t));
    } else if (res == ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "smart config timeout");
    } else if (res == ESP_FAIL) {
        ESP_LOGE(TAG, "smart config stopped");
    }
}

esp_err_t lowpower_evb_get_wifi_config(wifi_config_t *config)
{
    wifi_config_t wifi_config;
    int ret = iot_param_load(BATTERY_EB_PARAM, WIFI_CONFIG_PARAM, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get wifi config fail, error reason:%d (refer to nvs.h)", ret);
        return ESP_FAIL;
    }
    
    *config = wifi_config;
    return ESP_OK;
}

