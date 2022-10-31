// Copyright 2022-2024 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include <stdio.h>
#include "driver/touch_pad.h"

#define TOUCH_PROXIMITY_NUM_MAX    SOC_TOUCH_PROXIMITY_CHANNEL_NUM

typedef enum {
    PROXI_EVT_INACTIVE = 0,
    PROXI_EVT_ACTIVE,
} proxi_evt_t;

typedef struct {
    uint32_t channel_num;
    uint32_t channel_list[TOUCH_PROXIMITY_NUM_MAX];
    uint32_t response_ms;
    float smooth_coef;
    float baseline_coef;
    float max_p;
    float min_n;
    float threshold_p[TOUCH_PROXIMITY_NUM_MAX];
    float threshold_n[TOUCH_PROXIMITY_NUM_MAX];
    float hysteresis_p;
    float noise_p;
    float noise_n;
    uint32_t debounce_p;
    uint32_t debounce_n;
    uint32_t reset_p;
    uint32_t reset_n;
    uint32_t gold_value[TOUCH_PROXIMITY_NUM_MAX];
} proxi_config_t;

#define DEFAULTS_PROX_CONFIGS()\
{\
    .response_ms = 100,\
    .smooth_coef = 0.2,\
    .baseline_coef = 0.05,\
    .max_p = 0.2,\
    .min_n = 0.08,\
    .hysteresis_p = 0.05,\
    .noise_p = 0.001,\
    .noise_n = 0.001,\
    .debounce_p = 2,\
    .debounce_n = 1,\
    .reset_p = 1000,\
    .reset_n = 3,\
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * proximity sensor user callback type
 */
typedef void(*proxi_cb_t)(uint32_t channel, proxi_evt_t event, void *cb_arg);

/**
 * Config and start touch proximity sense, will detach user callback if events trigger
 *
 * @param config The touch pad channel configuration.
 * @param cb callback function
 * @param cb_arg The callback function argument.
 */
esp_err_t touch_proximity_sense_start(proxi_config_t *config, proxi_cb_t cb, void *cb_arg);

/**
 * Stop touch proximity sense, resource will be released
 */
esp_err_t touch_proximity_sense_stop(void);

#ifdef __cplusplus
}
#endif