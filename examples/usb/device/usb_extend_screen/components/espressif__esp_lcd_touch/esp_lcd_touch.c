/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_touch.h"

static const char *TAG = "TP";

/*******************************************************************************
* Function definitions
*******************************************************************************/

/*******************************************************************************
* Local variables
*******************************************************************************/

/*******************************************************************************
* Public API functions
*******************************************************************************/

esp_err_t esp_lcd_touch_enter_sleep(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);
    if (tp->enter_sleep == NULL) {
        ESP_LOGE(TAG, "Sleep mode not supported!");
        return ESP_FAIL;
    } else {
        return tp->enter_sleep(tp);
    }
}

esp_err_t esp_lcd_touch_exit_sleep(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);
    if (tp->exit_sleep == NULL) {
        ESP_LOGE(TAG, "Sleep mode not supported!");
        return ESP_FAIL;
    } else {
        return tp->exit_sleep(tp);
    }
}

esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);
    assert(tp->read_data != NULL);

    return tp->read_data(tp);
}

bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num)
{
    bool touched = false;

    assert(tp != NULL);
    assert(x != NULL);
    assert(y != NULL);
    assert(tp->get_xy != NULL);

    touched = tp->get_xy(tp, x, y, strength, track_id, point_num, max_point_num);
    if (!touched) {
        return false;
    }

    /* Process coordinates by user */
    if (tp->config.process_coordinates != NULL) {
        tp->config.process_coordinates(tp, x, y, strength, track_id, point_num, max_point_num);
    }

    /* Software coordinates adjustment needed */
    bool sw_adj_needed = ((tp->config.flags.mirror_x && (tp->set_mirror_x == NULL)) ||
                          (tp->config.flags.mirror_y && (tp->set_mirror_y == NULL)) ||
                          (tp->config.flags.swap_xy && (tp->set_swap_xy == NULL)));

    /* Adjust all coordinates */
    for (int i = 0; (sw_adj_needed && i < *point_num); i++) {

        /*  Mirror X coordinates (if not supported by HW) */
        if (tp->config.flags.mirror_x && tp->set_mirror_x == NULL) {
            x[i] = tp->config.x_max - x[i];
        }

        /*  Mirror Y coordinates (if not supported by HW) */
        if (tp->config.flags.mirror_y && tp->set_mirror_y == NULL) {
            y[i] = tp->config.y_max - y[i];
        }

        /* Swap X and Y coordinates (if not supported by HW) */
        if (tp->config.flags.swap_xy && tp->set_swap_xy == NULL) {
            uint16_t tmp = x[i];
            x[i] = y[i];
            y[i] = tmp;
        }
    }

    return touched;
}

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
esp_err_t esp_lcd_touch_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state)
{
    assert(tp != NULL);
    assert(state != NULL);

    *state = 0;

    if (tp->get_button_state) {
        return tp->get_button_state(tp, n, state);
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}
#endif

esp_err_t esp_lcd_touch_set_swap_xy(esp_lcd_touch_handle_t tp, bool swap)
{
    assert(tp != NULL);

    tp->config.flags.swap_xy = swap;

    /* Is swap supported by HW? */
    if (tp->set_swap_xy) {
        return tp->set_swap_xy(tp, swap);
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_get_swap_xy(esp_lcd_touch_handle_t tp, bool *swap)
{
    assert(tp != NULL);
    assert(swap != NULL);

    /* Is swap supported by HW? */
    if (tp->get_swap_xy) {
        return tp->get_swap_xy(tp, swap);
    } else {
        *swap = tp->config.flags.swap_xy;
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_set_mirror_x(esp_lcd_touch_handle_t tp, bool mirror)
{
    assert(tp != NULL);

    tp->config.flags.mirror_x = mirror;

    /* Is mirror supported by HW? */
    if (tp->set_mirror_x) {
        return tp->set_mirror_x(tp, mirror);
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_get_mirror_x(esp_lcd_touch_handle_t tp, bool *mirror)
{
    assert(tp != NULL);
    assert(mirror != NULL);

    /* Is swap supported by HW? */
    if (tp->get_mirror_x) {
        return tp->get_mirror_x(tp, mirror);
    } else {
        *mirror = tp->config.flags.mirror_x;
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_set_mirror_y(esp_lcd_touch_handle_t tp, bool mirror)
{
    assert(tp != NULL);

    tp->config.flags.mirror_y = mirror;

    /* Is mirror supported by HW? */
    if (tp->set_mirror_y) {
        return tp->set_mirror_y(tp, mirror);
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_get_mirror_y(esp_lcd_touch_handle_t tp, bool *mirror)
{
    assert(tp != NULL);
    assert(mirror != NULL);

    /* Is swap supported by HW? */
    if (tp->get_mirror_y) {
        return tp->get_mirror_y(tp, mirror);
    } else {
        *mirror = tp->config.flags.mirror_y;
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_del(esp_lcd_touch_handle_t tp)
{
    assert(tp != NULL);

    if (tp->del != NULL) {
        return tp->del(tp);
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback)
{
    esp_err_t ret = ESP_OK;
    assert(tp != NULL);

    /* Interrupt pin is not selected */
    if (tp->config.int_gpio_num == GPIO_NUM_NC) {
        return ESP_ERR_INVALID_ARG;
    }

    tp->config.interrupt_callback = callback;

    if (callback != NULL) {
        ret = gpio_install_isr_service(0);
        /* ISR service can be installed from user before, then it returns invalid state */
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "GPIO ISR install failed");
            return ret;
        }
        /* Add GPIO ISR handler */
        ret = gpio_intr_enable(tp->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR install failed");
        ret = gpio_isr_handler_add(tp->config.int_gpio_num, (gpio_isr_t)tp->config.interrupt_callback, tp);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR install failed");
    } else {
        /* Remove GPIO ISR handler */
        ret = gpio_isr_handler_remove(tp->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR remove handler failed");
        ret = gpio_intr_disable(tp->config.int_gpio_num);
        ESP_RETURN_ON_ERROR(ret, TAG, "GPIO ISR disable failed");
    }

    return ESP_OK;
}

esp_err_t esp_lcd_touch_register_interrupt_callback_with_data(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback, void *user_data)
{
    assert(tp != NULL);

    tp->config.user_data = user_data;
    return esp_lcd_touch_register_interrupt_callback(tp, callback);
}
