/* Lowpower EVB Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "lowpower_evb_adc.h"

static const char *TAG = "lowpower_evb_adc";

#define DEFAULT_VREF    1100        /* Use adc2_vref_to_gpio() to obtain a better estimate */
#define NO_OF_SAMPLES   64          /* Multisampling */

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_0;     /* GPIO36 */
static const adc_atten_t atten = ADC_ATTEN_DB_6;
static const adc_unit_t unit = ADC_UNIT_1;

static void check_efuse()
{
    /* Check TP is burned into eFuse */
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Two Point: Supported");
    } else {
        ESP_LOGI(TAG, "eFuse Two Point: NOT supported");
    }

    /* Check Vref is burned into eFuse */
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Vref: Supported");
    } else {
        ESP_LOGI(TAG, "eFuse Vref: NOT supported");
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

void adc_init()
{
    /* Check if Two Point or Vref are burned into eFuse */
    check_efuse();

    /* Configure ADC */
    if (unit == ADC_UNIT_1) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten((adc1_channel_t)channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    /* Characterize ADC */
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
}

uint32_t adc_get_supply_voltage()
{
    uint32_t adc_reading = 0;
    /* Multisampling */
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        if (unit == ADC_UNIT_1) {
            adc_reading += adc1_get_raw((adc1_channel_t)channel);
        } else {
            int raw;
            adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
            adc_reading += raw;
        }
    }
    
    adc_reading /= NO_OF_SAMPLES;
    /* Convert adc_reading to voltage in mV */
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    ESP_LOGD(TAG, "Raw: %d\tVoltage: %dmV", adc_reading, voltage);

    return 3 * voltage;
}
