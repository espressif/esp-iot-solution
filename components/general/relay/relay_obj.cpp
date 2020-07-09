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
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "iot_relay.h"

CRelay::CRelay(relay_io_t relay_io, relay_close_level_t close_level, relay_ctl_mode_t ctl_mode, relay_io_mode_t io_mode)
{
    m_relay_handle = iot_relay_create(relay_io, close_level, ctl_mode, io_mode);
}

CRelay::~CRelay()
{
    iot_relay_delete(m_relay_handle);
    m_relay_handle = NULL;
}

esp_err_t CRelay::on()
{
    return iot_relay_state_write(m_relay_handle, RELAY_STATUS_CLOSE);
}

esp_err_t CRelay::off()
{
    return iot_relay_state_write(m_relay_handle, RELAY_STATUS_OPEN);
}

relay_status_t CRelay::status()
{
    return iot_relay_state_read(m_relay_handle);
}
