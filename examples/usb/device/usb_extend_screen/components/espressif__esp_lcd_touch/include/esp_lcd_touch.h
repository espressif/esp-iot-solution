/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP LCD touch
 */

#pragma once

#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Touch controller type
 *
 */
typedef struct esp_lcd_touch_s esp_lcd_touch_t;
typedef esp_lcd_touch_t *esp_lcd_touch_handle_t;

/**
 * @brief Touch controller interrupt callback type
 *
 */
typedef void (*esp_lcd_touch_interrupt_callback_t)(esp_lcd_touch_handle_t tp);

/**
 * @brief Touch Configuration Type
 *
 */
typedef struct {
    uint16_t x_max; /*!< X coordinates max (for mirroring) */
    uint16_t y_max; /*!< Y coordinates max (for mirroring) */

    gpio_num_t rst_gpio_num;    /*!< GPIO number of reset pin */
    gpio_num_t int_gpio_num;    /*!< GPIO number of interrupt pin */

    struct {
        unsigned int reset: 1;    /*!< Level of reset pin in reset */
        unsigned int interrupt: 1;/*!< Active Level of interrupt pin */
    } levels;

    struct {
        unsigned int swap_xy: 1;  /*!< Swap X and Y after read coordinates */
        unsigned int mirror_x: 1; /*!< Mirror X after read coordinates */
        unsigned int mirror_y: 1; /*!< Mirror Y after read coordinates */
    } flags;

    /*!< User callback called after get coordinates from touch controller for apply user adjusting */
    void (*process_coordinates)(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num);
    /*!< User callback called after the touch interrupt occurred */
    esp_lcd_touch_interrupt_callback_t interrupt_callback;
    /*!< User data passed to callback */
    void *user_data;
    /*!< User data passed to driver */
    void *driver_data;
} esp_lcd_touch_config_t;

typedef struct {
    uint8_t points; /*!< Count of touch points saved */

    struct {
        uint8_t track_id; /*!< Track ID */
        uint16_t x; /*!< X coordinate */
        uint16_t y; /*!< Y coordinate */
        uint16_t strength; /*!< Strength */
    } coords[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
    uint8_t buttons; /*!< Count of buttons states saved */

    struct {
        uint8_t status; /*!< Status of button */
    } button[CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS];
#endif

    portMUX_TYPE lock; /*!< Lock for read/write */
} esp_lcd_touch_data_t;

/**
 * @brief Declare of Touch Type
 *
 */
struct esp_lcd_touch_s {

    /**
     * @brief set touch controller into sleep mode
     *
     * @note This function is usually blocking.
     *
     * @param tp: Touch handler
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*enter_sleep)(esp_lcd_touch_handle_t tp);

    /**
     * @brief set touch controller into normal mode
     *
     * @note This function is usually blocking.
     *
     * @param tp: Touch handler
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*exit_sleep)(esp_lcd_touch_handle_t tp);

    /**
     * @brief Read data from touch controller (mandatory)
     *
     * @note This function is usually blocking.
     *
     * @param tp: Touch handler
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*read_data)(esp_lcd_touch_handle_t tp);

    /**
     * @brief Get coordinates from touch controller (mandatory)
     *
     * @param tp: Touch handler
     * @param x: Array of X coordinates
     * @param y: Array of Y coordinates
     * @param strength: Array of strengths
     * @param point_num: Count of points touched (equals with count of items in x and y array)
     * @param max_point_num: Maximum count of touched points to return (equals with max size of x and y array)
     *
     * @return
     *      - Returns true, when touched and coordinates read. Otherwise returns false.
     */
    bool (*get_xy)(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num);

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
    /**
     * @brief Get button state (optional)
     *
     * @param tp: Touch handler
     * @param n: Button index
     * @param state: Button state
     *
     * @return
     *      - Returns true, when touched and coordinates read. Otherwise returns false.
     */
    esp_err_t (*get_button_state)(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state);
#endif

    /**
     * @brief Swap X and Y after read coordinates (optional)
     *        If set, then not used SW swapping.
     *
     * @param tp: Touch handler
     * @param swap: Set swap value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*set_swap_xy)(esp_lcd_touch_handle_t tp, bool swap);

    /**
     * @brief Are X and Y coordinates swapped (optional)
     *
     * @param tp: Touch handler
     * @param swap: Get swap value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*get_swap_xy)(esp_lcd_touch_handle_t tp, bool *swap);

    /**
     * @brief Mirror X after read coordinates
     *        If set, then not used SW mirroring.
     *
     * @param tp: Touch handler
     * @param mirror: Set X mirror value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*set_mirror_x)(esp_lcd_touch_handle_t tp, bool mirror);

    /**
     * @brief Is mirrored X (optional)
     *
     * @param tp: Touch handler
     * @param mirror: Get X mirror value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*get_mirror_x)(esp_lcd_touch_handle_t tp, bool *mirror);

    /**
     * @brief Mirror Y after read coordinates
     *        If set, then not used SW mirroring.
     *
     * @param tp: Touch handler
     * @param mirror: Set Y mirror value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*set_mirror_y)(esp_lcd_touch_handle_t tp, bool mirror);

    /**
     * @brief Is mirrored Y (optional)
     *
     * @param tp: Touch handler
     * @param mirror: Get Y mirror value
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*get_mirror_y)(esp_lcd_touch_handle_t tp, bool *mirror);

    /**
     * @brief Delete Touch
     *
     * @param tp: Touch handler
     *
     * @return
     *      - ESP_OK on success, otherwise returns ESP_ERR_xxx
     */
    esp_err_t (*del)(esp_lcd_touch_handle_t tp);

    /**
     * @brief Configuration structure
     */
    esp_lcd_touch_config_t config;

    /**
     * @brief Communication interface
     */
    esp_lcd_panel_io_handle_t io;

    /**
     * @brief Data structure
     */
    esp_lcd_touch_data_t data;
};

/**
 * @brief Read data from touch controller
 *
 * @note This function is usually blocking.
 *
 * @param tp: Touch handler
 *
 * @return
 *     - ESP_OK                 on success
 *     - ESP_ERR_INVALID_ARG    parameter error
 *     - ESP_FAIL               sending command error, slave hasn't ACK the transfer
 *     - ESP_ERR_INVALID_STATE  I2C driver not installed or not in master mode
 *     - ESP_ERR_TIMEOUT        operation timeout because the bus is busy
 */
esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t tp);

/**
 * @brief Read coordinates from touch controller
 *
 * @param tp: Touch handler
 * @param x: Array of X coordinates
 * @param y: Array of Y coordinates
 * @param strength: Array of the strengths (can be NULL)
 * @param point_num: Count of points touched (equals with count of items in x and y array)
 * @param max_point_num: Maximum count of touched points to return (equals with max size of x and y array)
 *
 * @return
 *      - Returns true, when touched and coordinates read. Otherwise returns false.
 */
bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *track_id, uint8_t *point_num, uint8_t max_point_num);

#if (CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS > 0)
/**
 * @brief Get button state
 *
 * @param tp: Touch handler
 * @param n: Button index
 * @param state: Button state
 *
 * @return
 *      - ESP_OK                    on success
 *      - ESP_ERR_NOT_SUPPORTED     if this function is not supported by controller
 *      - ESP_ERR_INVALID_ARG       if bad button index
 */
esp_err_t esp_lcd_touch_get_button_state(esp_lcd_touch_handle_t tp, uint8_t n, uint8_t *state);
#endif

/**
 * @brief Swap X and Y after read coordinates
 *
 * @param tp: Touch handler
 * @param swap: Set swap value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_set_swap_xy(esp_lcd_touch_handle_t tp, bool swap);

/**
 * @brief Are X and Y coordinates swapped
 *
 * @param tp: Touch handler
 * @param swap: Get swap value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_get_swap_xy(esp_lcd_touch_handle_t tp, bool *swap);

/**
 * @brief Mirror X after read coordinates
 *
 * @param tp: Touch handler
 * @param mirror: Set X mirror value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_set_mirror_x(esp_lcd_touch_handle_t tp, bool mirror);

/**
 * @brief Is mirrored X
 *
 * @param tp: Touch handler
 * @param mirror: Get X mirror value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_get_mirror_x(esp_lcd_touch_handle_t tp, bool *mirror);

/**
 * @brief Mirror Y after read coordinates
 *
 * @param tp: Touch handler
 * @param mirror: Set Y mirror value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_set_mirror_y(esp_lcd_touch_handle_t tp, bool mirror);

/**
 * @brief Is mirrored Y
 *
 * @param tp: Touch handler
 * @param mirror: Get Y mirror value
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_get_mirror_y(esp_lcd_touch_handle_t tp, bool *mirror);

/**
 * @brief Delete touch (free all allocated memory and restart HW)
 *
 * @param tp: Touch handler
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_del(esp_lcd_touch_handle_t tp);

/**
 * @brief Register user callback called after the touch interrupt occurred
 *
 * @param tp: Touch handler
 * @param callback: Interrupt callback
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_register_interrupt_callback(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback);

/**
 * @brief Register user callback called after the touch interrupt occurred with user data
 *
 * @param tp: Touch handler
 * @param callback: Interrupt callback
 * @param user_data: User data passed to callback
 *
 * @return
 *      - ESP_OK on success
 */
esp_err_t esp_lcd_touch_register_interrupt_callback_with_data(esp_lcd_touch_handle_t tp, esp_lcd_touch_interrupt_callback_t callback, void *user_data);

/**
 * @brief Enter sleep mode
 *
 * @param tp: Touch handler
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t esp_lcd_touch_enter_sleep(esp_lcd_touch_handle_t tp);

/**
 * @brief Exit sleep mode
 *
 * @param tp: Touch handler
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 */
esp_err_t esp_lcd_touch_exit_sleep(esp_lcd_touch_handle_t tp);

#ifdef __cplusplus
}
#endif
