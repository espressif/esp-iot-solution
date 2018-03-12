/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#ifndef _TOUCH_EVB_
#define _TOUCH_EVB_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "iot_touchpad.h"
#include "driver/adc.h"
#include "iot_led.h"

#define SCOPE_DEBUG               0     /**< 1: Use Serial Digital Scope to debug the Touch Sensor */

/* Daughter board plastic cover thickness setting */
#define COVER_THICK_SPRING_BUTTON 3     /**< Spring Button 0 mm ~ 3 mm */
#define COVER_THICK_LINEAR_SLIDER 0     /**< Linear Slider 0 mm ~ 1 mm */
#define COVER_THICK_MATRIX_BUTTON 0     /**< Matrix Button 0 mm ~ 1 mm */
#define COVER_THICK_DUPLEX_SLIDER 0     /**< Duplex Slider 0 mm ~ 1 mm */
#define COVER_THICK_TOUCH_WHEEL   0     /**< Touch Wheel   0 mm ~ 1 mm */

/*
 * Spring Button threshold settings. This define is millesimal.
 * if define n, the trigger threshold is (no_touch_value * n / 1000).
 * */
#if COVER_THICK_SPRING_BUTTON == 0                  /**< No Cover */
    #define SPRING_BUTTON_THRESH_PERCENT  870       /**< Threshold */
    #define SPRING_BUTTON_FILTER_VALUE    150
#elif COVER_THICK_SPRING_BUTTON <= 1                /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define SPRING_BUTTON_THRESH_PERCENT  970
    #define SPRING_BUTTON_FILTER_VALUE    50
#elif COVER_THICK_SPRING_BUTTON <= 2                /*!< Plastic cover thickness is 1 ~ 2 mm */
    #define SPRING_BUTTON_THRESH_PERCENT  980
    #define SPRING_BUTTON_FILTER_VALUE    20
#elif COVER_THICK_SPRING_BUTTON <= 3                /*!< Plastic cover thickness is 2 ~ 3 mm */
    #define SPRING_BUTTON_THRESH_PERCENT  985
    #define SPRING_BUTTON_FILTER_VALUE    20
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Linear slider threshold settings. This define is millesimal.
 * if define n, the trigger threshold is (no_touch_value * n / 1000).
 * */
#if COVER_THICK_LINEAR_SLIDER == 0                  /**< No Cover */
    #define LINEAR_SLIDER_THRESH_PERCENT  800
    #define LINEAR_SLIDER_FILTER_VALUE    20
    #define TOUCH_SLIDE_PAD_SCALE         5
#elif COVER_THICK_LINEAR_SLIDER <= 1                /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define LINEAR_SLIDER_THRESH_PERCENT  980
    #define LINEAR_SLIDER_FILTER_VALUE    20
    #define TOUCH_SLIDE_PAD_SCALE         5
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Matrix Button threshold settings. This define is millesimal.
 * if define n, the trigger threshold is (no_touch_value * n / 1000).
 * */
#if COVER_THICK_MATRIX_BUTTON == 0                  /**< No Cover */
    #define MATRIX_BUTTON_THRESH_PERCENT  950
    #define MATRIX_BUTTON_FILTER_VALUE    50
#elif COVER_THICK_MATRIX_BUTTON <= 1                /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define MATRIX_BUTTON_THRESH_PERCENT  980
    #define MATRIX_BUTTON_FILTER_VALUE    20
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Duplex Slider threshold settings. This define is millesimal.
 * if define n, the trigger threshold is (no_touch_value * n / 1000).
 * */
#if COVER_THICK_DUPLEX_SLIDER == 0                  /**< No Cover */
    #define DUPLEX_SLIDER_THRESH_PERCENT  850
    #define DUPLEX_SLIDER_FILTER_VALUE    20
    #define TOUCH_SEQ_SLIDE_PAD_SCALE     5
#elif COVER_THICK_DUPLEX_SLIDER <= 1                /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define DUPLEX_SLIDER_THRESH_PERCENT  990
    #define DUPLEX_SLIDER_FILTER_VALUE    20
    #define TOUCH_SEQ_SLIDE_PAD_SCALE     5
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Touch Wheel threshold settings. This define is millesimal.
 * if define n, the trigger threshold is (no_touch_value * n / 1000).
 * */
#if COVER_THICK_TOUCH_WHEEL == 0                  /**< No Cover */
    #define TOUCH_WHEEL_THRESH_PERCENT  800
    #define TOUCH_WHEEL_FILTER_VALUE    20
#elif COVER_THICK_TOUCH_WHEEL <= 1                /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define TOUCH_WHEEL_THRESH_PERCENT  980
    #define TOUCH_WHEEL_FILTER_VALUE    20
#else
    #error "Invalid setting of plastic cover thickness."
#endif

#ifdef __cplusplus
extern QueueHandle_t q_touch;
extern "C" {
#endif

#define V_REF   1100
#define ADC1_TEST_CHANNEL (ADC1_CHANNEL_7)

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_22
#define I2C_MASTER_SCL_IO GPIO_NUM_23
#define I2C_MASTER_FREQ_HZ 100000

enum {
    TOUCH_EVB_MODE_MATRIX,
    TOUCH_EVB_MODE_SLIDE,
    TOUCH_EVB_MODE_SPRING,
    TOUCH_EVB_MODE_CIRCLE,
    TOUCH_EVB_MODE_SEQ_SLIDE,
};

typedef enum {
    TOUCH_EVT_TYPE_SINGLE,
    TOUCH_EVT_TYPE_SINGLE_PUSH,
    TOUCH_EVT_TYPE_SINGLE_RELEASE,
    TOUCH_EVT_TYPE_SPRING_PUSH,
    TOUCH_EVT_TYPE_SPRING_RELEASE,
    TOUCH_EVT_TYPE_MATRIX,
    TOUCH_EVT_TYPE_MATRIX_RELEASE,
    TOUCH_EVT_TYPE_MATRIX_SERIAL,
    TOUCH_EVT_TYPE_SLIDE,
} touch_evt_type_t;

typedef struct {
    touch_evt_type_t type;
    union {
        struct {
            int idx;
        } single;
        struct {
            int x;
            int y;
        } matrix;
    };
} touch_evt_t;

/*
 * 7-segment display APIs
 */
void ch450_write_led(int mask, int on_off);
void ch450_write_dig(int idx, int val);
void ch450_write_dig_reverse(int idx, int num);
void ch450_dev_init();

/*
 * ADC APIs
 */
void evb_adc_init();
int  evb_adc_get_mode();

/*
 * RGB conponent API
 */
void evb_rgb_led_init();
void evb_rgb_led_clear();
uint8_t evb_rgb_led_color_get();
void evb_rgb_led_set(uint8_t color, uint8_t bright_percent);
void evb_rgb_led_get(uint8_t color, uint8_t *bright_percent);

/*
 * Touch button APIs
 */
void evb_touch_button_init();
void evb_touch_button_handle(int idx, int type);

/*
 * Touch matrix APIs
 */
void evb_touch_matrix_init();
void evb_touch_matrix_handle(int x, int y, int type);

/*
 * Touch spring APIs
 */
void evb_touch_spring_handle(int idx, int type);
void evb_touch_spring_init();

/*
 * Touch slide APIs
 */
void evb_touch_slide_init_then_run();
void evb_touch_wheel_init_then_run();
void evb_touch_seq_slide_init_then_run();

#ifdef __cplusplus
}
#endif

#endif
