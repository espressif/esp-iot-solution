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
#ifndef _IOT_ADC_H_
#define _IOT_ADC_H_
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/adc.h"

#ifdef __cplusplus
extern "C" {
#endif
#define DEFAULT_VREF    1100        /* Use adc2_vref_to_gpio() to obtain a better estimate */
#define NO_OF_SAMPLES   64          /* Multisampling */

typedef void* adc_handle_t;

/**
 * @brief Init ADC functions
 *
 * @param unit ADC unit index
 * @param channel ADC channel to configure
 * @param atten Attenuation level
 * @param bit_width ADC bit width
 *
 * @return A adc_handle_t handle to the created adc object, or NULL in case of error
 */
adc_handle_t iot_adc_create(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bits_width_t bit_width);

/**
 * @brief Delete an ADC object
 * @param adc_handle handle of the ADC object
 * @return
 *     - ESP_OK if success
 *     - ESP_FAIL otherwise
 */
esp_err_t iot_adc_delete(adc_handle_t adc_handle);

/**
 * @brief Get voltage by ADC
 * @param adc_handle Handle of the ADC object
 * @return
 *     - Voltage value (uint: mV)
 *     - -1 if error
 */
int iot_adc_get_voltage(adc_handle_t adc_handle);

/**
 * @brief Update ADC settings
 * @param adc_handle Handle of the ADC object
 * @param atten Attenuation level
 * @param bit_width ADC bit width
 * @param vref_mv Practical reference voltage of 1.1v(unit: mv)
 * @param sample_num Number of sample for each reading.
 * @return
 *     - ESP_OK if success
 *     - ESP_FAIL otherwise
 */
esp_err_t iot_adc_update(adc_handle_t adc_handle, adc_atten_t atten, adc_bits_width_t bit_width, int vref_mv, int sample_num);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

/**
 * class of ADC
 */
class ADConverter
{
private:
    adc_handle_t m_adc_handle;

    /**
     * prevent copy constructing
     */
    ADConverter(const ADConverter&);
    ADConverter& operator = (const ADConverter&);
public:

    /**
     * @brief constructor of ADConverter
     * @param unit ADC unit index
     * @param channel ADC channel to configure
     * @param atten Attenuation level
     * @param bit_width ADC bit width
     */
    ADConverter(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten = ADC_ATTEN_DB_11, adc_bits_width_t bit_width = ADC_WIDTH_BIT_12);

    /**
     * @brief constructor of ADConverter
     * @param io GPIO index of the ADC pad
     * @param atten Attenuation level
     * @param bit_width ADC bit width
     */
    ADConverter(gpio_num_t io, adc_atten_t atten = ADC_ATTEN_DB_11, adc_bits_width_t bit_width = ADC_WIDTH_BIT_12);

    ~ADConverter();

    /**
     * @brief Get voltage by ADC
     * @return
     *     - Voltage value (uint: mV)
     *     - -1 if error
     */
    int read();

    /**
     * @brief Update ADC settings
     * @param atten Attenuation level
     * @param bit_width ADC bit width
     * @param vref_mv Practical reference voltage of 1.1v(unit: mv)
     * @param sample_num Number of sample for each reading.
     * @return
     *     - ESP_OK if success
     *     - ESP_FAIL otherwise
     */
    esp_err_t update(adc_atten_t atten, adc_bits_width_t bit_width = ADC_WIDTH_BIT_12, int vref_mv = DEFAULT_VREF, int sample_num = NO_OF_SAMPLES);
};
#endif

#endif
