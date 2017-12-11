/* Touch Sensor Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "evb.h"
static const char* TAG = "evb_adc";

#if CONFIG_TOUCH_EB_V1
#define TOUCH_SLIDE_THRESH_UPPER     600
#define TOUCH_SLIDE_THRESH_LOWER     300
#define TOUCH_MATRIX_THRESH_UPPER    200
#define TOUCH_MATRIX_THRESH_LOWER    0
#elif CONFIG_TOUCH_EB_V2
#define TOUCH_SLIDE_THRESH_UPPER     350
#define TOUCH_SLIDE_THRESH_LOWER     300
#define TOUCH_MATRIX_THRESH_UPPER    500
#define TOUCH_MATRIX_THRESH_LOWER    450
#define TOUCH_SPRING_THRESH_UPPER    80
#define TOUCH_SPRING_THRESH_LOWER    30
#define TOUCH_CIRCLE_THRESH_UPPER    820
#define TOUCH_CIRCLE_THRESH_LOWER    780
#define TOUCH_SEQ_SLIDE_THRESH_UPPER  700
#define TOUCH_SEQ_SLIDE_THRESH_LOWER  650
#elif CONFIG_TOUCH_EB_V3
#define TOUCH_SLIDE_THRESH_UPPER     1175
#define TOUCH_SLIDE_THRESH_LOWER     935
#define TOUCH_MATRIX_THRESH_UPPER    1770
#define TOUCH_MATRIX_THRESH_LOWER    1530
#define TOUCH_SPRING_THRESH_UPPER    120
#define TOUCH_SPRING_THRESH_LOWER    0
#define TOUCH_CIRCLE_THRESH_UPPER    2810
#define TOUCH_CIRCLE_THRESH_LOWER    2630
#define TOUCH_SEQ_SLIDE_THRESH_UPPER 2620
#define TOUCH_SEQ_SLIDE_THRESH_LOWER 2400
#endif

void evb_adc_init()
{
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_11db);
}

int evb_adc_get_mode()
{
    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_11db, ADC_WIDTH_12Bit, &characteristics);

    while (1) {
        uint32_t voltage = adc1_to_voltage(ADC1_TEST_CHANNEL, &characteristics);
        ESP_LOGI(TAG, "%d mV", voltage);
        if (voltage < TOUCH_SLIDE_THRESH_UPPER && voltage > TOUCH_SLIDE_THRESH_LOWER) {
            ESP_LOGI(TAG, "SLIDE MODE...");
            return TOUCH_EVB_MODE_SLIDE;
        } else if (voltage < TOUCH_MATRIX_THRESH_UPPER && voltage > TOUCH_MATRIX_THRESH_LOWER) {
            ESP_LOGI(TAG, "MATRIX MODE...");
            return TOUCH_EVB_MODE_MATRIX;
        }
#if (CONFIG_TOUCH_EB_V2 || CONFIG_TOUCH_EB_V3)
        else if (voltage < TOUCH_SPRING_THRESH_UPPER && voltage > TOUCH_SPRING_THRESH_LOWER) {
            ESP_LOGI(TAG, "SPRING MODE...");
            return TOUCH_EVB_MODE_SPRING;
        } else if (voltage < TOUCH_CIRCLE_THRESH_UPPER && voltage > TOUCH_CIRCLE_THRESH_LOWER) {
            ESP_LOGI(TAG, "CIRCLE MODE...");
            return TOUCH_EVB_MODE_CIRCLE;
        } else if (voltage < TOUCH_SEQ_SLIDE_THRESH_UPPER && voltage > TOUCH_SEQ_SLIDE_THRESH_LOWER) {
            ESP_LOGI(TAG, "SEQ SLIDE MODE...");
            return TOUCH_EVB_MODE_SEQ_SLIDE;
        }

#endif
        else {
            ESP_LOGE(TAG, "Unexpected ADC value...");
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

