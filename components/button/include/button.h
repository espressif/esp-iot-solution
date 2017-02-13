#ifndef _IOT_BUTTON_H_
#define _IOT_BUTTON_H_

#include "driver/gpio.h"
typedef void (* key_function)(void);
typedef void (* key_cb)(void*);
typedef void* button_handle_t;

typedef enum {
    BUTTON_ACTIVE_HIGH = 1,
    BUTTON_ACTIVE_LOW = 0,
} button_active_t;

/**
 * @brief Init button functions
 *
 * @param gpio_num GPIO index of the pin that the button uses
 * @param cb_num "Press action" call back function number, users can register their own callback functions.
 * @param active_level button hardware active level.
 *        For "BUTTON_ACTIVE_LOW" it means when the button pressed, the GPIO will read high level.
 *
 * @return A button_handle_t handle to the created button object, or NULL in case of error.
 */
button_handle_t button_dev_init(gpio_num_t gpio_num, int cb_num, button_active_t active_level);

/**
 * @brief Register a callback function for a "TAP" action.
 *
 * @param cb callback function for "TAP" action.
 * @param arg Parameter for callback function
 * @param interval_tick a filter to bypass glitch, unit: FreeRTOS ticks.
 * @param btn_handle handle of the button object
 * @note
 *        Timer callback functions execute in the context of the timer service task.
 *        It is therefore essential that timer callback functions never attempt to block.
 *        For example, a timer callback function must not call vTaskDelay(), vTaskDelayUntil(),
 *        or specify a non zero block time when accessing a queue or a semaphore.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_dev_add_tap_cb(key_cb cb, void* arg, int interval_tick, button_handle_t btn_handle);

/**
 * @brief
 *
 * @param cb_idx Index for "PRESS" callback function.
 * @param cb callback function for "PRESS" action.
 * @param arg Parameter for callback function
 * @param interval_tick a filter to bypass glitch, unit: FreeRTOS ticks.
 * @param btn_handle handle of the button object
 * @note
 *        Timer callback functions execute in the context of the timer service task.
 *        It is therefore essential that timer callback functions never attempt to block.
 *        For example, a timer callback function must not call vTaskDelay(), vTaskDelayUntil(),
 *        or specify a non zero block time when accessing a queue or a semaphore.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_dev_add_press_cb(int cb_idx, key_cb cb, void* arg, int interval_tick, button_handle_t btn_handle);

/**
 * @brief Delete button object and free memory
 * @param btn_handle handle of the button object
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Parameter error
 */
esp_err_t button_dev_free(button_handle_t btn_handle);
#endif
