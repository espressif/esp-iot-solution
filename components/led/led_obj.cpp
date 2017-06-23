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
#include "sdkconfig.h"

#if CONFIG_STATUS_LED_ENABLE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "led.h"

#define QUICK_BLINK_FREQ_CPP     CONFIG_STATUS_LED_QUICK_BLINK_FREQ   /*!< default 5 */
#define SLOW_BLINK_FREQ_CPP      CONFIG_STATUS_LED_SLOW_BLINK_FREQ    /*!< default 1 */

static const char* LED_CPP_TAG = "LED_CPP";
uint64_t led::m_quick_blink_freq = 0;
uint64_t led::m_slow_blink_freq = 0;

led::led(uint8_t io_num, led_dark_level_t dark_level)
{
    if (m_quick_blink_freq == 0 && m_slow_blink_freq == 0) {
        m_quick_blink_freq = QUICK_BLINK_FREQ_CPP;
        m_slow_blink_freq = SLOW_BLINK_FREQ_CPP;
        ESP_LOGI(LED_CPP_TAG, "setup led\n");
        led_setup();
    }
    m_led_handle = led_create(io_num, dark_level);
}

led::~led()
{
    led_delete(m_led_handle);
    m_led_handle = NULL;
}

esp_err_t led::on()
{
    return led_state_write(m_led_handle, LED_ON);
}

esp_err_t led::off()
{
    return led_state_write(m_led_handle, LED_OFF);
}

esp_err_t led::state_write(led_status_t state)
{
    return led_state_write(m_led_handle, state);
}

led_status_t led::state_read()
{
    return led_state_read(m_led_handle);
}

esp_err_t led::mode_write(led_mode_t mode)
{
    return led_mode_write(m_led_handle, mode);
}

led_mode_t led::mode_read()
{
    return led_mode_read(m_led_handle);
}

esp_err_t led::quick_blink()
{
    return led_state_write(m_led_handle, LED_QUICK_BLINK);
}

esp_err_t led::slow_blink()
{
    return led_state_write(m_led_handle, LED_SLOW_BLINK);
}

esp_err_t led::night_mode()
{
    return mode_write(LED_NIGHT_MODE);
}

esp_err_t led::normal_mode()
{
    return mode_write(LED_NORMAL_MODE);
}

esp_err_t led::blink_freq_write(uint64_t quick_fre, uint64_t slow_fre)
{
    m_quick_blink_freq = quick_fre;
    m_slow_blink_freq = slow_fre;
    led_update_blink_freq(m_quick_blink_freq, m_slow_blink_freq);
    return ESP_OK;
}

esp_err_t led::night_duty_write(uint8_t duty)
{
    return led_night_duty_write(duty);
}

uint8_t led::night_duty_read()
{
    return led_night_duty_read();
}

#endif
