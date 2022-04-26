// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "modem_board.h"
#include "modem_wifi.h"
#include "modem_http_config.h"

#ifdef CONFIG_CDC_USE_TRACE_FACILITY
#include "esp_usbh_cdc.h"
#endif

static const char *TAG = "main";

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    /**
     * @brief Basic Settings：ssid, password, if_hide_ssid, auth_mode
     * 
     */
    static char user_ssid[36] = CONFIG_EXAMPLE_WIFI_SSID;
    static char user_password[36] = CONFIG_EXAMPLE_WIFI_PASSWORD;
    static char user_hide_ssid[8] = "false";
    static char user_auth_mode[16] = "WPA_WPA2_PSK";
    static size_t user_ssid_size = sizeof(user_ssid);
    static size_t user_password_size = sizeof(user_password);
    static size_t user_hide_ssid_size = sizeof(user_hide_ssid);
    static size_t user_auth_mode_size = sizeof(user_auth_mode);
    /**
     * @brief Advanced Settings: Bandwidth, Signal Channel
     * 
     */
    static char user_channel[4] = "";
    static size_t user_channel_size = sizeof(user_channel);
    #ifdef CONFIG_WIFI_BANDWIFTH_20
    static char user_bandwidth[4] = "20";
    #else 
    static char user_bandwidth[4] = "40";
    #endif
    static size_t user_bandwidth_size = sizeof(user_bandwidth);
#endif

#ifdef CONFIG_DUMP_SYSTEM_STATUS
#define TASK_MAX_COUNT 32
typedef struct {
    uint32_t ulRunTimeCounter;
    uint32_t xTaskNumber;
} taskData_t;

static taskData_t previousSnapshot[TASK_MAX_COUNT];
static int taskTopIndex = 0;
static uint32_t previousTotalRunTime = 0;

static taskData_t *getPreviousTaskData(uint32_t xTaskNumber)
{
    // Try to find the task in the list of tasks
    for (int i = 0; i < taskTopIndex; i++) {
        if (previousSnapshot[i].xTaskNumber == xTaskNumber) {
            return &previousSnapshot[i];
        }
    }
    // Allocate a new entry
    ESP_ERROR_CHECK(!(taskTopIndex < TASK_MAX_COUNT));
    taskData_t *result = &previousSnapshot[taskTopIndex];
    result->xTaskNumber = xTaskNumber;
    taskTopIndex++;
    return result;   
}

static void _system_dump()
{
    uint32_t totalRunTime;

    TaskStatus_t taskStats[TASK_MAX_COUNT];
    uint32_t taskCount = uxTaskGetSystemState(taskStats, TASK_MAX_COUNT, &totalRunTime);
    ESP_ERROR_CHECK(!(taskTopIndex < TASK_MAX_COUNT));
    uint32_t totalDelta = totalRunTime - previousTotalRunTime;
    float f = 100.0 / totalDelta;
    // Dumps the the CPU load and stack usage for all tasks
    // CPU usage is since last dump in % compared to total time spent in tasks. Note that time spent in interrupts will be included in measured time.
    // Stack usage is displayed as nr of unused bytes at peak stack usage.

    ESP_LOGI(TAG,"Task dump\n");
    ESP_LOGI(TAG,"Load\tStack left\tName\tPRI\n");

    for (uint32_t i = 0; i < taskCount; i++) {
        TaskStatus_t *stats = &taskStats[i];
        taskData_t *previousTaskData = getPreviousTaskData(stats->xTaskNumber);

        uint32_t taskRunTime = stats->ulRunTimeCounter;
        float load = f * (taskRunTime - previousTaskData->ulRunTimeCounter);
        ESP_LOGI(TAG,"%.2f \t%u \t%s \t%u\n", load, stats->usStackHighWaterMark, stats->pcTaskName, stats->uxBasePriority);

        previousTaskData->ulRunTimeCounter = taskRunTime;
    }
    ESP_LOGI(TAG, "Free heap=%d Free mini=%d bigst=%d, internal=%d bigst=%d",
                        heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),
                        heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
                        heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL), 
                        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    previousTotalRunTime = totalRunTime;
#ifdef CONFIG_CDC_USE_TRACE_FACILITY
    ESP_LOGI(TAG,"..............\n");
    usbh_cdc_print_buffer_msg();
#endif
}
#endif

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
static void nvs_get_str_log(esp_err_t err,char *key,char *value){
    switch (err) {
    case ESP_OK:
        printf("%s = %s\n",key, value);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("%s is not initialized yet!\n",key);
        break;
    default :
        printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
}

static esp_err_t init_nvs(void){
    nvs_handle_t my_handle; 
    esp_err_t err = nvs_open("memory", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return ESP_FAIL;
    } else {
        printf("Reading restart counter from NVS ... ");
        //int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_str(my_handle, "user_ssid", user_ssid,&user_ssid_size);
        nvs_get_str_log(err,"user_ssid",user_ssid);
        err = nvs_get_str(my_handle, "user_password",user_password,&user_password_size);
        nvs_get_str_log(err,"user_password",user_password);
        err = nvs_get_str(my_handle, "user_hide_ssid",user_hide_ssid,&user_hide_ssid_size);
        nvs_get_str_log(err,"user_hide_ssid",user_hide_ssid);
        err = nvs_get_str(my_handle, "user_auth_mode",user_auth_mode,&user_auth_mode_size);
        nvs_get_str_log(err,"user_auth_mode",user_auth_mode);
        err = nvs_get_str(my_handle, "user_channel",user_channel,&user_channel_size);
        nvs_get_str_log(err,"user_channel",user_channel);
        err = nvs_get_str(my_handle, "user_bandwidth",user_bandwidth,&user_bandwidth_size);
        nvs_get_str_log(err,"user_bandwidth",user_bandwidth);
        nvs_close(my_handle);
    }
    return ESP_OK;
}
#endif


void app_main(void)
{
    ESP_LOGW(TAG, "Force reset 4g board");
    gpio_config_t io_config = {
            .pin_bit_mask = BIT64(MODEM_RESET_GPIO),
            .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_config);
    gpio_set_level(MODEM_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(MODEM_RESET_GPIO, 1);

    /* Initialize NVS for Wi-Fi storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated and needs to be erased
         * Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize default TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Waitting for modem powerup */
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "     ESP 4G Cat.1 Wi-Fi Router");
    ESP_LOGI(TAG, "====================================");

    /* Initialize modem board. Dial-up internet */
    modem_config_t modem_config = MODEM_DEFAULT_CONFIG();
    esp_netif_t *ppp_netif = modem_board_init(&modem_config);
    assert(ppp_netif != NULL);

#ifdef CONFIG_EXAMPLE_ENABLE_WEB_ROUTER
    sprintf(user_channel,"%d",CONFIG_EXAMPLE_WIFI_CHANNEL);
    init_nvs();
    modem_http_config_init();
    modem_wifi_config_t modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();
    modem_wifi_config.max_connection = CONFIG_EXAMPLE_MAX_STA_CONN;
    modem_wifi_config.ssid = user_ssid;
    modem_wifi_config.password = user_password;
    modem_wifi_config.max_connection = 5;
    if ( !strcmp(user_auth_mode,"OPEN") )
        modem_wifi_config.authmode = WIFI_AUTH_OPEN;
    else if ( !strcmp(user_auth_mode,"WEP") )
        modem_wifi_config.authmode = WIFI_AUTH_WEP;
    else if ( !strcmp(user_auth_mode,"WPA2_PSK") )
        modem_wifi_config.authmode = WIFI_AUTH_WPA2_PSK;
    else if ( !strcmp(user_auth_mode,"WPA_WPA2_PSK") )
        modem_wifi_config.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    else
        ESP_LOGE(TAG,"User_auth_mode %s is not define",user_auth_mode);
    modem_wifi_config.channel = atoi(user_channel);
    if (strcmp(user_hide_ssid, "false") !=0 ){
        modem_wifi_config.ssid_hidden = 1;
    }
    if(!strcmp(user_bandwidth,"20")){
        modem_wifi_config.bandwidth=WIFI_BW_HT20;
    }else{
        modem_wifi_config.bandwidth=WIFI_BW_HT40;
    }
#else
    /* Initialize Wi-Fi. Start Wi-Fi AP */
    modem_wifi_config_t modem_wifi_config = MODEM_WIFI_DEFAULT_CONFIG();
    modem_wifi_config.max_connection = CONFIG_EXAMPLE_MAX_STA_CONN;
    modem_wifi_config.channel = CONFIG_EXAMPLE_WIFI_CHANNEL;
    modem_wifi_config.password = CONFIG_EXAMPLE_WIFI_PASSWORD;
    modem_wifi_config.ssid = CONFIG_EXAMPLE_WIFI_SSID;
    #ifdef CONFIG_WIFI_BANDWIFTH_20
        modem_wifi_config.bandwidth = WIFI_BW_HT20;
    #else
        modem_wifi_config.bandwidth = WIFI_BW_HT40;
    #endif
#endif    
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif != NULL);
#if CONFIG_EXAMPLE_MANUAL_DNS
    ESP_ERROR_CHECK(modem_wifi_set_dhcps(ap_netif, inet_addr(CONFIG_EXAMPLE_MANUAL_DNS_ADDR)));
    ESP_LOGI(TAG, "ap dns addr(manual): %s", CONFIG_EXAMPLE_MANUAL_DNS_ADDR);
#else /* using dns from ppp */
    esp_netif_dns_info_t dns;
    ESP_ERROR_CHECK(esp_netif_get_dns_info(ppp_netif, ESP_NETIF_DNS_MAIN, &dns));
    ESP_ERROR_CHECK(modem_wifi_set_dhcps(ap_netif, dns.ip.u_addr.ip4.addr));
    ESP_LOGI(TAG, "ap dns addr(auto): " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
#endif
    ESP_ERROR_CHECK(modem_wifi_ap_init());
    ESP_ERROR_CHECK(modem_wifi_set(&modem_wifi_config));
    ESP_ERROR_CHECK(modem_wifi_napt_enable());

    /* Dump system status */
#ifdef CONFIG_DUMP_SYSTEM_STATUS
    while (1) {
        _system_dump();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
#endif
}
