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

/* Daughter board plastic cover thickness setting */
#define COVER_THICK_SPRING_BUTTON CONFIG_COVER_THICK_SPRING_BUTTON      /**< Spring Button 0 mm ~ 3 mm */
#define COVER_THICK_LINEAR_SLIDER CONFIG_COVER_THICK_LINEAR_SLIDER      /**< Linear Slider 0 mm ~ 1 mm */
#define COVER_THICK_MATRIX_BUTTON CONFIG_COVER_THICK_MATRIX_BUTTON      /**< Matrix Button 0 mm ~ 1 mm */
#define COVER_THICK_DUPLEX_SLIDER CONFIG_COVER_THICK_DUPLEX_SLIDER      /**< Duplex Slider 0 mm ~ 1 mm */
#define COVER_THICK_TOUCH_WHEEL   CONFIG_COVER_THICK_TOUCH_WHEEL        /**< Touch Wheel   0 mm ~ 1 mm */

/*
 * Spring button threshold settings.
 * stores the max change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_SPRING_BUTTON <= 1          /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define SPRING_BUTTON_MAX_CHANGE_RATE_0    0.1129   // (1196-1061) / 1196 = 0.1129
    #define SPRING_BUTTON_MAX_CHANGE_RATE_1    0.1029   // (1215-1090) / 1215 = 0.1029
    #define SPRING_BUTTON_MAX_CHANGE_RATE_2    0.0950   // (1053-953 ) / 1053 = 0.0950
    #define SPRING_BUTTON_MAX_CHANGE_RATE_3    0.0856   // (1110-1015) / 1110 = 0.0856
    #define SPRING_BUTTON_MAX_CHANGE_RATE_4    0.0883   // (1132-1032) / 1132 = 0.0883
    #define SPRING_BUTTON_MAX_CHANGE_RATE_5    0.0862   // (986 -901 ) / 986  = 0.0862
#elif COVER_THICK_SPRING_BUTTON <= 2        /*!< Plastic cover thickness is 1 ~ 2 mm */
    #define SPRING_BUTTON_MAX_CHANGE_RATE_0    0.0627   // (1196-1121) / 1196 = 0.0627
    #define SPRING_BUTTON_MAX_CHANGE_RATE_1    0.0535   // (1215-1150) / 1215 = 0.0535
    #define SPRING_BUTTON_MAX_CHANGE_RATE_2    0.0522   // (1053-998 ) / 1053 = 0.0522
    #define SPRING_BUTTON_MAX_CHANGE_RATE_3    0.0450   // (1110-1060) / 1110 = 0.0450
    #define SPRING_BUTTON_MAX_CHANGE_RATE_4    0.0486   // (1132-1077) / 1132 = 0.0486
    #define SPRING_BUTTON_MAX_CHANGE_RATE_5    0.0406   // (986 - 946) / 986  = 0.0406
#elif COVER_THICK_SPRING_BUTTON <= 3        /*!< Plastic cover thickness is 2 ~ 3 mm */
    #define SPRING_BUTTON_MAX_CHANGE_RATE_0    0.0485   // (1196-1138) / 1196 = 0.0485
    #define SPRING_BUTTON_MAX_CHANGE_RATE_1    0.0453   // (1215-1160) / 1215 = 0.0453
    #define SPRING_BUTTON_MAX_CHANGE_RATE_2    0.0408   // (1053-1010) / 1053 = 0.0408
    #define SPRING_BUTTON_MAX_CHANGE_RATE_3    0.0414   // (1110-1064) / 1110 = 0.0414
    #define SPRING_BUTTON_MAX_CHANGE_RATE_4    0.0424   // (1132-1084) / 1132 = 0.0424
    #define SPRING_BUTTON_MAX_CHANGE_RATE_5    0.0396   // (986 -947 ) / 986  = 0.0396
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Linear slider threshold settings. This define is absolute value.
 * stores the change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_LINEAR_SLIDER == 0      /**< No Cover */
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_0   0.57      // (1158-658) / 1158 = 0.57
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_1   0.53      // (1053-553) / 1053 = 0.53
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_2   0.51      // (1023-523) / 1023 = 0.51
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_0    0.31      // (1158-358) / 1158 = 0.31
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_1    0.34      // (1210-410) / 1210 = 0.34
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_2    0.35      // (1222-422) / 1222 = 0.35
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_3    0.47      // (1122-522) / 1122 = 0.47
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_4    0.53      // (1065-565) / 1065 = 0.53
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_5    0.51      // (1030-530) / 1030 = 0.51
    #define TOUCH_SLIDE_PAD_RANGE            255       // the position range of slide
#elif COVER_THICK_LINEAR_SLIDER <= 1    /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_0   0.0777    // (1158-1068) / 1158 = 0.0777
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_1   0.0854    // (1053- 963) / 1053 = 0.0854
    #define TOUCH_BUTTON_MAX_CHANGE_RATE_2   0.0880    // (1023- 933) / 1023 = 0.0880
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_0    0.0820    // (1158-1063) / 1158 = 0.0820
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_1    0.0744    // (1210-1120) / 1210 = 0.0744
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_2    0.0736    // (1222-1132) / 1222 = 0.0736
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_3    0.0802    // (1122-1032) / 1122 = 0.0802
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_4    0.0798    // (1065- 980) / 1065 = 0.0798
    #define TOUCH_SLIDE_MAX_CHANGE_RATE_5    0.0777    // (1030- 950) / 1030 = 0.0777
    #define TOUCH_SLIDE_PAD_RANGE            150       // the position range of slide
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Matrix Button threshold settings. This define is absolute value.
 * stores the max change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_MATRIX_BUTTON == 0  /**< No Cover */
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X0    0.62         // (884 - 334) / 884 = 0.62
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X1    0.55         // (993 - 443) / 550 = 0.55
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X2    0.53         // (950 - 450) / 950 = 0.53
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y0    0.46         // (871 - 471) / 871 = 0.46
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y1    0.38         // (790 - 490) / 790 = 0.38
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y2    0.42         // (714 - 414) / 714 = 0.42
#elif COVER_THICK_MATRIX_BUTTON <= 1    /*!< Plastic cover thickness is 0 ~ 1 mm */
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X0    0.0452       // (884 - 844) / 884 = 0.0452
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X1    0.0503       // (993 - 943) / 550 = 0.0503
#define TOUCH_MATRIX_MAX_CHANGE_RATE_X2    0.0421       // (950 - 910) / 950 = 0.0421
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y0    0.0401       // (871 - 836) / 871 = 0.0401
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y1    0.0340       // (790 - 760) / 790 = 0.0340
#define TOUCH_MATRIX_MAX_CHANGE_RATE_Y2    0.0378       // (714 - 687) / 714 = 0.0378
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Duplex Slider threshold settings. This define is absolute value.
 * stores the max change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_DUPLEX_SLIDER == 0  /**< No Cover */
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_0    0.52     //(835 - 400) / 835 = 0.52
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_1    0.46     //(846 - 460) / 846 = 0.46
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_2    0.42     //(796 - 460) / 796 = 0.42
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_3    0.43     //(920 - 520) / 920 = 0.43
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_4    0.44     //(899 - 500) / 899 = 0.44
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_5    0.40     //(903 - 540) / 903 = 0.40
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_6    0.39     //(849 - 520) / 849 = 0.39
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_7    0.47     //(904 - 480) / 904 = 0.47
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_8    0.35     //(835 - 540) / 835 = 0.35
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_9    0.43     //(920 - 520) / 920 = 0.43
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_10   0.39     //(849 - 520) / 849 = 0.39
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_11   0.32     //(846 - 540) / 846 = 0.32
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_12   0.40     //(899 - 540) / 899 = 0.40
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_13   0.42     //(904 - 520) / 904 = 0.42
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_14   0.42     //(796 - 460) / 796 = 0.42
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_15   0.49     //(903 - 460) / 903 = 0.49
    #define TOUCH_SEQ_SLIDE_PAD_RANGE          255      // the position range of slide
#elif COVER_THICK_DUPLEX_SLIDER <= 1    /*!< Plastic cover thickness is 0 ~ 1 mm */
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_0    0.0455   //(835 - 797) / 835 = 0.0455
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_1    0.0355   //(846 - 816) / 835 = 0.0355
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_2    0.0427   //(796 - 762) / 835 = 0.0427
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_3    0.0489   //(920 - 875) / 835 = 0.0489
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_4    0.0445   //(899 - 859) / 835 = 0.0445
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_5    0.0465   //(903 - 861) / 835 = 0.0465
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_6    0.0530   //(849 - 804) / 835 = 0.0530
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_7    0.0476   //(904 - 861) / 835 = 0.0476
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_8    0.0539   //(835 - 790) / 835 = 0.0539
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_9    0.0534   //(920 - 870) / 835 = 0.0534
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_10   0.0448   //(849 - 811) / 835 = 0.0448
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_11   0.0473   //(846 - 806) / 835 = 0.0473
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_12   0.0445   //(899 - 859) / 835 = 0.0445
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_13   0.0332   //(904 - 874) / 835 = 0.0332
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_14   0.0503   //(796 - 756) / 835 = 0.0503
    #define DUPLEX_SLIDER_MAX_CHANGE_RATE_15   0.0443   //(903 - 863) / 835 = 0.0443
    #define TOUCH_SEQ_SLIDE_PAD_RANGE          150      // the position range of slide
#else
    #error "Invalid setting of plastic cover thickness."
#endif

/*
 * Touch Wheel threshold settings. This define is absolute value.
 * stores the max change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#if COVER_THICK_TOUCH_WHEEL == 0    /**< No Cover */
#define WHEEL_SLIDER_MAX_CHANGE_RATE_0     0.63         //(1070 - 400) / 1070 = 0.63
#define WHEEL_SLIDER_MAX_CHANGE_RATE_1     0.64         //(1252 - 450) / 1252 = 0.64
#define WHEEL_SLIDER_MAX_CHANGE_RATE_2     0.69         //(1300 - 400) / 1300 = 0.69
#define WHEEL_SLIDER_MAX_CHANGE_RATE_3     0.65         //(1270 - 450) / 1270 = 0.65
#define WHEEL_SLIDER_MAX_CHANGE_RATE_4     0.67         //(1230 - 400) / 1230 = 0.67
#define WHEEL_SLIDER_MAX_CHANGE_RATE_5     0.67         //(1060 - 350) / 1060 = 0.67
#define WHEEL_SLIDER_MAX_CHANGE_RATE_6     0.63         //(1090 - 400) / 1090 = 0.63
#define WHEEL_SLIDER_MAX_CHANGE_RATE_7     0.53         //(860  - 400) / 860  = 0.53
#define WHEEL_SLIDER_MAX_CHANGE_RATE_8     0.64         //(970  - 350) / 970  = 0.64
#define TOUCH_WHEEL_PAD_RANGE              255          // the position range of slide
#elif COVER_THICK_TOUCH_WHEEL <= 1  /*!< Plastic cover thickness is 0 ~ 1 mm */
#define WHEEL_SLIDER_MAX_CHANGE_RATE_0     0.103        //(1070 - 970 ) / 1070 = 0.103
#define WHEEL_SLIDER_MAX_CHANGE_RATE_1     0.120        //(1252 - 1102) / 1252 = 0.120
#define WHEEL_SLIDER_MAX_CHANGE_RATE_2     0.192        //(1300 - 1050) / 1300 = 0.192
#define WHEEL_SLIDER_MAX_CHANGE_RATE_3     0.118        //(1270 - 1120) / 1270 = 0.118
#define WHEEL_SLIDER_MAX_CHANGE_RATE_4     0.130        //(1230 - 1070) / 1230 = 0.130
#define WHEEL_SLIDER_MAX_CHANGE_RATE_5     0.142        //(1060 - 910 ) / 1060 = 0.142
#define WHEEL_SLIDER_MAX_CHANGE_RATE_6     0.119        //(1090 - 960 ) / 1090 = 0.119
#define WHEEL_SLIDER_MAX_CHANGE_RATE_7     0.105        //(860  - 770 ) / 860  = 0.105
#define WHEEL_SLIDER_MAX_CHANGE_RATE_8     0.103        //(970  - 870 ) / 970  = 0.103
#define TOUCH_WHEEL_PAD_RANGE              150          // the position range of slide
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
    TOUCH_EVB_MODE_MAX,
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
