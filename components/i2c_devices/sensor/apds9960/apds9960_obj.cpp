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
#include <stdlib.h>
#include <math.h>

static const char *tag = "APDS9960";
#include "iot_apds9960.h"

CApds9960::CApds9960(CI2CBus *p_i2c_bus, uint8_t addr, uint32_t timeout)
{
    bus = p_i2c_bus;
    m_sensor_handle = iot_apds9960_create(bus->get_bus_handle(), addr);
    iot_apds9960_set_timeout(m_sensor_handle, timeout);
}

esp_err_t CApds9960::enable_device(bool en)
{
    return iot_apds9960_enable(m_sensor_handle, en);
}

CApds9960::~CApds9960()
{
    iot_apds9960_delete(m_sensor_handle, false);
    m_sensor_handle = NULL;
}

bool CApds9960::gesture_init(uint16_t iTimeMS, apds9960_again_t aGain)
{
    uint8_t apds9960_deviceid;
    /* Make sure we're actually connected */
    iot_apds9960_get_deviceid(m_sensor_handle, &apds9960_deviceid);
    if ((apds9960_deviceid != 0xAb) && (apds9960_deviceid != 0xA8)
            && (apds9960_deviceid != 0xA9) && (apds9960_deviceid != 0xAA)) {
        ESP_LOGE(tag, "%02x\n", apds9960_deviceid);
        return false;
    }

    /* Set default integration time and gain */
    set_adc_integration_time(iTimeMS);
    set_ambient_light_gain(aGain);

    // disable everything to start
    enable_gesture(false);
    enable_proximity(false);
    enable_color(false);

    enable_color_interrupt(false);
    enable_proximity_interrupt(false);
    clear_interrupt();

    /* Note: by default, the device is in power down mode on bootup */
    enable_device(false);
    vTaskDelay(10 / portTICK_RATE_MS);
    enable_device(true);
    vTaskDelay(10 / portTICK_RATE_MS);

    //default to all gesture dimensions
    set_gesture_dimensions(APDS9960_DIMENSIONS_ALL);
    set_gesture_fifo_threshold(APDS9960_GFIFO_4);
    set_gesture_gain(APDS9960_GGAIN_1X);
    set_gesture_proximity_threshold(30, 10);
    reset_counts();

    set_gesture_pulse(APDS9960_GPULSELEN_32US, 9);

    enable_proximity(true);
    enable_gesture(true);
    enable_gesture_interrupt(true);

    return true;
}

esp_err_t CApds9960::set_adc_integration_time(uint16_t iTimeMS)
{
    return iot_apds9960_set_adc_integration_time(m_sensor_handle, iTimeMS);
}

float CApds9960::get_adc_integration_time(void)
{
    return iot_apds9960_get_adc_integration_time(m_sensor_handle);
}

esp_err_t CApds9960::set_ambient_light_gain(apds9960_again_t aGain)
{
    return iot_apds9960_set_ambient_light_gain(m_sensor_handle, aGain);
}

apds9960_again_t CApds9960::get_ambient_light_gain(void)
{
    return iot_apds9960_get_ambient_light_gain(m_sensor_handle);
}

esp_err_t CApds9960::set_led_drive_boost(apds9960_leddrive_t drive,
        apds9960_ledboost_t boost)
{
    return iot_apds9960_set_led_drive_boost(m_sensor_handle, drive, boost);
}

esp_err_t CApds9960::set_proximity_pulse(apds9960_ppulse_len_t pLen, uint8_t pulses)
{
    return iot_apds9960_set_proximity_pulse(m_sensor_handle, pLen, pulses);
}

esp_err_t CApds9960::enable_proximity(bool en)
{
    return iot_apds9960_enable_proximity_engine(m_sensor_handle, en);
}

esp_err_t CApds9960::set_proximity_gain(apds9960_pgain_t gain)
{
    return iot_apds9960_set_proximity_gain(m_sensor_handle, gain);
}

apds9960_pgain_t CApds9960::get_proximity_gain(void)
{
    return iot_apds9960_get_proximity_gain(m_sensor_handle);
}

esp_err_t CApds9960::enable_proximity_interrupt(bool en)
{
    return iot_apds9960_enable_proximity_interrupt(m_sensor_handle, en);
}

uint8_t CApds9960::read_proximity(void)
{
    return iot_apds9960_read_proximity(m_sensor_handle);
}

esp_err_t CApds9960::set_proximity_interrupt_threshold(uint8_t low, uint8_t high,
        uint8_t persistance)
{
    return iot_apds9960_set_proximity_interrupt_threshold(m_sensor_handle, low, high,
            persistance);
}

bool CApds9960::get_proximity_interrupt()
{
    return iot_apds9960_get_proximity_interrupt(m_sensor_handle);
}

esp_err_t CApds9960::clear_interrupt()
{
    return iot_apds9960_clear_interrupt(m_sensor_handle);
}

esp_err_t CApds9960::enable_color(bool en)
{
    return iot_apds9960_enable_color_engine(m_sensor_handle, en);
}

bool CApds9960::color_data_ready()
{
    return iot_apds9960_color_data_ready(m_sensor_handle);
}

esp_err_t CApds9960::get_color_data(uint16_t *r, uint16_t *g, uint16_t *b,
        uint16_t *c)
{
    return iot_apds9960_get_color_data(m_sensor_handle, r, g, b, c);
}

uint16_t CApds9960::calculate_color_temperature(uint16_t r, uint16_t g,
        uint16_t b)
{
    return iot_apds9960_calculate_color_temperature(m_sensor_handle, r, g, b);
}

uint16_t CApds9960::calculate_lux(uint16_t r, uint16_t g, uint16_t b)
{
    return iot_apds9960_calculate_lux(m_sensor_handle, r, g, b);
}

esp_err_t CApds9960::enable_color_interrupt(bool en)
{
    return iot_apds9960_enable_color_interrupt(m_sensor_handle, en );
}

esp_err_t CApds9960::set_int_limits(uint16_t l, uint16_t h)
{
    return iot_apds9960_set_int_limits(m_sensor_handle, l, h);
}

bool CApds9960::gesture_valid()
{
    return iot_apds9960_gesture_valid(m_sensor_handle);
}

esp_err_t CApds9960::set_gesture_pulse(apds9960_gpulselen_t gLen, uint8_t pulses)
{
    return iot_apds9960_set_gesture_pulse(m_sensor_handle, gLen, pulses);
}

void CApds9960::reset_counts()
{
    iot_apds9960_reset_counts(m_sensor_handle);
}

uint8_t CApds9960::read_gesture(void)
{
    return iot_apds9960_read_gesture(m_sensor_handle);
}

esp_err_t CApds9960::set_gesture_dimensions(uint8_t dims)
{
    return iot_apds9960_set_gesture_dimensions(m_sensor_handle, dims);
}

esp_err_t CApds9960::set_gesture_fifo_threshold(uint8_t thresh)
{
    return iot_apds9960_set_gesture_fifo_threshold(m_sensor_handle, thresh);
}

esp_err_t CApds9960::set_gesture_waittime(apds9960_gwtime_t time)
{
    return iot_apds9960_set_gesture_waittime(m_sensor_handle, time);
}

esp_err_t CApds9960::set_gesture_gain(apds9960_ggain_t ggain)
{
    return iot_apds9960_set_gesture_gain(m_sensor_handle, ggain);
}

esp_err_t CApds9960::set_gesture_proximity_threshold(uint8_t entthresh, uint8_t exitthresh)
{
    return iot_apds9960_set_gesture_proximity_threshold(m_sensor_handle, entthresh, exitthresh);
}

esp_err_t CApds9960::set_gesture_offset(uint8_t offset_up, uint8_t offset_down,
        uint8_t offset_left, uint8_t offset_right)
{
    return iot_apds9960_set_gesture_offset(m_sensor_handle, offset_up, offset_down,
            offset_left, offset_right);
}

esp_err_t CApds9960::enable_gesture_interrupt(bool en)
{
    return iot_apds9960_enable_gesture_interrupt(m_sensor_handle, en);
}

esp_err_t CApds9960::enable_gesture(bool en)
{
    return iot_apds9960_enable_gesture_engine(m_sensor_handle, en);
}
