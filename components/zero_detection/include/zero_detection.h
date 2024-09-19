/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ZERO_DETECTION_H_
#define _ZERO_DETECTION_H_

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "hal/gpio_ll.h"
#if defined(CONFIG_USE_GPTIMER)
#include "driver/gptimer.h"
#endif
#include "esp_timer.h"

#include "soc/gpio_struct.h"
#include "esp_log.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "zero_detection.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZERO_DETECTION_INIT_CONFIG_DEFAULT() { \
    .valid_times = 6, \
    .invalid_times = 5, \
    .signal_lost_time_us = 100000, \
    .freq_range_min_hz = 45,\
    .freq_range_max_hz = 65,\
    .capture_pin = 2,\
    .zero_signal_type = SQUARE_WAVE,\
    .zero_driver_type = GPIO_TYPE,\
}

/**
 * @brief Zero detection events
 */
typedef enum {
    SIGNAL_FREQ_OUT_OF_RANGE = 0,  //Event when the current signal frequency exceeds the set range
    SIGNAL_VALID,   //Event when the frequency of the current signal is within the range, and the number of occurrences surpasses the user-set limit
    SIGNAL_INVALID, //Event when the frequency of the current signal exceeds the range, and the number of occurrences surpasses the user-set limit
    SIGNAL_LOST,    //Event occurring when no signal is received within 100ms
    SIGNAL_RISING_EDGE,            //Event on the rising edge of the signal
    SIGNAL_FALLING_EDGE,           //Event on the falling edge of the signal
} zero_detect_event_t;

/**
 * @brief Zero detection wave type
 */
typedef enum {
    SQUARE_WAVE = 0,
    PULSE_WAVE,
} zero_signal_type_t;

/**
 * @brief Zero detection signal edge type
 */
typedef enum {
    CAP_EDGE_POS = 0,
    CAP_EDGE_NEG,
} zero_signal_edge_t;

/**
 * @brief Zero detection driver type
 */
typedef enum {
#if SOC_MCPWM_SUPPORTED
    MCPWM_TYPE = 0,  /*!< Use MCPWM as driver */
#endif
    GPIO_TYPE,       /*!< Use GPIO as driver */
} zero_driver_type_t;

/**
 * @brief Event callback parameters union
 */
typedef union {
    /**
     * @brief Signal exceeds frequency range data return type
     */
    struct signal_freq_event_data_t {
        zero_signal_edge_t cap_edge; /*!< Trigger edge of zero cross signal */
        uint32_t full_cycle_us;        /*!< Current signal cycle */
    } signal_freq_event_data;          /*!< Signal freq event data struct */

    /**
     * @brief Signal valid data return type
     */
    struct signal_valid_event_data_t {
        zero_signal_edge_t cap_edge; /*!< Trigger edge of zero cross signal */
        uint32_t full_cycle_us;        /*!< Current signal cycle */
        uint16_t valid_count;          /*!< Counting when the signal is valid */
    } signal_valid_event_data;         /*!< Signal valid event data struct */

    /**
     * @brief Signal invalid data return type
     */
    struct signal_invalid_event_data_t {
        zero_signal_edge_t cap_edge; /*!< Trigger edge of zero cross signal */
        uint32_t full_cycle_us;        /*!< Current signal cycle */
        uint16_t invalid_count;        /*!< Counting when the signal is invalid */
    } signal_invalid_event_data;       /*!< Signal invalid event data struct */

    /**
     * @brief Signal rising edge data return type
     */
    struct signal_rising_edge_event_data_t {
        uint16_t valid_count;          /*!< Counting when the signal is valid */
        uint16_t invalid_count;        /*!< Counting when the signal is invalid */
        uint32_t full_cycle_us;        /*!< Current signal cycle */
    } signal_rising_edge_event_data;   /*!< Signal rising edge event data struct */

    /**
     * @brief Signal falling edge data return type
     */
    struct signal_falling_edge_event_data_t {
        uint16_t valid_count;          /*!< Counting when the signal is valid */
        uint16_t invalid_count;        /*!< Counting when the signal is invalid */
        uint32_t full_cycle_us;        /*!< Current signal cycle */
    } signal_falling_edge_event_data;  /*!< Signal falling edge event_data */
} zero_detect_cb_param_t;

/**
 * @brief Callback format
 */
typedef void (*zero_cross_cb_t)(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data);

/**
 * @brief User config data type
 */
typedef struct {
    int32_t capture_pin;            /*!< GPIO number for zero cross detect capture */
    uint16_t valid_times;            /*!< Minimum required number of times for detecting signal validity */
    uint16_t invalid_times;          /*!< Minimum required number of times for detecting signal invalidity */
    uint64_t signal_lost_time_us;      /*!< Minimum required duration for detecting signal loss */
    zero_signal_type_t zero_signal_type;          /*!< Zero Crossing Signal type */
    zero_driver_type_t zero_driver_type;          /*!< Zero crossing driver type */
    double freq_range_max_hz;       /*!< Maximum value of the frequency range when determining a valid signal */
    double freq_range_min_hz;       /*!< Minimum value of the frequency range when determining a valid signal */
} zero_detect_config_t;

typedef void *zero_detect_handle_t;

/**
 * @brief Create a zero detect target
 *
 * @param config A zero detect object to config
 *
 * @return
 *      - zero_detect_handle_t  A zero cross detection object
 */
zero_detect_handle_t zero_detect_create(zero_detect_config_t *config);

/**
 * @brief Show zero detect test data
 *
 * @param zcd_handle A zero detect handle
 *
 */
void zero_show_data(zero_detect_handle_t zcd_handle);

/**
 * @brief Delete a zero detect device
 *
 * @param zcd_handle A zero detect handle
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t zero_detect_delete(zero_detect_handle_t zcd_handle);

/**
 * @brief Get relay power status
 *
 * @param zcd_handle A zero detect handle
 *
 * @return
 *      - true  Power on
 *      - false Power off
 */
bool zero_detect_get_power_status(zero_detect_handle_t zcd_handle);

/**
 * @brief Get signal invalid status
 *
 * @param zcd_handle A zero detect handle
 *
 * @return
 *      - true  Signal is invalid
 *      - false Signal is valid
 */
bool zero_detect_singal_invaild_status(zero_detect_handle_t zcd_handle);

/**
 * @brief Get signal type
 *
 * @param zcd_handle A zero detect handle
 *
 * @return
 *      - SQUARE_WAVE Signal type is square
 *      - PULSE_WAVE  Signal type is pulse
 */
zero_signal_type_t zero_detect_get_signal_type(zero_detect_handle_t zcd_handle);

/**
 * @brief Register zero cross detection callback
 *
 * @param zcd_handle A zero detect handle
 * @param cb A callback function
 * @param usr_data User data
 *
 * @return
 *      - ESP_OK  Success
 *      - ESP_FAIL Failure
 */
esp_err_t zero_detect_register_cb(zero_detect_handle_t zcd_handle, zero_cross_cb_t cb, void *usr_data);

#ifdef __cplusplus
}
#endif

#endif
