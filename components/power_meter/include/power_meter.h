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

#ifndef _IOT_POWER_METER_H_
#define _IOT_POWER_METER_H_
#include "esp_system.h"
#include "driver/pcnt.h"

typedef enum {
    PM_BOTH_VC = 0,
    PM_SINGLE_CURRENT,
    PM_SINGLE_VOLTAGE,
} pm_mode_t;

typedef enum {
    PM_POWER = 0,
    PM_VOLTAGE,
    PM_CURRENT,
} pm_value_type_t;

typedef struct {
    uint8_t power_io_num;
    pcnt_unit_t power_pcnt_unit;
    uint32_t power_ref_param;
    uint8_t voltage_io_num;
    pcnt_unit_t voltage_pcnt_unit;
    uint32_t voltage_ref_param;
    uint8_t current_io_num;
    pcnt_unit_t current_pcnt_unit;
    uint32_t current_ref_param;
    uint8_t sel_io_num;
    uint8_t sel_level;
    pm_mode_t pm_mode;
} pm_config_t;

typedef void* pm_handle_t;
pm_handle_t powermeter_create(pm_config_t pm_config);
uint32_t powermeter_read(pm_handle_t pm_handle, pm_value_type_t value_type);
esp_err_t powermeter_change_mode(pm_handle_t pm_handle, pm_mode_t mode);
#endif
