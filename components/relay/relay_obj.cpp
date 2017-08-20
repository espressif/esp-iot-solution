/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "relay.h"

CRelay::CRelay(relay_io_t relay_io, relay_close_level_t close_level, relay_ctl_mode_t ctl_mode, relay_io_mode_t io_mode)
{
    m_relay_handle = relay_create(relay_io, close_level, ctl_mode, io_mode);
}

CRelay::~CRelay()
{
    relay_delete(m_relay_handle);
    m_relay_handle = NULL;
}

esp_err_t CRelay::on()
{
    return relay_state_write(m_relay_handle, RELAY_STATUS_CLOSE);
}

esp_err_t CRelay::off()
{
    return relay_state_write(m_relay_handle, RELAY_STATUS_OPEN);
}

relay_status_t CRelay::status()
{
    return relay_state_read(m_relay_handle);
}
