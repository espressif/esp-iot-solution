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

typedef void* tp_handle_t;
typedef void* tp_slide_handle_t;
typedef void* tp_matrix_handle_t;

typedef void (* tp_cb)(void *);           /**< callback function of touchpad */
typedef void (* tp_matrix_cb)(void *, uint8_t, uint8_t);      /**< callback function of touchpad matrix */

typedef enum {
    TOUCHPAD_CB_PUSH = 0,        /**< touch pad push callback */
    TOUCHPAD_CB_RELEASE,         /**< touch pad release callback */
    TOUCHPAD_CB_TAP,             /**< touch pad quick tap callback */
    TOUCHPAD_CB_MAX,
} tp_cb_type_t;

/**
  * @brief  create touchpad device
  *
  * @param  touch_pad_num refer to struct touch_pad_t
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent 1/1000
  * @param  absolute value of threshold for pad
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
tp_handle_t iot_tp_create(touch_pad_t touch_pad_num, uint16_t thres_percent, uint16_t thresh_abs, uint32_t filter_value);

/**
  * @brief  delete touchpad device
  *
  * @param  tp_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_delete(tp_handle_t tp_handle);

/**
  * @brief  add callback function
  *
  * @param  tp_handle
  * @param  cb_type the type of callback to be added
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_cb(tp_handle_t tp_handle, tp_cb_type_t cb_type, tp_cb cb, void  *arg);

/**
  * @brief  set serial tigger
  *
  * @param  tp_handle
  * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param  interval_ms evetn would trigger every interval_ms
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_set_serial_trigger(tp_handle_t tp_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg);

/**
  * @brief  add custom callback function
  *
  * @param  tp_handle
  * @param  press_sec the callback function would be called pointed seconds
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_custom_cb(tp_handle_t tp_handle, uint32_t press_sec, tp_cb cb, void  *arg);

/**
  * @brief  get the number of a touchpad
  *
  * @param  tp_handle
  *
  * @return  touchpad number
  */
touch_pad_t iot_tp_num_get(tp_handle_t tp_handle);

/**
  * @brief  set the threshold of touchpad
  *
  * @param  tp_handle
  * @param  threshold value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_set_threshold(tp_handle_t tp_handle, uint32_t threshold);

/**
  * @brief  set the filter value of touchpad
  *
  * @param  tp_handle
  * @param  filter_value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_set_filter(tp_handle_t tp_handle, uint32_t filter_value);

/**
  * @brief  get sample value of the touchpad
  *
  * @param  tp_handle
  * @param  touch_value_ptr pointer to the value read 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_read(tp_handle_t tp_handle, uint16_t *touch_value_ptr);

/**
  * @brief  create touchpad slide device
  *
  * @param  num number of touchpads the slide uses
  * @param  tps the array of touchpad num
  * @param  pos_scale the position scale of each pad
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  thresh_abs absolute value array of threshold for each pad.
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
tp_slide_handle_t iot_tp_slide_create(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale, uint16_t thres_percent, const uint16_t* thresh_abs, uint32_t filter_value);

/**
  * @brief  get relative position
  *
  * @param  tp_slide_handle
  *
  * @return  relative position of your touch on slide
  */
uint8_t iot_tp_slide_position(tp_slide_handle_t tp_slide_handle);

/**
  * @brief  delete touchpad slide device
  *
  * @param  tp_slide_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_slide_handle is NULL
  */
esp_err_t iot_tp_slide_delete(tp_slide_handle_t tp_slide_handle);

/**
  * @brief  create touchpad matrix device
  *
  * @param  x_num number of touch sensor on x axis
  * @param  y_num number of touch sensor on y axis
  * @param  x_tps the array of touch sensor num on x axis
  * @param  y_tps the array of touch sensor num on y axis
  * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
  * @param  thresh_abs threshold value list of each pad.
  * @param  filter_value filter time of touchpad
  *
  * @return
  *     NULL: fail
  */
tp_matrix_handle_t iot_tp_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t thres_percent, const uint16_t *thresh_abs, uint32_t filter_value);

/**
  * @brief  delete touchpad matrix device
  *
  * @param  tp_matrix_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_matrix_hd is NULL
  */
esp_err_t iot_tp_matrix_delete(tp_matrix_handle_t tp_matrix_hd);

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
esp_err_t iot_tp_matrix_add_cb(tp_matrix_handle_t tp_matrix_hd, tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg);

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
esp_err_t iot_tp_matrix_add_custom_cb(tp_matrix_handle_t tp_matrix_hd, uint32_t press_sec, tp_matrix_cb cb, void *arg);

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
esp_err_t iot_tp_matrix_set_serial_trigger(tp_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define DEFAULT_THRES_PERCENT   950
#define DEFAULT_FILTER_MS       100

/**
 * class of touchpad
 */
class CTouchPad
{
private:
    tp_handle_t m_tp_handle;

    /**
     * prevent copy construct
     */
    CTouchPad(const CTouchPad &);
    CTouchPad& operator = (const CTouchPad &);
public:
    /**
      * @brief  constructor of CTouchPad
      *
      * @param  touch_pad_num refer to struct touch_pad_t
      * @param  thres_percent touchpad would be active if the sample value decay to thres_percent 1/1000
      * @param  filter_value filter time of touchpad
      */
    CTouchPad(touch_pad_t touch_pad_num, uint16_t thres_percent = DEFAULT_THRES_PERCENT, uint16_t thresh_abs = 0, uint32_t filter_value = DEFAULT_FILTER_MS);
    
    ~CTouchPad();

    /**
      * @brief  add callback function
      *
      * @param  cb_type the type of callback to be added
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_cb(tp_cb_type_t cb_type, tp_cb cb, void  *arg);

    /**
      * @brief  set serial tigger
      *
      * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
      * @param  interval_ms evetn would trigger every interval_ms
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg);
    
    /**
      * @brief  add custom callback function
      *
      * @param  press_sec the callback function would be called pointed seconds
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_custom_cb(uint32_t press_sec, tp_cb cb, void  *arg);

    /**
      * @brief  get the number of a touchpad
      *
      * @return  touchpad number
      */
    touch_pad_t tp_num();

    /**
      * @brief  set the threshold of touchpad
      *
      * @param  threshold value
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: the param tp_handle is NULL
      */
    esp_err_t set_threshold(uint32_t threshold);

    /**
      * @brief  set the filter value of touchpad
      *
      * @param  filter_value
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: the param tp_handle is NULL
      */
    esp_err_t set_filter(uint32_t filter_value);

    /**
      * @brief  get sample value of the touchpad
      *
      * @return sample value
      */
    uint16_t value();
};

/**
 * class of touchpad slide
 */
class CTouchPadSlide
{
private:
    tp_slide_handle_t m_tp_slide_handle;

    /**
     * prevent copy construct
     */
    CTouchPadSlide(const CTouchPadSlide &);
    CTouchPadSlide& operator = (const CTouchPadSlide &);
public:
    /**
      * @brief  constructor of CTouchPadSlide
      *
      * @param  num number of touchpads the slide uses
      * @param  tps the array of touchpad num
      * @param  pos_scale the position scale of each pad
      * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
      * @param  filter_value filter time of touchpad
      */
    CTouchPadSlide(uint8_t num, const touch_pad_t *tps, uint32_t pos_scale = 2, uint16_t threshold = DEFAULT_THRES_PERCENT, const uint16_t *thresh_abs = NULL, uint32_t filter_value = DEFAULT_FILTER_MS);
    
    ~CTouchPadSlide();

    /**
      * @brief  get relative position
      *
      * @return  relative position of your touch on slide
      */
    uint8_t get_position();
};

/**
 * class of touchpad matrix
 */
class CTouchPadMatrix
{
private:
    tp_matrix_handle_t m_tp_matrix;

    /**
     * prevent copy construct
     */
    CTouchPadMatrix(const CTouchPadMatrix &);
    CTouchPadMatrix& operator = (const CTouchPadMatrix &);
public:
    /**
      * @brief  constructor of CTouchPadMatrix
      *
      * @param  x_num number of touch sensor on x axis
      * @param  y_num number of touch sensor on y axis
      * @param  x_tps the array of touch sensor num on x axis
      * @param  y_tps the array of touch sensor num on y axis
      * @param  thres_percent touchpad would be active if the sample value decay to thres_percent
      * @param  filter_value filter time of touchpad
      *
      */
    CTouchPadMatrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, uint16_t threshold = DEFAULT_THRES_PERCENT, const uint16_t *thresh_abs = NULL, uint32_t filter_value = DEFAULT_FILTER_MS);
    
    ~CTouchPadMatrix();

    /**
      * @brief  add callback function
      *
      * @param  cb_type the type of callback to be added
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_cb(tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg);

    /**
      * @brief  add custom callback function
      *
      * @param  press_sec the callback function would be called after pointed seconds
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_custom_cb(uint32_t press_sec, tp_matrix_cb cb, void *arg);

    /**
      * @brief  set serial tigger
      *
      * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
      * @param  interval_ms evetn would trigger every interval_ms
      * @param  cb the callback function
      * @param  arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg);
};

#endif

#endif
