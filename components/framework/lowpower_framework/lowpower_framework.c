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
#include "esp_system.h"
#include "esp_log.h"
#include "lowpower_framework.h"
#include "esp_sleep.h"

static const char* TAG = "lowpower_framework";
#define GOTO_EXIT_IF_FAIL(FUNC, LOG, EXIT)  if(FUNC != ESP_OK) {    \
                                                ESP_LOGE(TAG, LOG); \
                                                goto EXIT;          \
                                            }

typedef esp_err_t (*callback_without_param_t)(void);

typedef struct {
    callback_without_param_t init;
    callback_without_param_t network_connect;
    callback_without_param_t collect_data;
    callback_without_param_t ulp_cp_init;
    callback_without_param_t get_ulp_data;
    callback_without_param_t upload;
    callback_without_param_t upload_done;
    callback_without_param_t enter_deep_sleep;
} lowpower_frm_cb_t;

static lowpower_frm_cb_t lowpower_framework_cb;

esp_err_t lowpower_framework_register_callback(lowpower_framework_cb_t cb_type, void *cb_func)
{
    switch (cb_type) {
        case LPFCB_DEVICE_INIT:
            lowpower_framework_cb.init = cb_func;
            break;
        case LPFCB_WIFI_CONNECT:
            lowpower_framework_cb.network_connect = cb_func;
            break;
        case LPFCB_GET_DATA_BY_CPU:
            lowpower_framework_cb.collect_data = cb_func;
            break;
        case LPFCB_ULP_PROGRAM_INIT:
            lowpower_framework_cb.ulp_cp_init = cb_func;
            break;
        case LPFCB_GET_DATA_FROM_ULP:
            lowpower_framework_cb.get_ulp_data = cb_func;
            break;
        case LPFCB_SEND_DATA_TO_SERVER:
            lowpower_framework_cb.upload = cb_func;
            break;
        case LPFCB_SEND_DATA_DONE:
            lowpower_framework_cb.upload_done = cb_func;
            break;
        case LPFCB_START_DEEP_SLEEP:
            lowpower_framework_cb.enter_deep_sleep = cb_func;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t _check_and_run_callback(callback_without_param_t func)
{
    if (func) {
        if ((*func)() != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t fw_device_init()
{
    return _check_and_run_callback(lowpower_framework_cb.init);
}

static esp_err_t fw_network_connect()
{
    return _check_and_run_callback(lowpower_framework_cb.network_connect);
}

static esp_err_t fw_upload_data_to_server()
{
    return _check_and_run_callback(lowpower_framework_cb.upload);
}

static esp_err_t fw_upload_data_to_server_done()
{
    return _check_and_run_callback(lowpower_framework_cb.upload_done);
}

static esp_err_t fw_enter_deep_sleep()
{
    return _check_and_run_callback(lowpower_framework_cb.enter_deep_sleep);
}

static esp_err_t fw_read_ulp_data_from_rtc_memory()
{
    return _check_and_run_callback(lowpower_framework_cb.get_ulp_data);
}

static esp_err_t fw_ulp_coprocessor_init()
{
    return _check_and_run_callback(lowpower_framework_cb.ulp_cp_init);
}

static esp_err_t fw_collect_data_by_cpu()
{
    return _check_and_run_callback(lowpower_framework_cb.collect_data);
}

void lowpower_framework_start(void)
{
    GOTO_EXIT_IF_FAIL(fw_device_init(), "Board init error", EXIT_PORT_DEEP_SLEEP);
    GOTO_EXIT_IF_FAIL(fw_network_connect(), "Board network error", EXIT_PORT_DEEP_SLEEP);

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "wake up cause:%d", cause);

    if (cause == ESP_SLEEP_WAKEUP_ULP) {
        /* If wakeup cause is ULP wakeup, we will read data from ULP. */
        GOTO_EXIT_IF_FAIL(fw_read_ulp_data_from_rtc_memory(), "Board read ulp data error", EXIT_PORT_DEEP_SLEEP);
    } else {
        ESP_LOGI(TAG, "Not ULP wakeup");
        GOTO_EXIT_IF_FAIL(fw_ulp_coprocessor_init(), "Board ulp coprocessor init error", EXIT_PORT_DEEP_SLEEP);

        /* If wakeup cause is not defined, we should go to start deep sleep callback, */
        /* which will define wakeup cause and start deepsleep. */
        if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
            goto EXIT_PORT_DEEP_SLEEP;
        }

        /* If wakeup cause is defined but not ULP wakeup, we will use cpu to read sensor data. */
        GOTO_EXIT_IF_FAIL(fw_collect_data_by_cpu(), "Board collect data by CPU error", EXIT_PORT_DEEP_SLEEP);
    }

    GOTO_EXIT_IF_FAIL(fw_upload_data_to_server(), "Board upload data error", EXIT_PORT_DEEP_SLEEP);
    GOTO_EXIT_IF_FAIL(fw_upload_data_to_server_done(), "Board upload data done error", EXIT_PORT_DEEP_SLEEP);

EXIT_PORT_DEEP_SLEEP:
    GOTO_EXIT_IF_FAIL(fw_enter_deep_sleep(), "Board enter deep sleep error", EXIT_PORT_DEEP_SLEEP);
}

