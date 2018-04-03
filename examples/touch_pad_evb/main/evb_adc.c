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

/*
 * The divided resistance on the motherboard is 10 KΩ.
 * The table below lists the divided resistance on each daughterboard.
 * +---------------+---------------------------+-------------------+-------------------+
 * |Daughter Board | Divided resistance (Kohm) | ADC reading (Min) | ADC reading (Max) |
 * +---------------+---------------------------+-------------------+-------------------+
 * | Spring Button |              0            |          0        |        250        |
 * +---------------+---------------------------+-------------------+-------------------+
 * | Linear Slider |            4.7            |        805        |       1305        |
 * +---------------+---------------------------+-------------------+-------------------+
 * | Matrix Button |             10            |       1400        |       1900        |
 * +---------------+---------------------------+-------------------+-------------------+
 * | Duplex Slider |           19.1            |       1916        |       2416        |
 * +---------------+---------------------------+-------------------+-------------------+
 * |   Touch Wheel |             47            |       2471        |       2971        |
 * +---------------+---------------------------+-------------------+-------------------+
 */
#define TOUCH_SPRING_THRESH_LOWER    0
#define TOUCH_SPRING_THRESH_UPPER    250
#define TOUCH_SLIDE_THRESH_LOWER     805
#define TOUCH_SLIDE_THRESH_UPPER     1305
#define TOUCH_MATRIX_THRESH_LOWER    1400
#define TOUCH_MATRIX_THRESH_UPPER    1900
#define TOUCH_SEQ_SLIDE_THRESH_LOWER 1916
#define TOUCH_SEQ_SLIDE_THRESH_UPPER 2416
#define TOUCH_CIRCLE_THRESH_LOWER    2471
#define TOUCH_CIRCLE_THRESH_UPPER    2971

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Two Point: Supported");
    } else {
        ESP_LOGD(TAG, "eFuse Two Point: NOT supported");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Vref: Supported");
    } else {
        ESP_LOGD(TAG, "eFuse Vref: NOT supported");
    }
}

void evb_adc_init(void)
{
    check_efuse();
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_DB_11);
}

int evb_adc_get_mode(void)
{
    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, V_REF, &characteristics);

    while (1) {
        uint32_t adc_reading = adc1_get_raw((adc1_channel_t)ADC1_TEST_CHANNEL);
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &characteristics);
        ESP_LOGI(TAG, "ADC read voltage: %d mV", voltage);
        if (voltage < TOUCH_SLIDE_THRESH_UPPER && voltage >= TOUCH_SLIDE_THRESH_LOWER) {
            ESP_LOGI(TAG, "Daughter Board is Linear Slider");
            return TOUCH_EVB_MODE_SLIDE;
        } else if (voltage < TOUCH_MATRIX_THRESH_UPPER && voltage > TOUCH_MATRIX_THRESH_LOWER) {
            ESP_LOGI(TAG, "Daughter Board is Matrix Button");
            return TOUCH_EVB_MODE_MATRIX;
        } else if (voltage < TOUCH_SPRING_THRESH_UPPER && voltage > TOUCH_SPRING_THRESH_LOWER) {
            ESP_LOGI(TAG, "Daughter Board is Spring Button");
            return TOUCH_EVB_MODE_SPRING;
        } else if (voltage < TOUCH_CIRCLE_THRESH_UPPER && voltage > TOUCH_CIRCLE_THRESH_LOWER) {
            ESP_LOGI(TAG, "Daughter Board is Touch Wheel");
            return TOUCH_EVB_MODE_CIRCLE;
        } else if (voltage < TOUCH_SEQ_SLIDE_THRESH_UPPER && voltage > TOUCH_SEQ_SLIDE_THRESH_LOWER) {
            ESP_LOGI(TAG, "Daughter Board is Duplex Slider");
            return TOUCH_EVB_MODE_SEQ_SLIDE;
        } else {
            ESP_LOGE(TAG, "Don't find daughter Board...");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    }
}

