// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "button_adc.h"
#include "esp_timer.h"

static const char *TAG = "adc button";

#define ADC_BTN_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#define DEFAULT_VREF    1100
#define NO_OF_SAMPLES   CONFIG_ADC_BUTTON_SAMPLE_TIMES     //Multisampling

#if CONFIG_IDF_TARGET_ESP32
#define ADC_BUTTON_WIDTH       ADC_WIDTH_BIT_12
#elif CONFIG_IDF_TARGET_ESP32S2
#define ADC_BUTTON_WIDTH       ADC_WIDTH_BIT_13
#endif
#define ADC_BUTTON_ATTEN       ADC_ATTEN_DB_11
#define ADC_BUTTON_ADC_UNIT    ADC_UNIT_1
#define ADC_BUTTON_MAX_CHANNEL CONFIG_ADC_BUTTON_MAX_CHANNEL
#define ADC_BUTTON_MAX_BUTTON  CONFIG_ADC_BUTTON_MAX_BUTTON_PER_CHANNEL

typedef struct {
    uint16_t min;
    uint16_t max;
} button_data_t;

typedef struct {
    adc1_channel_t channel;
    uint8_t is_init;
    button_data_t btns[ADC_BUTTON_MAX_BUTTON];  /* all button on the channel */
    uint64_t last_time;  /* the last time of adc sample */
} btn_adc_channel_t;

typedef struct {
    bool is_configured;
    esp_adc_cal_characteristics_t adc_chars;
    btn_adc_channel_t ch[ADC_BUTTON_MAX_CHANNEL];
    uint8_t ch_num;
} adc_button_t;

static adc_button_t g_button = {0};

static int find_unused_channel(void)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.ch[i].is_init) {
            return i;
        }
    }
    return -1;
}

static int find_channel(adc1_channel_t channel)
{
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (channel == g_button.ch[i].channel) {
            return i;
        }
    }
    return -1;
}

esp_err_t button_adc_init(const button_adc_config_t *config)
{
    ADC_BTN_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    ADC_BTN_CHECK(config->adc_channel < ADC1_CHANNEL_MAX, "channel out of range", ESP_ERR_NOT_SUPPORTED);
    ADC_BTN_CHECK(config->button_index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", ESP_ERR_NOT_SUPPORTED);
    ADC_BTN_CHECK(config->max > 0, "key max voltage invalid", ESP_ERR_INVALID_ARG);

    int ch_index = find_channel(config->adc_channel);
    if (ch_index >= 0) { /**< the channel has been initialized */
        ADC_BTN_CHECK(g_button.ch[ch_index].btns[config->button_index].max == 0, "The button_index has been used", ESP_ERR_INVALID_STATE);
    } else { /**< this is a new channel */
        int unused_ch_index = find_unused_channel();
        ADC_BTN_CHECK(unused_ch_index >= 0, "exceed max channel number, can't create a new channel", ESP_ERR_INVALID_STATE);
        ch_index = unused_ch_index;
    }

    /** initialize adc */
    if (0 == g_button.is_configured) {
        //Configure ADC
        adc1_config_width(ADC_BUTTON_WIDTH);
        //Characterize ADC
        esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_BUTTON_ADC_UNIT, ADC_BUTTON_ATTEN, ADC_BUTTON_WIDTH, DEFAULT_VREF, &g_button.adc_chars);
        if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
            ESP_LOGI(TAG, "Characterized using Two Point Value");
        } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
            ESP_LOGI(TAG, "Characterized using eFuse Vref");
        } else {
            ESP_LOGI(TAG, "Characterized using Default Vref");
        }
        g_button.is_configured = 1;
    }

    /** initialize adc channel */
    if (0 == g_button.ch[ch_index].is_init) {
        adc1_config_channel_atten(config->adc_channel, ADC_BUTTON_ATTEN);
        g_button.ch[ch_index].channel = config->adc_channel;
        g_button.ch[ch_index].is_init = 1;
        g_button.ch[ch_index].last_time = 0;
    }
    g_button.ch[ch_index].btns[config->button_index].max = config->max;
    g_button.ch[ch_index].btns[config->button_index].min = config->min;
    g_button.ch_num++;

    return ESP_OK;
}

esp_err_t button_adc_deinit(adc1_channel_t channel, int button_index)
{
    ADC_BTN_CHECK(channel < ADC1_CHANNEL_MAX, "channel out of range", ESP_ERR_INVALID_ARG);
    ADC_BTN_CHECK(button_index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", ESP_ERR_INVALID_ARG);

    int ch_index = find_channel(channel);
    ADC_BTN_CHECK(ch_index >= 0, "can't find the channel", ESP_ERR_INVALID_ARG);

    g_button.ch[ch_index].btns[button_index].max = 0;
    g_button.ch[ch_index].btns[button_index].min = 0;

    /** check button usage on the channel*/
    uint8_t unused_button = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_BUTTON; i++) {
        if (0 == g_button.ch[ch_index].btns[i].max) {
            unused_button++;
        }
    }
    if (unused_button == ADC_BUTTON_MAX_BUTTON && g_button.ch[ch_index].is_init) {  /**< if all button is unused, deinit the channel */
        /* TODO: to deinit the channel  */
        g_button.ch[ch_index].is_init = 0;
        g_button.ch[ch_index].channel = ADC1_CHANNEL_MAX;
        ESP_LOGD(TAG, "all button is unused on channel%d, deinit the channel", g_button.ch[ch_index].channel);
    }

    /** check channel usage on the adc*/
    uint8_t unused_ch = 0;
    for (size_t i = 0; i < ADC_BUTTON_MAX_CHANNEL; i++) {
        if (0 == g_button.ch[i].is_init) {
            unused_ch++;
        }
    }
    if (unused_ch == ADC_BUTTON_MAX_CHANNEL && g_button.is_configured) { /**< if all channel is unused, deinit the adc */
        /* TODO: to deinit the peripheral adc  */
        g_button.is_configured = false;
        memset(&g_button, 0, sizeof(adc_button_t));
        ESP_LOGD(TAG, "all channel is unused, , deinit adc");
    }

    return ESP_OK;
}

static uint32_t get_adc_volatge(adc1_channel_t channel)
{
    uint32_t adc_reading = 0;
    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= NO_OF_SAMPLES;
    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &g_button.adc_chars);
    ESP_LOGV(TAG, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);
    return voltage;
}

uint8_t button_adc_get_key_level(void *button_index)
{
    static uint16_t vol = 0;
    uint32_t ch = ADC_BUTTON_SPLIT_CHANNEL(button_index);
    uint32_t index = ADC_BUTTON_SPLIT_INDEX(button_index);
    ADC_BTN_CHECK(ch < ADC1_CHANNEL_MAX, "channel out of range", 0);
    ADC_BTN_CHECK(index < ADC_BUTTON_MAX_BUTTON, "button_index out of range", 0);
    int ch_index = find_channel(ch);
    ADC_BTN_CHECK(ch_index >= 0, "The button_index is not init", 0);

    /** It starts only when the elapsed time is more than 1ms */
    if ((esp_timer_get_time() - g_button.ch[ch_index].last_time) > 1000) {
        vol = get_adc_volatge(ch);
        g_button.ch[ch_index].last_time = esp_timer_get_time();
    }

    if (vol <= g_button.ch[ch_index].btns[index].max &&
            vol > g_button.ch[ch_index].btns[index].min) {
        return 1;
    }
    return 0;
}
