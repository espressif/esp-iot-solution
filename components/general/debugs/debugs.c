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
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "iot_debugs.h"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {                                 \
        ESP_LOGE(tag,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret);                                                      \
        }
#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINTER_ASSERT(tag, param)  IOT_CHECK((tag), (param) != NULL, ESP_FAIL)

#define DEBUG_STACK_DEPTH       2048
#define DEBUG_TASK_PRIORITY     5
#define DEBUG_POLLING_PERIOD    20
#define DEBUG_BUFF_SIZE         128
#define DEBUG_CMD_INIT_NUM      5

static const char* TAG = "debug_module";
static debug_cmd_info_t *g_debug_cmds = NULL;
static int g_debug_cmd_num = DEBUG_CMD_INIT_NUM;
static uart_port_t g_uart_num = UART_NUM_0;

static void wifi_info_log(void *arg)
{
    wifi_mode_t mode;
    wifi_config_t config;
    if (ESP_ERR_WIFI_NOT_INIT == esp_wifi_get_mode(&mode)) {
        ESP_LOGI(TAG, "wifi is not initialized");
        return;
    }
    ESP_LOGI(TAG, "******************************");
    switch (mode) {
        case WIFI_MODE_STA:
            ESP_LOGI(TAG, "wifi mode: station");
            esp_wifi_get_config(ESP_IF_WIFI_STA, &config);
            ESP_LOGI(TAG, "ssid: %s, password: %s", config.sta.ssid, config.sta.password);
            break;
        case WIFI_MODE_AP:
            ESP_LOGI(TAG, "wifi mode: soft-AP");
            esp_wifi_get_config(ESP_IF_WIFI_AP, &config);
            ESP_LOGI(TAG, "ssid: %s, password: %s", config.ap.ssid, config.ap.password);
            break;
        case WIFI_MODE_APSTA:
            ESP_LOGI(TAG, "wifi mode: station + soft-AP");
            esp_wifi_get_config(ESP_IF_WIFI_STA, &config);
            ESP_LOGI(TAG, "station mode ssid: %s, station mode password: %s", config.sta.ssid, config.sta.password);
            esp_wifi_get_config(ESP_IF_WIFI_AP, &config);
            ESP_LOGI(TAG, "soft-AP ssid: %s, soft-AP password: %s", config.ap.ssid, config.ap.password);
            break;
        default:
            break;
    }
    ESP_LOGI(TAG, "******************************");
}

static const char* wakeup_info[] = {
    "wakeup not caused by deep sleep",
    "wakeup caused by ext0",
    "wakeup caused by ext1",
    "wakeup caused by timer",
    "wakeup caused by touchpad",
    "wakeup caused by ulp"
};

static void wakeup_info_log(void *arg)
{
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "wakeup reason: %s", wakeup_info[wakeup_cause]);
}

static void debug_restart(void *arg)
{
    ESP_LOGI(TAG, "going to restart in 1 second...");
    vTaskDelay(1000 / portTICK_RATE_MS);
    esp_restart();
}

static void debug_task(void *arg)
{
    uart_port_t uart_num = (uart_port_t) arg;
    char data[DEBUG_BUFF_SIZE];
    while (1) {
        int len = uart_read_bytes(uart_num, (uint8_t*) data, DEBUG_BUFF_SIZE, DEBUG_POLLING_PERIOD / portTICK_RATE_MS);
        if (len > 0) {
            printf("==============================\n");
            printf("RECEIVE len %d string %s\n", len, data);
            printf("==============================\n");
            for (int i = 0; i < g_debug_cmd_num; i++) {
                debug_cmd_info_t *cmd_info = &g_debug_cmds[i];
                if (cmd_info->cmd != NULL && strcmp(cmd_info->cmd, data) == 0 && cmd_info->cb != NULL) {
                    cmd_info->cb(cmd_info->arg);
                    break;
                }
            }
            memset(data, 0, DEBUG_BUFF_SIZE);
        }
    }
}

esp_err_t iot_debug_init(uart_port_t uart_num, int baud_rate, int tx_io, int rx_io)
{
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };
    ERR_ASSERT(TAG, uart_param_config(uart_num, &uart_config));
    ERR_ASSERT(TAG, uart_set_pin(uart_num, tx_io, rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ERR_ASSERT(TAG, uart_driver_install(uart_num, DEBUG_BUFF_SIZE * 2, 0, 0, NULL, 0));
    g_debug_cmds = (debug_cmd_info_t*) calloc(DEBUG_CMD_INIT_NUM, sizeof(debug_cmd_info_t));
    g_debug_cmd_num = DEBUG_CMD_INIT_NUM;
    g_uart_num = uart_num;
    POINTER_ASSERT(TAG, g_debug_cmds);
    xTaskCreate(debug_task, "debug_task", DEBUG_STACK_DEPTH, (void* )uart_num, DEBUG_TASK_PRIORITY, NULL);
    return ESP_OK;
}

esp_err_t iot_debug_add_custom_cmd(const char *cmd, debug_cb_t cb, void *arg)
{
    POINTER_ASSERT(TAG, cmd);
    POINTER_ASSERT(TAG, cb);
    int i = 0;
    for (i = 0; i < g_debug_cmd_num; i++) {
        if (g_debug_cmds[i].cmd == NULL) {
            debug_cmd_info_t *cmd_info = &g_debug_cmds[i];
            cmd_info->cmd = (char*) calloc(1, strlen(cmd));
            POINTER_ASSERT(TAG, cmd_info->cmd);
            strcpy(cmd_info->cmd, cmd);
            cmd_info->cb = cb;
            cmd_info->arg = arg;
            return ESP_OK;
        } else if (strcmp(g_debug_cmds[i].cmd, cmd) == 0) {
            ESP_LOGW(TAG, "the cmd:%s has been added!", cmd);
            return ESP_FAIL;
        }
    }
    g_debug_cmd_num += DEBUG_CMD_INIT_NUM;
    debug_cmd_info_t *cmds = (debug_cmd_info_t*) calloc(g_debug_cmd_num, sizeof(debug_cmd_info_t));
    memcpy(cmds, g_debug_cmds, g_debug_cmd_num-DEBUG_CMD_INIT_NUM);
    free(g_debug_cmds);
    g_debug_cmds = cmds;
    debug_cmd_info_t *cmd_info = &g_debug_cmds[i];
    cmd_info->cmd = (char*) calloc(1, strlen(cmd));
    POINTER_ASSERT(TAG, cmd_info->cmd);
    strcpy(cmd_info->cmd, cmd);
    cmd_info->cb = cb;
    cmd_info->arg = arg;
    return ESP_OK;
}

esp_err_t iot_debug_add_cmd(const char *cmd, debug_cmd_type_t cmd_type)
{
    POINTER_ASSERT(TAG, cmd);
    switch (cmd_type) {
        case DEBUG_CMD_WIFI_INFO:
            iot_debug_add_custom_cmd(cmd, wifi_info_log, NULL);
            break;
        case DEBUG_CMD_WAKEUP_INFO:
            iot_debug_add_custom_cmd(cmd, wakeup_info_log, NULL);
            break;
        case DEBUG_CMD_RESTART:
            iot_debug_add_custom_cmd(cmd, debug_restart, NULL);
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t iot_debug_add_cmd_group(debug_cmd_info_t debug_cmds[], int len)
{
    if (len <= 0) {
        return ESP_FAIL;
    }
    for (int i = 0; i < len; i++) {
        debug_cmd_info_t cmd_info = debug_cmds[i];
        iot_debug_add_custom_cmd(cmd_info.cmd, cmd_info.cb, cmd_info.arg);
    }
    return ESP_OK;
}
