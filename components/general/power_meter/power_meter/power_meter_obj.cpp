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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_power_meter.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif
CPowerMeter::CPowerMeter(const pm_config_t &pm_config)
{
    m_pm_handle = iot_powermeter_create(pm_config);
}

CPowerMeter::~CPowerMeter()
{
    iot_powermeter_delete(m_pm_handle);
    m_pm_handle = NULL;
}

uint32_t CPowerMeter::read(pm_value_type_t value_type)
{
    return iot_powermeter_read(m_pm_handle, value_type);
}

esp_err_t CPowerMeter::change_mode(pm_mode_t mode)
{
    return iot_powermeter_change_mode(m_pm_handle, mode);
}
#ifdef __cplusplus
}
#endif
