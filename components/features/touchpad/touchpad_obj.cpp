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
#include "iot_touchpad.h"
#include "esp_log.h"

CTouchPad::CTouchPad(touch_pad_t touch_pad_num, uint16_t thres_percent, uint16_t thresh_abs, uint32_t filter_value)
{
    m_tp_handle = iot_tp_create(touch_pad_num, thres_percent, thresh_abs, filter_value);
}

CTouchPad::~CTouchPad()
{
    iot_tp_delete(m_tp_handle);
    m_tp_handle = NULL;
}

esp_err_t CTouchPad::add_cb(tp_cb_type_t cb_type, tp_cb cb, void  *arg)
{
    return iot_tp_add_cb(m_tp_handle, cb_type, cb, arg);
}

esp_err_t CTouchPad::set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg)
{
    return iot_tp_set_serial_trigger(m_tp_handle, trigger_thres_sec, interval_ms, cb, arg);
}

esp_err_t CTouchPad::add_custom_cb(uint32_t press_sec, tp_cb cb, void  *arg)
{
    return iot_tp_add_custom_cb(m_tp_handle, press_sec, cb, arg);
}

touch_pad_t CTouchPad::tp_num()
{
    return iot_tp_num_get(m_tp_handle);
}

esp_err_t CTouchPad::set_threshold(uint32_t threshold)
{
    return iot_tp_set_threshold(m_tp_handle, threshold);
}

esp_err_t CTouchPad::set_filter(uint32_t filter_value)
{
    return iot_tp_set_filter(m_tp_handle, filter_value);
}

uint16_t CTouchPad::value()
{
    uint16_t tp_value = 0;
    iot_tp_read(m_tp_handle, &tp_value);
    return tp_value;
}

CTouchPadSlide::CTouchPadSlide(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, const uint16_t *thresh_abs, uint32_t filter_value)
{
    m_tp_slide_handle = iot_tp_slide_create(num, tps, pos_scale, thres_percent, thresh_abs, filter_value);
}

CTouchPadSlide::~CTouchPadSlide()
{
    iot_tp_slide_delete(m_tp_slide_handle);
    m_tp_slide_handle = NULL;
}

uint8_t CTouchPadSlide::get_position()
{
    return iot_tp_slide_position(m_tp_slide_handle);
}

CTouchPadMatrix::CTouchPadMatrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, const uint16_t thres_percent, const uint16_t *thresh_abs, uint32_t filter_value)
{
    m_tp_matrix = iot_tp_matrix_create(x_num, y_num, x_tps, y_tps, thres_percent, thresh_abs, filter_value);
}

CTouchPadMatrix::~CTouchPadMatrix()
{
    iot_tp_matrix_delete(m_tp_matrix);
    m_tp_matrix = NULL;
}

esp_err_t CTouchPadMatrix::add_cb(tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg)
{
    return iot_tp_matrix_add_cb(m_tp_matrix, cb_type, cb, arg);
}

esp_err_t CTouchPadMatrix::add_custom_cb(uint32_t press_sec, tp_matrix_cb cb, void *arg)
{
    return iot_tp_matrix_add_custom_cb(m_tp_matrix, press_sec, cb, arg);
}

esp_err_t CTouchPadMatrix::set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg)
{
    return iot_tp_matrix_set_serial_trigger(m_tp_matrix, trigger_thres_sec, interval_ms, cb, arg);
}

