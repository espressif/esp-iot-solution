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

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_log.h"
#include "power_meter.h"

#define PM_PCNT_CHANNEL     PCNT_CHANNEL_0
#define PM_PCNT_THRES0     1
#define PM_MAX  2
#define PM_PIN_MAX  3
#define PM_CRITICAL_PERIOD  (5 * 1000)
#define PM_VALUE_MULTIPLE   100
#define PM_PCNT_H_LIM   2
#define PM_PCNT_L_LIM   -1000
#define PM_PCNT_FILTER  100
#define PM_VALUE_INF    (1000 * 1000 * 1000)

#define IOT_CHECK(tag, a, ret)  if(!(a)) {       \
        return (ret);                            \
        }
#define ERR_ASSERT(tag, param)  IOT_CHECK(tag, (param) == ESP_OK, ESP_FAIL)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)

typedef struct {
    pcnt_unit_t pcnt_unit;
    uint32_t last_time;
    uint32_t time_diff;
    uint32_t ref_param;
    TimerHandle_t xTimer;
} pm_pin_t;

typedef struct {
    pm_pin_t* pm_pin[PM_PIN_MAX];
    uint8_t sel_io_num;
    uint8_t sel_level;
    pm_mode_t pm_mode;
} pm_dev_t;

// Debug tag in esp log
static const char* TAG = "power meter";
static pm_dev_t* g_pm_group[PM_MAX];
static uint8_t g_pm_num;

static void pm_timer_callback(TimerHandle_t xTimer)
{
    pm_dev_t* pm_dev;
    pm_pin_t* pm_pin;
    for (int i = 0; i < g_pm_num; i++) {
        pm_dev = g_pm_group[i];
        for (int j = 0; j < PM_PIN_MAX; j++) {
            if (pm_dev->pm_pin[j] != NULL) {
                pm_pin = pm_dev->pm_pin[j];
                if (pm_pin->xTimer == xTimer) {
                    pm_pin->time_diff = PM_VALUE_INF;
                    pcnt_counter_clear(pm_pin->pcnt_unit);
                    pcnt_counter_resume(pm_pin->pcnt_unit);
                }
            }
        }
    }
}

static void pm_pcnt_intr_handler(void *arg)
{
    uint32_t intr_status = PCNT.int_st.val;
    PCNT.int_clr.val = 0xff;
    pm_dev_t* pm_dev;
    pm_pin_t* pm_pin;
    for(uint8_t i = 0; i < PM_MAX; i++) {
        if (g_pm_group[i] != NULL) {
            pm_dev = g_pm_group[i];
            for(uint8_t j = 0; j < PM_PIN_MAX; j++) {
                pm_pin = pm_dev->pm_pin[j];
                if (pm_pin != NULL && (BIT(pm_pin->pcnt_unit) & intr_status)) {
                    if (PCNT.status_unit[pm_pin->pcnt_unit].h_lim_lat) {
                        pcnt_counter_clear(pm_pin->pcnt_unit);
                        pcnt_counter_resume(pm_pin->pcnt_unit);
                        uint32_t time = system_get_time();
                        pm_pin->time_diff = (time - pm_pin->last_time) / (PM_PCNT_H_LIM - 1);
                        pm_pin->last_time = time;
                        xTimerResetFromISR(pm_pin->xTimer, pdFALSE);
                    }
                    if (PCNT.status_unit[pm_pin->pcnt_unit].thres0_lat) {
                        pm_pin->last_time = system_get_time();
                    }
                }
            }
        }
    }   
}

static pm_pin_t* powermeter_pin_create(uint8_t io_num, pcnt_unit_t pcnt_unit, uint32_t ref_param)
{
    pcnt_config_t pcnt_config = {
        .pulse_gpio_num = io_num,
        .ctrl_gpio_num = -1,
        .channel = PM_PCNT_CHANNEL,
        .unit = pcnt_unit,
        .pos_mode = PCNT_COUNT_INC,
        .neg_mode = PCNT_COUNT_DIS,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .counter_h_lim = PM_PCNT_H_LIM,
        .counter_l_lim = PM_PCNT_L_LIM,
    };
    pcnt_unit_config(&pcnt_config);
    pcnt_set_filter_value(pcnt_unit, PM_PCNT_FILTER);
    pcnt_filter_enable(pcnt_unit);
    pcnt_event_enable(pcnt_unit, PCNT_EVT_H_LIM);
    pcnt_set_event_value(pcnt_unit, PCNT_EVT_THRES_0, PM_PCNT_THRES0);
    pcnt_event_enable(pcnt_unit, PCNT_EVT_THRES_0);
    pcnt_counter_pause(pcnt_unit);
    pcnt_counter_clear(pcnt_unit);
    pcnt_isr_register(pm_pcnt_intr_handler, NULL, 0, NULL);
    pcnt_intr_enable(pcnt_unit);
    pcnt_counter_resume(pcnt_unit);

    pm_pin_t* pm_pin = (pm_pin_t*) calloc(1, sizeof(pm_pin_t));
    pm_pin->last_time = 0;
    pm_pin->pcnt_unit = pcnt_unit;
    pm_pin->ref_param = ref_param;
    pm_pin->time_diff = PM_VALUE_INF;
    pm_pin->xTimer = xTimerCreate("pm_timer", PM_CRITICAL_PERIOD / portTICK_RATE_MS, pdFALSE, (void*) 0, pm_timer_callback);
    return pm_pin;
}

static esp_err_t powermeter_pin_delete(pm_pin_t* pm_pin)
{
    POINT_ASSERT(TAG, pm_pin);
    xTimerDelete(pm_pin->xTimer, portMAX_DELAY);
    free(pm_pin);
    return ESP_OK;
}

pm_handle_t powermeter_create(pm_config_t pm_config)
{
    pm_dev_t* pm_dev = (pm_dev_t*)calloc(1, sizeof(pm_dev_t));
    for(int i = 0; i < PM_PIN_MAX; i++) {
        pm_dev->pm_pin[i] = NULL;
    }
    if (pm_config.power_io_num != 0xff) {
        pm_dev->pm_pin[PM_POWER] = powermeter_pin_create(pm_config.power_io_num, pm_config.power_pcnt_unit, pm_config.power_ref_param);
    }
    if (pm_config.voltage_io_num != 0xff) {
        pm_dev->pm_pin[PM_VOLTAGE] = powermeter_pin_create(pm_config.voltage_io_num, pm_config.voltage_pcnt_unit, pm_config.voltage_ref_param);
    }
    if (pm_config.current_io_num != 0xff) {
        pm_dev->pm_pin[PM_CURRENT] = powermeter_pin_create(pm_config.current_io_num, pm_config.current_pcnt_unit, pm_config.current_ref_param);
    }
    pm_dev->sel_io_num = pm_config.sel_io_num;
    pm_dev->sel_level = pm_config.sel_level;
    if (pm_config.sel_io_num < GPIO_PIN_COUNT) {
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT(pm_dev->sel_io_num);
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        gpio_set_level(pm_dev->sel_io_num, pm_config.sel_level);
    }
    pm_dev->pm_mode = pm_config.pm_mode;
    if (g_pm_num >= PM_MAX) {
        return NULL;
    }
    g_pm_num++;
    for (int i = 0; i < PM_MAX; i++) {
        if (g_pm_group[i] == NULL) {
            g_pm_group[i] = pm_dev;
            break;
        }
    }
            
    return (pm_handle_t) pm_dev;
}

esp_err_t powermeter_delete(pm_handle_t pm_handle)
{
    pm_dev_t* pm_dev = (pm_dev_t*) pm_handle;
    POINT_ASSERT(TAG, pm_handle);
    for (int i = 0; i < PM_MAX; i++) {
        if (g_pm_group[i] == pm_dev) {
            g_pm_group[i] = NULL;
            break;
        }
    }
    for (int i = 0; i < PM_PIN_MAX; i++) {
        if (pm_dev->pm_pin[i] != NULL) {
            powermeter_pin_delete(pm_dev->pm_pin[i]);
            pm_dev->pm_pin[i] = NULL;
        }
    }
    free(pm_dev);
    g_pm_num--;
    return ESP_OK;
}

esp_err_t powermeter_change_mode(pm_handle_t pm_handle, pm_mode_t mode)
{
    pm_dev_t* pm_dev = (pm_dev_t*) pm_handle;
    if (mode == PM_BOTH_VC) {
        return ESP_FAIL;
    }
    else if (pm_dev->pm_mode == mode) {
        ESP_LOGI(TAG, "power meter mode:%d", mode);
        return ESP_OK;
    }
    else {
        pm_dev->pm_mode = mode;
        pm_dev->sel_level = (~pm_dev->sel_level) & 0x01;
        IOT_CHECK(TAG, pm_dev->sel_io_num < GPIO_PIN_COUNT, ESP_FAIL);
        gpio_set_level(pm_dev->sel_io_num, pm_dev->sel_level);
        ESP_LOGI(TAG, "power meter mode:%d", mode);
        return ESP_OK;
    }
}

uint32_t powermeter_read(pm_handle_t pm_handle, pm_value_type_t value_type)
{
    pm_dev_t* pm_dev = (pm_dev_t*) pm_handle;
    switch (value_type) {
        case PM_POWER:
            if (pm_dev->pm_pin[PM_POWER] != NULL) {
                if (pm_dev->pm_pin[PM_POWER]->ref_param == 0) {
                    return pm_dev->pm_pin[PM_POWER]->time_diff;
                }
                else {
                    IOT_CHECK(TAG, pm_dev->pm_pin[PM_POWER]->time_diff != 0, PM_VALUE_INF);
                    return PM_VALUE_MULTIPLE * pm_dev->pm_pin[PM_POWER]->ref_param / pm_dev->pm_pin[PM_POWER]->time_diff;
                }
            }
            break;
         case PM_VOLTAGE:
            if (pm_dev->pm_mode == PM_SINGLE_CURRENT) {
                if (pm_dev->pm_pin[PM_POWER] != NULL && pm_dev->pm_pin[PM_CURRENT] != NULL) {
                    IOT_CHECK(TAG, pm_dev->pm_pin[PM_POWER]->time_diff != 0, PM_VALUE_INF);
                    uint32_t power = pm_dev->pm_pin[PM_POWER]->ref_param / pm_dev->pm_pin[PM_POWER]->time_diff;
                    IOT_CHECK(TAG, pm_dev->pm_pin[PM_CURRENT]->time_diff != 0, PM_VALUE_INF);
                    uint32_t current = pm_dev->pm_pin[PM_CURRENT]->ref_param / pm_dev->pm_pin[PM_CURRENT]->time_diff;
                    IOT_CHECK(TAG, power != 0, 0);
                    IOT_CHECK(TAG, current != 0, PM_VALUE_INF);
                    return PM_VALUE_MULTIPLE * power / current;     
                }
            }
            else {
                if (pm_dev->pm_pin[PM_VOLTAGE] != NULL) {
                    if (pm_dev->pm_pin[PM_VOLTAGE]->ref_param == 0) {
                        return pm_dev->pm_pin[PM_VOLTAGE]->time_diff;
                    }
                    else {
                        IOT_CHECK(TAG, pm_dev->pm_pin[PM_VOLTAGE]->time_diff != 0, PM_VALUE_INF);
                        return PM_VALUE_MULTIPLE * pm_dev->pm_pin[PM_VOLTAGE]->ref_param / pm_dev->pm_pin[PM_VOLTAGE]->time_diff;
                    }
                }
            }
            break;
        case PM_CURRENT:
            if (pm_dev->pm_mode == PM_SINGLE_VOLTAGE) {
                if (pm_dev->pm_pin[PM_POWER] != NULL && pm_dev->pm_pin[PM_VOLTAGE] != NULL) {
                    IOT_CHECK(TAG, pm_dev->pm_pin[PM_POWER]->time_diff != 0, PM_VALUE_INF);
                    uint32_t power = pm_dev->pm_pin[PM_POWER]->ref_param / pm_dev->pm_pin[PM_POWER]->time_diff;
                    IOT_CHECK(TAG, pm_dev->pm_pin[PM_VOLTAGE]->time_diff != 0, PM_VALUE_INF);
                    uint32_t voltage = pm_dev->pm_pin[PM_VOLTAGE]->ref_param / pm_dev->pm_pin[PM_VOLTAGE]->time_diff;
                    IOT_CHECK(TAG, power != 0, 0);
                    IOT_CHECK(TAG, voltage != 0, PM_VALUE_INF);
                    return PM_VALUE_MULTIPLE * power / voltage;
                }
            }
            else {
                if (pm_dev->pm_pin[PM_CURRENT] != NULL) {
                    if (pm_dev->pm_pin[PM_CURRENT]->ref_param == 0) {
                        return pm_dev->pm_pin[PM_CURRENT]->time_diff;
                    }
                    else {
                        IOT_CHECK(TAG, pm_dev->pm_pin[PM_CURRENT]->time_diff != 0, PM_VALUE_INF);
                        return PM_VALUE_MULTIPLE * pm_dev->pm_pin[PM_CURRENT]->ref_param / pm_dev->pm_pin[PM_CURRENT]->time_diff;
                    }
                }
            }
            break;
        default:
            break;
    }
    return 0;
}
