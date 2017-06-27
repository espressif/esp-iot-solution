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

#ifndef _IOT_TOUCHPAD_H_
#define _IOT_TOUCHPAD_H_
#include "driver/touch_pad.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* touchpad_handle_t;
typedef void* touchpad_slide_handle_t;
typedef void* touchpad_matrix_handle_t;
typedef void* proximity_sensor_handle_t;
typedef void* gesture_sensor_handle_t;

typedef void (* touchpad_cb)(void *);
typedef void (* touchpad_matrix_cb)(void *, uint8_t, uint8_t);

typedef enum {
    TOUCHPAD_CB_PUSH = 0,        /**< touch pad push callback */
    TOUCHPAD_CB_RELEASE,         /**< touch pad release callback */
    TOUCHPAD_CB_TAP,             /**< touch pad quick tap callback */
    TOUCHPAD_CB_MAX,
} touchpad_cb_type_t;

typedef enum {
    PROXIMITY_IDLE = 0,
    PROXIMITY_APPROACHING,
    PROXIMITY_LEAVING,
} proximity_status_t;

typedef void (* proximity_cb)(void *, proximity_status_t);

typedef enum {
    GESTURE_TOP_TO_BOTTOM = 0,
    GESTURE_BOTTOM_TO_TOP,
    GESTURE_LEFT_TO_RIGHT,
    GESTURE_RIGHT_TO_LEFT,
    GESTURE_TYPE_MAX,
} gesture_type_t;

typedef void (* gesture_cb)(void *, gesture_type_t);
/**
  * @brief  create touchpad device
  *
  * @param  touch_pad_num refer to struct touch_pad_t
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent 1/1000
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
touchpad_handle_t touchpad_create(touch_pad_t touch_pad_num, uint16_t thres_percent, uint32_t filter_value);

/**
  * @brief  delete touchpad device
  *
  * @param  touchpad_handle 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_delete(touchpad_handle_t touchpad_handle);

/**
  * @brief  add callback function
  *
  * @param  touchpad_handle 
  * @param  cb_type the type of callback to be added
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_add_cb(touchpad_handle_t touchpad_handle, touchpad_cb_type_t cb_type, touchpad_cb cb, void  *arg);

/**
  * @brief  set serial tigger
  *
  * @param  touchpad_handle 
  * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param  interval_ms evetn would trigger every interval_ms
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_set_serial_trigger(touchpad_handle_t touchpad_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_cb cb, void *arg);

/**
  * @brief  add custom callback function
  *
  * @param  touchpad_handle 
  * @param  press_sec the callback function would be called pointed seconds
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_add_custom_cb(touchpad_handle_t touchpad_handle, uint32_t press_sec, touchpad_cb cb, void  *arg);

/**
  * @brief  get the number of a touchpad
  *
  * @param  touchpad_handle 
  *
  * @return  touchpad number
  */
touch_pad_t touchpad_num_get(touchpad_handle_t touchpad_handle);

/**
  * @brief  set the threshold of touchpad
  *
  * @param  touchpad_handle 
  * @param  threshold value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_set_threshold(touchpad_handle_t touchpad_handle, uint32_t threshold);

/**
  * @brief  set the filter value of touchpad
  *
  * @param  touchpad_handle 
  * @param  filter_value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_set_filter(touchpad_handle_t touchpad_handle, uint32_t filter_value);

/**
  * @brief  get sample value of the touchpad
  *
  * @param  touchpad_handle 
  * @param  touch_value_ptr pointer to the value read 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param touchpad_handle is NULL
  */
esp_err_t touchpad_read(touchpad_handle_t touchpad_handle, uint16_t *touch_value_ptr);

/**
  * @brief  create touchpad slide device
  *
  * @param  num number of touchpads the slide uses
  * @param  tps the array of touchpad num
  * @param  pos_scale the position scale of each pad
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
touchpad_slide_handle_t touchpad_slide_create(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, uint32_t filter_value);

/**
  * @brief  get relative position
  *
  * @param  tp_slide_handle
  *
  * @return  relative position of your touch on slide
  */
uint8_t touchpad_slide_position(touchpad_slide_handle_t tp_slide_handle);

/**
  * @brief  delete touchpad slide device
  *
  * @param  tp_slide_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_slide_handle is NULL
  */
esp_err_t touchpad_slide_delete(touchpad_slide_handle_t tp_slide_handle);

/**
  * @brief  create touchpad matrix device
  *
  * @param  x_num number of touch sensor on x axis
  * @param  y_num number of touch sensor on y axis
  * @param  x_tps the array of touch sensor num on x axis
  * @param  y_tps the array of touch sensor num on y axis
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
touchpad_matrix_handle_t touchpad_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, uint32_t filter_value);

/**
  * @brief  delete touchpad matrix device
  *
  * @param  tp_matrix_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_matrix_hd is NULL
  */
esp_err_t touchpad_matrix_delete(touchpad_matrix_handle_t tp_matrix_hd);

/**
  * @brief  add callback function
  *
  * @param  tp_matrix_hd 
  * @param  cb_type the type of callback to be added
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_matrix_add_cb(touchpad_matrix_handle_t tp_matrix_hd, touchpad_cb_type_t cb_type, touchpad_matrix_cb cb, void *arg);

/**
  * @brief  add custom callback function
  *
  * @param  tp_matrix_hd 
  * @param  press_sec the callback function would be called after pointed seconds
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_matrix_add_custom_cb(touchpad_matrix_handle_t tp_matrix_hd, uint32_t press_sec, touchpad_matrix_cb cb, void *arg);

/**
  * @brief  set serial tigger
  *
  * @param  tp_matrix_hd 
  * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param  interval_ms evetn would trigger every interval_ms
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t touchpad_matrix_set_serial_trigger(touchpad_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_matrix_cb cb, void *arg);

/**
  * @brief  create proximity sensor
  *
  * @param  tp number of touch sensor
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
proximity_sensor_handle_t proximity_sensor_create(touch_pad_t tp, uint16_t thres_percent, uint32_t filter_value);

/**
  * @brief  add callback function
  *
  * @param  prox_hd
  * @param  cb
  * @param  arg
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t proximity_sensor_add_cb(proximity_sensor_handle_t prox_hd, proximity_cb cb, void *arg);

/**
  * @brief  delete proximity sensor
  *
  * @param  prox_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param prox_hd is NULL
  */
esp_err_t proximity_sensor_delete(proximity_sensor_handle_t prox_hd);

/**
  * @brief  get relative value of proximity sensor
  *
  * @param  prox_hd 
  * @param  pval pointer to the value read 
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t proximity_sensor_read(proximity_sensor_handle_t prox_hd, uint16_t *pval);

/**
  * @brief  create gesture sensor
  *
  * @param  tps number of four touch sensors
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
gesture_sensor_handle_t gesture_sensor_create(const touch_pad_t *tps, uint16_t thres_percent, uint32_t filter_value);

/**
  * @brief  delete gesture sensor
  *
  * @param  gest_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param gest_hd is NULL
  */
esp_err_t gesture_sensor_delete(gesture_sensor_handle_t gest_hd);

/**
  * @brief  add callback function
  *
  * @param  gest_hd
  * @param  cb
  * @param  arg
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t gesture_sensor_add_cb(gesture_sensor_handle_t gest_hd, gesture_cb cb, void *arg);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class touchpad
{
private:
    touchpad_handle_t m_tp_handle;
    touchpad(const touchpad &);
    touchpad& operator = (const touchpad &);
public:
    touchpad(touch_pad_t touch_pad_num, uint16_t thres_percent, uint32_t filter_value);
    ~touchpad();

    esp_err_t add_cb(touchpad_cb_type_t cb_type, touchpad_cb cb, void  *arg);
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_cb cb, void *arg);
    esp_err_t add_custom_cb(uint32_t press_sec, touchpad_cb cb, void  *arg);
    touch_pad_t touchpad_num();
    esp_err_t set_threshold(uint32_t threshold);
    esp_err_t set_filter(uint32_t filter_value);
    uint16_t value();
};

class touchpad_slide
{
private:
    touchpad_slide_handle_t m_tp_slide_handle;
    touchpad_slide(const touchpad_slide &);
    touchpad_slide& operator = (const touchpad_slide &);
public:
    touchpad_slide(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, uint32_t filter_value);
    ~touchpad_slide();

    uint8_t get_position();
};

class touchpad_matrix
{
private:
    touchpad_matrix_handle_t m_tp_matrix;
    touchpad_matrix(const touchpad_matrix &);
    touchpad_matrix& operator = (const touchpad_matrix &);
public:
    touchpad_matrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, uint32_t filter_value);
    ~touchpad_matrix();

    esp_err_t add_cb(touchpad_cb_type_t cb_type, touchpad_matrix_cb cb, void *arg);
    esp_err_t add_custom_cb(uint32_t press_sec, touchpad_matrix_cb cb, void *arg);
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, touchpad_matrix_cb cb, void *arg);
};

#endif

#endif
