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
#ifndef _IOT_TOUCHPAD_H_
#define _IOT_TOUCHPAD_H_
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "sdkconfig.h"

#ifdef CONFIG_DATA_SCOPE_DEBUG
#include "touch_tune_tool.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *tp_handle_t;
typedef void *tp_slide_handle_t;
typedef void *tp_matrix_handle_t;

typedef void (* tp_cb)(void *);           /**< callback function of touchpad */
typedef void (* tp_matrix_cb)(void *, uint8_t, uint8_t);      /**< callback function of touchpad matrix */

typedef enum {
    TOUCHPAD_CB_PUSH = 0,        /**< touch pad push callback */
    TOUCHPAD_CB_RELEASE,         /**< touch pad release callback */
    TOUCHPAD_CB_TAP,             /**< touch pad quick tap callback */
    TOUCHPAD_CB_SLIDE,           /**< touch pad slide type callback */
    TOUCHPAD_CB_MAX,
} tp_cb_type_t;

/**
  * @brief create single button device
  *
  * @param touch_pad_num Reference touch_pad_t structure
  * @param sensitivity The max change rate of the reading value when a touch event occurs.
  *         i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     tp_handle_t: Touch pad handle.
  */
tp_handle_t iot_tp_create(touch_pad_t touch_pad_num, float sensitivity);

/**
  * @brief delete touchpad device
  *
  * @param tp_handle Touch pad handle.
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_delete(tp_handle_t tp_handle);

/**
  * @brief add callback function
  *
  * @param tp_handle Touch pad handle.
  * @param cb_type the type of callback to be added
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_cb(tp_handle_t tp_handle, tp_cb_type_t cb_type, tp_cb cb, void  *arg);

/**
  * @brief set serial trigger
  *
  * @param tp_handle Touch pad handle.
  * @param trigger_thres_sec serial event would be trigger after trigger_thres_sec seconds
  * @param interval_ms event would be trigger every interval_ms
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_set_serial_trigger(tp_handle_t tp_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg);

/**
  * @brief add custom callback function
  *
  * @param tp_handle Touch pad handle.
  * @param press_sec the callback function would be called after pointed seconds
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_custom_cb(tp_handle_t tp_handle, uint32_t press_sec, tp_cb cb, void  *arg);

/**
  * @brief get the number of a touchpad
  *
  * @param tp_handle Touch pad handle.
  *
  * @return touchpad number
  */
touch_pad_t iot_tp_num_get(tp_handle_t tp_handle);

/**
  * @brief Set the trigger threshold of touchpad.
  *
  * @param tp_handle Touch pad handle.
  * @param threshold Should be less than the max change rate of touch.
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_set_threshold(tp_handle_t tp_handle, float threshold);

/**
  * @brief Get the trigger threshold of touchpad.
  *
  * @param tp_handle Touch pad handle.
  * @param threshold value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_threshold(const tp_handle_t tp_handle, float *threshold);

/**
  * @brief Get the IIR filter interval of touch sensor when touching.
  *
  * @param tp_handle Touch pad handle.
  * @param filter_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_touch_filter_interval(const tp_handle_t tp_handle, uint32_t *filter_ms);

/**
  * @brief Get the IIR filter interval of touch sensor when idle.
  *
  * @param tp_handle Touch pad handle.
  * @param filter_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_idle_filter_interval(const tp_handle_t tp_handle, uint32_t *filter_ms);

/**
  * @brief Get filtered touch sensor counter value from IIR filter process.
  *
  * @param tp_handle Touch pad handle.
  * @param touch_value_ptr pointer to the value read
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_read(tp_handle_t tp_handle, uint16_t *touch_value_ptr);

/**
  * @brief Get raw touch sensor counter value from IIR filter process.
  *
  * @param tp_handle Touch pad handle.
  * @param touch_value_ptr pointer to the value read
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t tp_read_raw(const tp_handle_t tp_handle, uint16_t *touch_value_ptr);

/**
  * @brief Create touchpad slide device.
  *
  * @param num number of touchpads the slide uses
  * @param tps the array of touchpad num
  * @param pos_range Set the range of the slide position. (0 ~ 255)
  * @param p_sensitivity Data list, the list stores the max change rate of the reading value when a touch event occurs.
  *         i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     NULL: error of input parameter
  *     tp_slide_handle_t: slide handle
  */
tp_slide_handle_t iot_tp_slide_create(uint8_t num, const touch_pad_t *tps, uint8_t pos_range, const float *p_sensitivity);

/**
  * @brief Get relative position of touch.
  *
  * @param tp_slide_handle
  *
  * @return relative position of your touch on slide. The range is 0 ~  pos_range.
  */
uint8_t iot_tp_slide_position(tp_slide_handle_t tp_slide_handle);

/**
  * @brief delete touchpad slide device
  *
  * @param tp_slide_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_slide_handle is NULL
  */
esp_err_t iot_tp_slide_delete(tp_slide_handle_t tp_slide_handle);

/**
  * @brief create touchpad matrix device
  *
  * @param x_num number of touch sensor on x axis
  * @param y_num number of touch sensor on y axis
  * @param x_tps the array of touch sensor num on x axis
  * @param y_tps the array of touch sensor num on y axis
  * @param p_sensitivity Data list(x list + y list), the list stores the max change rate of the reading value
  *         when a touch event occurs. i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     NULL: error of input parameter
  *     tp_matrix_handle_t: matrix handle
  */
tp_matrix_handle_t iot_tp_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, \
                                        const touch_pad_t *y_tps, const float *p_sensitivity);

/**
  * @brief delete touchpad matrix device
  *
  * @param tp_matrix_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_matrix_hd is NULL
  */
esp_err_t iot_tp_matrix_delete(tp_matrix_handle_t tp_matrix_hd);

/**
  * @brief add callback function
  *
  * @param tp_matrix_hd
  * @param cb_type the type of callback to be added
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_add_cb(tp_matrix_handle_t tp_matrix_hd, tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg);

/**
  * @brief add custom callback function
  *
  * @param tp_matrix_hd
  * @param press_sec the callback function would be called after pointed seconds
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_add_custom_cb(tp_matrix_handle_t tp_matrix_hd, uint32_t press_sec, tp_matrix_cb cb, void *arg);

/**
  * @brief set serial trigger
  *
  * @param tp_matrix_hd
  * @param trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param interval_ms evetn would trigger every interval_ms
  * @param cb the callback function
  * @param arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_set_serial_trigger(tp_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg);


#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

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
    CTouchPad &operator = (const CTouchPad &);
public:
    /**
      * @brief constructor of CTouchPad
      *
      * @param touch_pad_num Reference touch_pad_t structure
      * @param sensitivity Store the max change rate of the reading value when a touch event occurs.
      *         i.e., (non-trigger value - trigger value) / non-trigger value.
      *         Decreasing this threshold appropriately gives higher sensitivity.
      *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
      *
      */
    CTouchPad(touch_pad_t touch_pad_num, float sensitivity = 0.2);

    ~CTouchPad();

    /**
      * @brief add callback function
      *
      * @param cb_type the type of callback to be added
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_cb(tp_cb_type_t cb_type, tp_cb cb, void  *arg);

    /**
      * @brief set serial trigger
      *
      * @param trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
      * @param interval_ms evetn would trigger every interval_ms
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg);

    /**
      * @brief add custom callback function
      *
      * @param press_sec the callback function would be called after pointed seconds
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_custom_cb(uint32_t press_sec, tp_cb cb, void  *arg);

    /**
      * @brief get the number of a touchpad
      *
      * @return touchpad number
      */
    touch_pad_t tp_num();

    /**
      * @brief Set the trigger threshold of touchpad.
      *
      * @param threshold Should be less than the max change rate of touch.
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: the param tp_handle is NULL
      */
    esp_err_t set_threshold(float threshold);

    /**
      * @brief Get the trigger threshold of touchpad.
      *
      * @param threshold value
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: the param tp_handle is NULL
      */
    esp_err_t get_threshold(float *threshold);

    /**
      * @brief get filtered touch sensor counter value by IIR filter.
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
    CTouchPadSlide &operator = (const CTouchPadSlide &);
public:
    /**
      * @brief constructor of CTouchPadSlide
      *
      * @param num number of touchpads the slide uses
      * @param tps the array of touchpad num
      * @param pos_range the position range of each pad, Must be a multiple of (num-1).
      * @param p_sensitivity  Data list(x list + y list), the list stores change rate of the reading
      *         value when a touch event occurs. i.e., (non-trigger value - trigger value) / non-trigger value.
      *         Decreasing this threshold appropriately gives higher sensitivity.
      *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
      */
    CTouchPadSlide(uint8_t num, const touch_pad_t *tps, uint32_t pos_range = 50,  const float *p_sensitivity = NULL);

    ~CTouchPadSlide();

    /**
      * @brief Get relative position of touch.
      *
      * @return relative position of touch on slide. The range is 0 ~  pos_range.
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
    CTouchPadMatrix &operator = (const CTouchPadMatrix &);
public:
    /**
      * @brief constructor of CTouchPadMatrix
      *
      * @param x_num number of touch sensor on x axis
      * @param y_num number of touch sensor on y axis
      * @param x_tps the array of touch sensor num on x axis
      * @param y_tps the array of touch sensor num on y axis
      * @param p_sensitivity Data list(x list + y list), the list stores the change rate of the reading
      *         value when a touch event occurs. i.e., (non-trigger value - trigger value) / non-trigger value.
      *         Decreasing this threshold appropriately gives higher sensitivity.
      *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
      *
      */
    CTouchPadMatrix(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, const touch_pad_t *y_tps, const float *p_sensitivity = NULL);

    ~CTouchPadMatrix();

    /**
      * @brief add callback function
      *
      * @param cb_type the type of callback to be added
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_cb(tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg);

    /**
      * @brief add custom callback function
      *
      * @param press_sec the callback function would be called after pointed seconds
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t add_custom_cb(uint32_t press_sec, tp_matrix_cb cb, void *arg);

    /**
      * @brief set serial trigger
      *
      * @param trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
      * @param interval_ms evetn would trigger every interval_ms
      * @param cb the callback function
      * @param arg the argument of callback function
      *
      * @return
      *     - ESP_OK: succeed
      *     - ESP_FAIL: fail
      */
    esp_err_t set_serial_trigger(uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg);
};

#endif

#endif
