// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdio.h>
#include "sdkconfig.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc_cal.h"
#include "iot_adc.h"

static const char *TAG = "iot_adc";

typedef struct {
    adc_channel_t channel;
    adc_atten_t atten;
    adc_unit_t unit;
    adc_bits_width_t bit_width;
    uint32_t vref;
    int sample_num;
    esp_adc_cal_characteristics_t *adc_chars;
} adc_dev_t;

static bool adc_handle_is_valid(adc_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "ADC object not initialzed properly");
        return false;
    }
    return true;
}

static void check_efuse()
{
    /* Check TP is burned into eFuse */
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Two Point: Supported");
    } else {
        ESP_LOGD(TAG, "eFuse Two Point: NOT supported");
    }

    /* Check Vref is burned into eFuse */
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGD(TAG, "eFuse Vref: Supported");
    } else {
        ESP_LOGD(TAG, "eFuse Vref: NOT supported");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "Characterized using eFuse Vref");
    } else {
        ESP_LOGI(TAG, "Characterized using Default Vref");
    }
}

esp_err_t iot_adc_update(adc_handle_t adc_handle, adc_atten_t atten, adc_bits_width_t bit_width, int vref_mv, int sample_num)
{
    if (!adc_handle_is_valid(adc_handle)) {
        return ESP_FAIL;
    }
    adc_dev_t *adc_dev = (adc_dev_t *) adc_handle;
    adc_dev->vref = vref_mv;
    adc_dev->sample_num = sample_num;
    /* Configure ADC */
    if (adc_dev->unit == ADC_UNIT_1) {
        adc1_config_width(adc_dev->bit_width);
        adc1_config_channel_atten((adc1_channel_t)adc_dev->channel, adc_dev->atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)adc_dev->channel, adc_dev->atten);
    }
    /* Characterize ADC */
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(adc_dev->channel, adc_dev->atten, adc_dev->bit_width, adc_dev->vref, adc_dev->adc_chars);
    print_char_val_type(val_type);

    return ESP_OK;
}

esp_err_t iot_adc_delete(adc_handle_t adc_handle)
{
    if (!adc_handle_is_valid(adc_handle)) {
        return ESP_FAIL;
    }
    adc_dev_t *adc_dev = (adc_dev_t *)adc_handle;
    free(adc_dev->adc_chars);
    adc_dev->adc_chars = NULL;
    free(adc_dev);
    return ESP_OK;
}

adc_handle_t iot_adc_create(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bits_width_t bit_width)
{

    adc_dev_t *adc_dev = (adc_dev_t *)calloc(1, sizeof(adc_dev_t));
    if (!adc_handle_is_valid(adc_dev)) {
        return NULL;
    }
    adc_dev->channel = channel;
    adc_dev->atten = atten;
    adc_dev->unit = unit;
    adc_dev->bit_width = bit_width;
    adc_dev->vref = DEFAULT_VREF;
    adc_dev->sample_num = NO_OF_SAMPLES;

    /* Check if Two Point or Vref are burned into eFuse */
    check_efuse();

    /* Configure ADC */
    if (unit == ADC_UNIT_1) {
        adc1_config_width(adc_dev->bit_width);
        adc1_config_channel_atten((adc1_channel_t)adc_dev->channel, adc_dev->atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)adc_dev->channel, adc_dev->atten);
    }

    /* Characterize ADC */
    adc_dev->adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(adc_dev->unit, adc_dev->atten, adc_dev->bit_width, adc_dev->vref, adc_dev->adc_chars);
    print_char_val_type(val_type);
    
    return (adc_handle_t) adc_dev;
}

int iot_adc_get_voltage(adc_handle_t adc_handle)
{
    if (!adc_handle_is_valid(adc_handle)) {
        return -1;
    }
    uint32_t adc_reading = 0;
    adc_dev_t *adc_dev = (adc_dev_t *)adc_handle;
    /* Multisampling */
    for (int i = 0; i < adc_dev->sample_num; i++) {
        if (adc_dev->unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)adc_dev->channel);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)adc_dev->channel, adc_dev->bit_width, &raw);
            adc_reading += raw;
        }
    }
    
    adc_reading /= adc_dev->sample_num;
    /* Convert adc_reading to voltage in mV */
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_dev->adc_chars);
    ESP_LOGD(TAG, "Raw ADC value: %d\tVoltage: %dmV", adc_reading, voltage);

    return voltage;
}

