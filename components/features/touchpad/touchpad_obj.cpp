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
#include "esp_system.h"
#include "iot_touchpad.h"
#include "esp_log.h"

CTouchPad::CTouchPad(touch_pad_t touch_pad_num, float sensitivity)
{
    m_tp_handle = iot_tp_create(touch_pad_num, sensitivity);
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

esp_err_t CTouchPad::set_threshold(float threshold)
{
    return iot_tp_set_threshold(m_tp_handle, threshold);
}

esp_err_t CTouchPad::get_threshold(float *threshold)
{
    return iot_tp_get_threshold(m_tp_handle, threshold);
}

uint16_t CTouchPad::value()
{
    uint16_t tp_value = 0;
    iot_tp_read(m_tp_handle, &tp_value);
    return tp_value;
}

CTouchPadSlide::CTouchPadSlide(uint8_t num, const touch_pad_t *tps, uint32_t pos_range, const float *p_sensitivity)
{
    m_tp_slide_handle = iot_tp_slide_create(num, tps, pos_range, p_sensitivity);
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

CTouchPadMatrix::CTouchPadMatrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, \
        const touch_pad_t *y_tps, const float *p_sensitivity)
{
    m_tp_matrix = iot_tp_matrix_create(x_num, y_num, x_tps, y_tps, p_sensitivity);
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

