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
#include "esp_system.h"
#include "touchpad.h"
#include "esp_log.h"

CTouchPad::CTouchPad(touch_pad_t touch_pad_num, uint16_t thres_percent, uint32_t filter_value)
{
    m_tp_handle = touchpad_create(touch_pad_num, thres_percent, filter_value);
}

CTouchPad::~CTouchPad()
{
    touchpad_delete(m_tp_handle);
    m_tp_handle = NULL;
}

esp_err_t CTouchPad::add_cb(touchpad_cb_type_t cb_type, touchpad_cb cb, void  *arg)
{
    return touchpad_add_cb(m_tp_handle, cb_type, cb, arg);
}

esp_err_t CTouchPad::set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_cb cb, void *arg)
{
    return touchpad_set_serial_trigger(m_tp_handle, trigger_thres_sec, interval_ms, cb, arg);
}

esp_err_t CTouchPad::add_custom_cb(uint32_t press_sec, touchpad_cb cb, void  *arg)
{
    return touchpad_add_custom_cb(m_tp_handle, press_sec, cb, arg);
}

touch_pad_t CTouchPad::touchpad_num()
{
    return touchpad_num_get(m_tp_handle);
}

esp_err_t CTouchPad::set_threshold(uint32_t threshold)
{
    return touchpad_set_threshold(m_tp_handle, threshold);
}

esp_err_t CTouchPad::set_filter(uint32_t filter_value)
{
    return touchpad_set_filter(m_tp_handle, filter_value);
}

uint16_t CTouchPad::value()
{
    uint16_t tp_value = 0;
    touchpad_read(m_tp_handle, &tp_value);
    return tp_value;
}

CTouchPadSlide::CTouchPadSlide(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, uint32_t filter_value)
{
    m_tp_slide_handle = touchpad_slide_create(num, tps, pos_scale, thres_percent, filter_value);
}

CTouchPadSlide::~CTouchPadSlide()
{
    touchpad_slide_delete(m_tp_slide_handle);
    m_tp_slide_handle = NULL;
}

uint8_t CTouchPadSlide::get_position()
{
    return touchpad_slide_position(m_tp_slide_handle);
}

CTouchPadMatrix::CTouchPadMatrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, uint32_t filter_value)
{
    m_tp_matrix = touchpad_matrix_create(x_num, y_num, x_tps, y_tps, thres_percent, filter_value);
}

CTouchPadMatrix::~CTouchPadMatrix()
{
    touchpad_matrix_delete(m_tp_matrix);
    m_tp_matrix = NULL;
}

esp_err_t CTouchPadMatrix::add_cb(touchpad_cb_type_t cb_type, touchpad_matrix_cb cb, void *arg)
{
    return touchpad_matrix_add_cb(m_tp_matrix, cb_type, cb, arg);
}

esp_err_t CTouchPadMatrix::add_custom_cb(uint32_t press_sec, touchpad_matrix_cb cb, void *arg)
{
    return touchpad_matrix_add_custom_cb(m_tp_matrix, press_sec, cb, arg);
}

esp_err_t CTouchPadMatrix::set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_matrix_cb cb, void *arg)
{
    return touchpad_matrix_set_serial_trigger(m_tp_matrix, trigger_thres_sec, interval_ms, cb, arg);
}

CProximitySensor::CProximitySensor(touch_pad_t tp, uint16_t thres_percent, uint32_t filter_value)
{
    m_prox_sensor = proximity_sensor_create(tp, thres_percent, filter_value);
}

CProximitySensor::~CProximitySensor()
{
    proximity_sensor_delete(m_prox_sensor);
    m_prox_sensor = NULL;
}

esp_err_t CProximitySensor::add_cb(proximity_cb cb, void *arg)
{
    return proximity_sensor_add_cb(m_prox_sensor, cb, arg);
}

uint16_t CProximitySensor::value()
{
    uint16_t val = 0;
    proximity_sensor_read(m_prox_sensor, &val);
    return val;
}

CGestureSensor::CGestureSensor(const touch_pad_t *tps, uint16_t thres_percent, uint32_t filter_value)
{
    m_gest_sensor = gesture_sensor_create(tps, thres_percent, filter_value);
}

CGestureSensor::~CGestureSensor()
{
    gesture_sensor_delete(m_gest_sensor);
    m_gest_sensor = NULL;
}

esp_err_t CGestureSensor::add_cb(gesture_cb cb, void *arg)
{
    return gesture_sensor_add_cb(m_gest_sensor, cb, arg);
}
