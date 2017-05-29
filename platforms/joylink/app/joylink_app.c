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



/****************************************************************************
*
* This file is for gatt client. It can scan ble device, connect one device,
*
****************************************************************************/
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "joylink_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "jd_innet.h"
#include "button.h"
#include "driver/gpio.h"

extern int joylink_main_start();
extern void joylink_dev_clear_jlp_info(void);

#if JOYLINK_BLE_ENABLE
extern void joylink_ble_init(void);
extern esp_err_t event_handler(void *ctx, system_event_t *event);
extern void joylink_gatts_adv_data_enable(void);
#endif

static void joylink_task(void *pvParameters)
{
    nvs_handle out_handle;
    wifi_config_t config;
    size_t size = 0;
    bool flag = false;
    if (nvs_open("joylink_wifi", NVS_READONLY, &out_handle) == ESP_OK) {
        memset(&config,0x0,sizeof(config));
        size = sizeof(config.sta.ssid);
        if (nvs_get_str(out_handle,"ssid",(char*)config.sta.ssid,&size) == ESP_OK) {
            if (size > 0) {
                size = sizeof(config.sta.password);
                if (nvs_get_str(out_handle,"password",(char*)config.sta.password,&size) == ESP_OK) {
                    flag = true;
                } else {
                    printf("--get password fail");
                }
            }
        } else {
            printf("--get ssid fail");
        }

        nvs_close(out_handle);
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
   if (flag) {
       esp_wifi_set_config(ESP_IF_WIFI_STA,&config);
       esp_wifi_connect();
   }
    joylink_main_start();

    vTaskDelete(NULL);
}
#if ( JOYLINK_BLE_ENABLE == 0 )
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        // esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        break;
    default:
        break;
    }
    return ESP_OK;
}
#endif
static void initialise_wifi(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void joylink_wifi_clear_info(void)
{
    nvs_handle out_handle;
    if (nvs_open("joylink_wifi", NVS_READWRITE, &out_handle) == ESP_OK) {
        nvs_erase_all(out_handle);
        nvs_close(out_handle);
    }
}

static void joylink_button_smnt_tap_cb(void* arg)
{
    joylink_wifi_clear_info();
    jd_innet_start_task();
}

static void joylink_button_reset_tap_cb(void* arg)
{
    joylink_wifi_clear_info();
    joylink_dev_clear_jlp_info();
    esp_restart();
}

#if JOYLINK_BLE_BUTTON_ENABLE
static void joylink_button_ble_tap_cb(void* arg)
{
    joylink_gatts_adv_data_enable();
}
#endif

static void initialise_key(void)
{
    button_handle_t btn_handle = button_dev_init(JOYLINK_SMNT_BUTTON_NUM, 0, BUTTON_ACTIVE_LOW);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, joylink_button_smnt_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS, btn_handle);

    btn_handle = button_dev_init(JOYLINK_RESET_BUTTON_NUM, 0, BUTTON_ACTIVE_LOW);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, joylink_button_reset_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS, btn_handle);

#if JOYLINK_BLE_BUTTON_ENABLE
    btn_handle = button_dev_init(CONFIG_JOYLINK_BLE_BUTTON_NUM, 0, BUTTON_ACTIVE_LOW);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, joylink_button_ble_tap_cb, "PUSH", 50 / portTICK_PERIOD_MS, btn_handle);
#endif
}

void joylink_app_start(void)
{
    nvs_flash_init();
    initialise_wifi();
#if JOYLINK_BLE_ENABLE
    joylink_ble_init();
#endif
    initialise_key();
    xTaskCreate(joylink_task, "jl_task", 1024*10, NULL, 2, NULL);
}

