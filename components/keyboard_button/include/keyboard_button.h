/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "kbd_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Keyboard button event
 *
 */
typedef enum {
    KBD_EVENT_PRESSED = 0,            /*!< Report all currently pressed keys when a key is either pressed or released. */
    KBD_EVENT_COMBINATION,            /*!< When the component buttons are pressed in sequence, report. */
    KBD_EVENT_MAX,
} keyboard_btn_event_t;

/**
 * @brief keyboard button data
 *
 */
typedef struct {
    uint8_t output_index;             /*!< key position's output gpio number */
    uint8_t input_index;              /*!< key position's input gpio number */
} keyboard_btn_data_t;

/**
 * @brief keyboard button report data
 *
 */
typedef struct {
    int key_change_num;                     /*!< Number of key changes */
    uint32_t key_pressed_num;               /*!< Number of keys pressed */
    uint32_t key_release_num;               /*!< Number of keys released */
    keyboard_btn_data_t *key_data;          /*!< Array, contains key codes */
    keyboard_btn_data_t *key_release_data;  /*!< Array, contains key codes */
} keyboard_btn_report_t;

/**
 * @brief keyboard button event data
 *
 */
typedef union {
    /**
     * @brief combination event data
     * eg: Set key_data = {(1,1), (2,2)} means
     * that the button sequence is (1,1) -> (2,2)
     */
    struct combination_t {
        uint32_t key_num;                 /*!< Number of keys */
        keyboard_btn_data_t *key_data;    /*!< Array, contains key codes by index. The button sequence is also provided through this */
    } combination;                        /*!< combination event */
} keyboard_btn_event_data_t;

/**
 * @brief keyboard handle type
 *
 */
typedef struct keyboard_btn_t *keyboard_btn_handle_t;

/**
 * @brief keyboard callback handle for unregister
 *
 */
typedef void *keyboard_btn_cb_handle_t;

typedef void (*keyboard_btn_callback_t)(keyboard_btn_handle_t kbd_handle, keyboard_btn_report_t kbd_report, void *user_data);

/**
 * @brief keybaord button callback config
 *
 */
typedef struct {
    keyboard_btn_event_t event;           /*!< Event type */
    keyboard_btn_event_data_t event_data; /*!< Event data */
    keyboard_btn_callback_t callback;     /*!< Callback function */
    void *user_data;                      /*!< Callback user data */
} keyboard_btn_cb_config_t;

/**
 * @brief keyboard button config
 *
 */
typedef struct {
    const int *output_gpios;          /*!< Array, contains output GPIO numbers used by rom/col line */
    const int *input_gpios;           /*!< Array, contains input GPIO numbers used by rom/col line */
    uint32_t output_gpio_num;         /*!< output_gpios array size */
    uint32_t input_gpio_num;          /*!< input_gpios array size */
    uint32_t active_level;            /*!< active level for the input gpios */
    uint32_t debounce_ticks;          /*!< debounce time in ticks */
    uint32_t ticks_interval;          /*!< interval time in us */
    bool enable_power_save;           /*!< enable power save mode */
    UBaseType_t priority;             /*!< FreeRTOS task priority */
    BaseType_t core_id;               /*!< ESP32 core ID */
} keyboard_btn_config_t;

/**
 * @brief Create a keyboard instance
 *
 * @param kbd_cfg keyboard configuration
 * @param kbd_handle keyboard handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_NO_MEM        No more memory allocation.
 */
esp_err_t keyboard_button_create(keyboard_btn_config_t *kbd_cfg, keyboard_btn_handle_t *kbd_handle);

/**
 * @brief Delete the keyboard instance
 *
 * @param kbd_handle keyboard handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t keyboard_button_delete(keyboard_btn_handle_t kbd_handle);

/**
 * @brief Register the button callback function
 *
 * @param kbd_handle keyboard handle
 * @param cb_cfg callback configuration
 * @param rtn_cb_hdl callback handle for unregister
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_NO_MEM        No more memory allocation for the event
 */
esp_err_t keyboard_button_register_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_cb_config_t cb_cfg, keyboard_btn_cb_handle_t *rtn_cb_hdl);

/**
 * @brief Unregister the button callback function
 *
 * @note  If only the event is provided, all callbacks associated with this event will be canceled.
 *        If rtn_cb_hdl is provided, only the specified callback will be unregistered.
 * @param kbd_handle keyboard handle
 * @param event event type
 * @param rtn_cb_hdl callback handle for unregister
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_NO_MEM        No more memory allocation for the event
 */
esp_err_t keyboard_button_unregister_cb(keyboard_btn_handle_t kbd_handle, keyboard_btn_event_t event, keyboard_btn_cb_handle_t rtn_cb_hdl);

/**
 * @brief Get index by gpio number
 *
 * @param kbd_handle keyboard handle
 * @param gpio_num gpio number
 * @param gpio_mode gpio mode, input or output
 * @param index return index
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_NOT_FOUND     The gpio number is not found.
 */
esp_err_t keyboard_button_get_index_by_gpio(keyboard_btn_handle_t kbd_handle, uint32_t gpio_num, kbd_gpio_mode_t gpio_mode, uint32_t *index);

/**
 * @brief Get gpio number by index
 *
 * @param kbd_handle keyboard handle
 * @param index index
 * @param gpio_mode gpio mode, input or output
 * @param gpio_num return gpio number
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 *      - ESP_ERR_NOT_FOUND     The index is not found.
 */
esp_err_t keyboard_button_get_gpio_by_index(keyboard_btn_handle_t kbd_handle, uint32_t index, kbd_gpio_mode_t gpio_mode, uint32_t *gpio_num);

#ifdef __cplusplus
}
#endif
