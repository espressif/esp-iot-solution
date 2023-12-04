/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <ctype.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "hidd.h"

#define ADC_UNIT                    ADC_UNIT_1

#define JOYSTICK_IN_X_ADC_CHANNEL   ADC_CHANNEL_0
#define JOYSTICK_IN_Y_ADC_CHANNEL   ADC_CHANNEL_1

#define ADC_BITWIDTH                ADC_BITWIDTH_DEFAULT
#define ADC_ATTEN                   ADC_ATTEN_DB_11

#define ADC_RAW_MAX 4095

#if CONFIG_BUTTON_INPUT_MODE_BOOT
// If without external hardware, the boot button serves as button A
#define PIN_BUTTON_BOOT CONFIG_BOARD_BUTTON_GPIO
#else // CONFIG_BUTTON_INPUT_MODE_GPIO
#define PIN_BUTTON_A CONFIG_BUTTON_PIN_A
#define PIN_BUTTON_B CONFIG_BUTTON_PIN_B
#define PIN_BUTTON_C CONFIG_BUTTON_PIN_C
#define PIN_BUTTON_D CONFIG_BUTTON_PIN_D
#endif

#ifdef CONFIG_BUTTON_INPUT_MODE_BOOT
#define BUTTON_PIN_BIT_MASK (1ULL<<PIN_BUTTON_BOOT)
#else // CONFIG_BUTTON_INPUT_MODE_GPIO
#define BUTTON_PIN_BIT_MASK ((1ULL<<PIN_BUTTON_A) | (1ULL<<PIN_BUTTON_B) | (1ULL<<PIN_BUTTON_C) | (1ULL<<PIN_BUTTON_D))
#endif

#define DELAY(x) vTaskDelay(x / portTICK_PERIOD_MS)

// 10% threshold to trigger joystick input event when using external hardware
#define JOYSTICK_THRESHOLD (UINT8_MAX / 10)

// 30 milliseconds debounce time
#define DEBOUNCE_TIME_US 30000

/**
 * @brief   Enum to differentiate input events
 * @details 3 types of events: button interrupts, console input, and joystick input
 * @see button_isr_handler()
 * @see joystick_console_read()
 * @see joystick_ext_read()
 */
enum {
    INPUT_SOURCE_BUTTON = 0,
    INPUT_SOURCE_CONSOLE = 1,
    INPUT_SOURCE_JOYSTICK = 2,
};

/**
 * @brief   Simple struct to store input event data in RTOS queue
 * @note    This struct serves as an simplified implementation
 *          Up to user modify for more complex use cases or performance
*/
typedef struct {
    uint8_t input_source;
    uint8_t data_button;
    uint8_t data_console;
    uint8_t data_joystick_x;
    uint8_t data_joystick_y;
} input_event_t;

/**
 * @brief   Initialize resources for button input
*/
esp_err_t button_init(void);

/**
 * @brief   Read button input values
 *
 * @details     This function returns a 8 bit value to represent button input
 *              The least significant 4 bits represent button A (LSB), B, C, D
 * @details     Implementation depends on the HID report descriptor (see hidd.c)
 * @return      8 bit value to represent button input (only 4 LSB are used in this example)
*/
uint8_t button_read(void);

/**
 * @brief   Deinitialize resources for button input
*/
esp_err_t button_deinit(void);

/**
 * @brief   Initialize resources for joystick input
*/
esp_err_t joystick_init(void);

/**
 * @brief  Print out help message for console input mode (to emulate joystick movement)
*/
void print_console_read_help(void);

/**
 * @brief   Convert user input to joystick input
 * @param[in]   user_input  User input character
 * @param[out]  x_axis      Pointer to store joystick x-axis value
 * @param[out]  y_axis      Pointer to store joystick y-axis value
*/
void char_to_joystick_input(uint8_t user_input, uint8_t *x_axis, uint8_t *y_axis);

/**
 * @brief   Read user input from console
 * @details This function will send joystick input event to RTOS queue
*/
void joystick_console_read(void *args);

/**
 * @brief   Read joystick input from external joystick hardware
 * @details This function will send joystick input event to RTOS queue
*/
void joystick_ext_read(void *args);

/**
 * @brief   Read joystick input values from ADC pins
 * @param[out]  x_axis  Pointer to store joystick x-axis value
 * @param[out]  y_axis  Pointer to store joystick y-axis value
*/
void joystick_adc_read(uint8_t *x_axis, uint8_t *y_axis);

/**
 * @brief   Deinitialize resources for joystick input
*/
esp_err_t joystick_deinit(void);
