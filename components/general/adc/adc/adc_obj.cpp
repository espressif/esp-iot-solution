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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "iot_adc.h"
#include "driver/gpio.h"
#include "driver/adc.h"

static const char* TAG = "ADC_OBJ";

typedef struct {
    int unit;
    int channel;
} adc_desc_t;

static const adc_desc_t adc_desc[GPIO_NUM_MAX] = {
        {ADC_UNIT_2, ADC2_CHANNEL_1}, // 0
        {-1, -1},
        {ADC_UNIT_2, ADC2_CHANNEL_2}, // 2
        {-1, -1},
        {ADC_UNIT_2, ADC2_CHANNEL_0}, // 4
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {ADC_UNIT_2, ADC2_CHANNEL_5}, // 12
        {ADC_UNIT_2, ADC2_CHANNEL_4}, // 13
        {ADC_UNIT_2, ADC2_CHANNEL_6}, // 14
        {ADC_UNIT_2, ADC2_CHANNEL_3}, // 15
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {ADC_UNIT_2, ADC2_CHANNEL_8}, // 25
        {ADC_UNIT_2, ADC2_CHANNEL_9}, // 26
        {ADC_UNIT_2, ADC2_CHANNEL_7}, // 27
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {-1, -1},
        {ADC_UNIT_1, ADC1_CHANNEL_4}, // 32
        {ADC_UNIT_1, ADC1_CHANNEL_5}, // 33
        {ADC_UNIT_1, ADC1_CHANNEL_6}, // 34
        {ADC_UNIT_1, ADC1_CHANNEL_7}, // 35
        {ADC_UNIT_1, ADC1_CHANNEL_0}, // 36
        {ADC_UNIT_1, ADC1_CHANNEL_1}, // 37
        {ADC_UNIT_1, ADC1_CHANNEL_2}, // 38
        {ADC_UNIT_1, ADC1_CHANNEL_3}, // 39
};

ADConverter::ADConverter(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bits_width_t bit_width)
{
    m_adc_handle = iot_adc_create(unit, channel, atten, bit_width);
}

ADConverter::ADConverter(gpio_num_t io, adc_atten_t atten, adc_bits_width_t bit_width)
{
    if (adc_desc[io].channel == -1 || adc_desc[io].unit == -1) {
        ESP_LOGE(TAG, "This GPIO does not support ADC function");
        m_adc_handle = NULL;
        return;
    }
    m_adc_handle = iot_adc_create((adc_unit_t) adc_desc[io].unit, (adc_channel_t) adc_desc[io].channel, atten, bit_width);
}

int ADConverter::read()
{
    return iot_adc_get_voltage(m_adc_handle);
}

esp_err_t ADConverter::update(adc_atten_t atten, adc_bits_width_t bit_width, int vref_mv, int sample_num)
{
    return iot_adc_update(m_adc_handle, atten, bit_width, vref_mv, sample_num);
}

ADConverter::~ADConverter()
{
    iot_adc_delete(m_adc_handle);
    m_adc_handle = NULL;
}
