/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
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

#ifndef _IOT_BUTTON_H_
#define _IOT_BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/gpio.h"
typedef void (* button_cb)(void*);
typedef void* button_handle_t;

typedef enum {
    BUTTON_SINGLE_TRIGGER,  /**< button event will only trigger once no matter how long you press the touchpad */
    BUTTON_SERIAL_TRIGGER,  /**< number of button event triggerred will be in direct proportion to the duration of press*/
} button_trigger_t;

typedef enum {
    BUTTON_ACTIVE_HIGH = 1,    /*!<button active level: high level*/
    BUTTON_ACTIVE_LOW = 0,     /*!<button active level: low level*/
} button_active_t;

typedef enum {
    BUTTON_PUSH_CB = 0,   /*!<button push callback event */
    BUTTON_RELEASE_CB,    /*!<button release callback event */
    BUTTON_TAP_CB,        /*!<button quick tap callback event(will not trigger if there already is a "PRESS" event) */
    BUTTON_SERIAL_CB,     /*!<button serial trigger callback event */
} button_cb_type_t;

/**
 * @brief Init button functions
 *
 * @param gpio_num GPIO index of the pin that the button uses
 * @param active_level button hardware active level.
 *        For "BUTTON_ACTIVE_LOW" it means when the button pressed, the GPIO will read low level.
 * @param trigger button event trigger mode
 * @param serial_thres_sec the number of seconds more than whick a BUTTON_SERIAL_CB would accour
 *
 * @return A button_handle_t handle to the created button object, or NULL in case of error.
 */
button_handle_t button_create(gpio_num_t gpio_num, button_active_t active_level, button_trigger_t trigger, uint32_t serial_thres_sec);

/**
 * @brief Register a callback function for a button_cb_type_t action.
 *
 * @param btn_handle handle of the button object
 * @param type callback function type
 * @param cb callback function for "TAP" action.
 * @param arg Parameter for callback function
 * @param interval_tick a filter to bypass glitch, unit: FreeRTOS ticks.
 * @note
 *        Button callback functions execute in the context of the timer service task.
 *        It is therefore essential that button callback functions never attempt to block.
 *        For example, a button callback function must not call vTaskDelay(), vTaskDelayUntil(),
 *        or specify a non zero block time when accessing a queue or a semaphore.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_add_cb(button_handle_t btn_handle, button_cb_type_t type, button_cb cb, void* arg, int interval_tick);

/**
 * @brief
 *
 * @param btn_handle handle of the button object
 * @param press_sec the callback function would be called if you press the button for a specified period of time
 * @param cb callback function for "PRESS" action.
 * @param arg Parameter for callback function
 *
 * @note
 *        Button callback functions execute in the context of the timer service task.
 *        It is therefore essential that button callback functions never attempt to block.
 *        For example, a button callback function must not call vTaskDelay(), vTaskDelayUntil(),
 *        or specify a non zero block time when accessing a queue or a semaphore.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_add_custom_cb(button_handle_t btn_handle, uint32_t press_sec, button_cb cb, void* arg);

/**
 * @brief Delete button object and free memory
 * @param btn_handle handle of the button object
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_delete(button_handle_t btn_handle);

/**
 * @brief Remove callback
 *
 * @param btn_handle The handle of the button object
 * @param type callback function event type
 *
 * @return
 *     - ESP_OK Success
 */
esp_err_t button_rm_cb(button_handle_t btn_handle, button_cb_type_t type);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/**
 * class of button
 * simple usage:
 * CButton* btn = new CButton(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL, BUTTON_SERIAL_TRIGGER, 3);
 * btn->add_cb(BUTTON_PUSH_CB, button_tap_cb, (void*) push, 50 / portTICK_PERIOD_MS);
 * btn->add_custom_cb(5, button_press_5s_cb, NULL);
 * ......
 * delete btn;
 */
class CButton
{
private:
    button_handle_t m_btn_handle;

    /**
     * prevent copy constructing
     */
    CButton(const CButton&);
    CButton& operator = (const CButton&);
public:

    /**
     * @brief constructor of CButton
     * 
     * @param gpio_num GPIO index of the pin that the button uses
     * @param active_level button hardware active level.
     *        For "BUTTON_ACTIVE_LOW" it means when the button pressed, the GPIO will read low level.
     * @param trigger button event trigger mode
     * @param serial_thres_sec the number of seconds more than whick a BUTTON_SERIAL_CB would accour
     */
    CButton(gpio_num_t gpio_num, button_active_t active_level = BUTTON_ACTIVE_LOW, button_trigger_t trigger = BUTTON_SINGLE_TRIGGER, uint32_t serial_thres_sec = 3);
    
    ~CButton();

    /**
     * @brief Register a callback function for a button_cb_type_t action.
     *
     * @param type callback function type
     * @param cb callback function for "TAP" action.
     * @param arg Parameter for callback function
     * @param interval_tick a filter to bypass glitch, unit: FreeRTOS ticks.
     * @note
     *        Button callback functions execute in the context of the timer service task.
     *        It is therefore essential that button callback functions never attempt to block.
     *        For example, a button callback function must not call vTaskDelay(), vTaskDelayUntil(),
     *        or specify a non zero block time when accessing a queue or a semaphore.
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Parameter error
     */
    esp_err_t add_cb(button_cb_type_t type, button_cb cb, void* arg, int interval_tick);


    /**
     * @brief
     *
     * @param press_sec the callback function would be called if you press the button for a specified period of time
     * @param cb callback function for "PRESS" action.
     * @param arg Parameter for callback function
     *
     * @note
     *        Button callback functions execute in the context of the timer service task.
     *        It is therefore essential that button callback functions never attempt to block.
     *        For example, a button callback function must not call vTaskDelay(), vTaskDelayUntil(),
     *        or specify a non zero block time when accessing a queue or a semaphore.
     * @return
     *     - ESP_OK Success
     *     - ESP_FAIL Parameter error
     */
    esp_err_t add_custom_cb(uint32_t press_sec, button_cb cb, void* arg);

    /**
     * @brief Remove callback
     *
     * @param type callback function event type
     *
     * @return
     *     - ESP_OK Success
     */
    esp_err_t rm_cb(button_cb_type_t type);
};
#endif

#endif
