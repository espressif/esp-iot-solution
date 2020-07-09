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
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_led.h"

#define QUICK_BLINK_FREQ_CPP     CONFIG_STATUS_LED_QUICK_BLINK_FREQ   /*!< default 5 */
#define SLOW_BLINK_FREQ_CPP      CONFIG_STATUS_LED_SLOW_BLINK_FREQ    /*!< default 1 */

static const char* LED_CPP_TAG = "LED_CPP";
uint64_t CLED::m_quick_blink_freq = 0;
uint64_t CLED::m_slow_blink_freq = 0;

CLED::CLED(uint8_t io_num, led_dark_level_t dark_level)
{
    if (m_quick_blink_freq == 0 && m_slow_blink_freq == 0) {
        m_quick_blink_freq = QUICK_BLINK_FREQ_CPP;
        m_slow_blink_freq = SLOW_BLINK_FREQ_CPP;
        ESP_LOGI(LED_CPP_TAG, "setup led\n");
        iot_led_setup();
    }
    m_led_handle = iot_led_create(io_num, dark_level);
}

CLED::~CLED()
{
    iot_led_delete(m_led_handle);
    m_led_handle = NULL;
}

esp_err_t CLED::toggle()
{
    if (state_read() == LED_OFF) {
        return on();
    } else {
        return off();
    }
}

esp_err_t CLED::on()
{
    return iot_led_state_write(m_led_handle, LED_ON);
}

esp_err_t CLED::off()
{
    return iot_led_state_write(m_led_handle, LED_OFF);
}

esp_err_t CLED::state_write(led_status_t state)
{
    return iot_led_state_write(m_led_handle, state);
}

led_status_t CLED::state_read()
{
    return iot_led_state_read(m_led_handle);
}

esp_err_t CLED::mode_write(led_mode_t mode)
{
    return iot_led_mode_write(m_led_handle, mode);
}

led_mode_t CLED::mode_read()
{
    return iot_led_mode_read(m_led_handle);
}

esp_err_t CLED::quick_blink()
{
    return iot_led_state_write(m_led_handle, LED_QUICK_BLINK);
}

esp_err_t CLED::slow_blink()
{
    return iot_led_state_write(m_led_handle, LED_SLOW_BLINK);
}

esp_err_t CLED::night_mode()
{
    return mode_write(LED_NIGHT_MODE);
}

esp_err_t CLED::normal_mode()
{
    return mode_write(LED_NORMAL_MODE);
}

esp_err_t CLED::blink_freq_write(uint64_t quick_fre, uint64_t slow_fre)
{
    m_quick_blink_freq = quick_fre;
    m_slow_blink_freq = slow_fre;
    iot_led_update_blink_freq(m_quick_blink_freq, m_slow_blink_freq);
    return ESP_OK;
}

esp_err_t CLED::night_duty_write(uint8_t duty)
{
    return iot_led_night_duty_write(duty);
}

uint8_t CLED::night_duty_read()
{
    return iot_led_night_duty_read();
}

