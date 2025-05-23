/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "adc_tp_calibration.h"
#include "adc_driver.h"

static const char* TAG = "ADC_TP_CALIBRATION";

static int compare_ints(const void* a, const void* b)
{
    int arg1 = *(const int*)a;
    int arg2 = *(const int*)b;
    return (arg1 > arg2) - (arg1 < arg2);
}

static int calculate_middle_average(int *values, int count)
{
    qsort(values, count, sizeof(int), compare_ints);
    int middle_start = (count - 10) / 2;
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += values[middle_start + i];
    }
    return sum / 10;
}

static esp_err_t measure_adc_voltage(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_bitwidth_t bitwidth, int target_voltage, int *raw_value)
{
    adc_driver_handle_t driver = adc_driver_create(unit, channel, atten, bitwidth);
    if (driver == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC driver");
        return ESP_FAIL;
    }

    int *valid_values = (int*)malloc(CONFIG_ADC_READ_TIMES * sizeof(int));
    if (valid_values == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ADC values");
        adc_driver_delete(&driver);
        return ESP_ERR_NO_MEM;
    }

    int valid_count = 0;
    ESP_LOGI(TAG, "Memory allocated successfully. Please input %dmV to ADC%d channel %d", target_voltage, unit + 1, channel);

    while (valid_count < CONFIG_ADC_READ_TIMES) {
        int raw = 0, cali = 0;
        ESP_ERROR_CHECK(adc_driver_read_calibrated_value(driver, &raw, &cali));
        ESP_LOGI(TAG, "ADC%d channel %d Measuring %dmV - Progress: %d/%d, raw: %d, voltage: %d mV",
                 unit + 1, channel, target_voltage, valid_count + 1, CONFIG_ADC_READ_TIMES, raw, cali);

        if (abs(cali - target_voltage) < CONFIG_ADC_ERROR_TOLERANCE_RANGE) {
            valid_values[valid_count] = raw;
            valid_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    *raw_value = calculate_middle_average(valid_values, valid_count);
    free(valid_values);
    adc_driver_delete(&driver);
    return ESP_OK;
}

void app_main(void)
{
#if CONFIG_IDF_TARGET_ESP32
    int adc1_raw_150mv = 0, adc1_raw_850mv = 0;
    int adc2_raw_150mv = 0, adc2_raw_850mv = 0;
    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12, 150, &adc1_raw_150mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 0dB attenuation 150mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_150mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12, 850, &adc1_raw_850mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 0dB attenuation 850mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_850mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12, 150, &adc2_raw_150mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 0dB attenuation 150mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_150mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12, 850, &adc2_raw_850mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 0dB attenuation 850mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_850mv);

    ESP_LOGI(TAG, "ESP32 ADC TP Calibration Success, adc1_raw_value_150mv_atten0: %d, adc1_raw_value_850mv_atten0: %d", adc1_raw_150mv, adc1_raw_850mv);
    ESP_LOGI(TAG, "ESP32 ADC TP Calibration Success, adc2_raw_value_150mv_atten0: %d, adc2_raw_value_850mv_atten0: %d", adc2_raw_150mv, adc2_raw_850mv);

    adc_tp_cali_config_t adc1_cali_config = {
        .adc_unit = ADC_UNIT_1,
        .adc_raw_value_150mv_atten0 = adc1_raw_150mv,
        .adc_raw_value_850mv_atten0 = adc1_raw_850mv,
    };

    adc_tp_cali_config_t adc2_cali_config = {
        .adc_unit = ADC_UNIT_2,
        .adc_raw_value_150mv_atten0 = adc2_raw_150mv,
        .adc_raw_value_850mv_atten0 = adc2_raw_850mv,
    };

#elif CONFIG_IDF_TARGET_ESP32S2
    int adc1_raw_600mv = 0, adc1_raw_800mv = 0, adc1_raw_1000mv = 0, adc1_raw_2000mv = 0;
    int adc2_raw_600mv = 0, adc2_raw_800mv = 0, adc2_raw_1000mv = 0, adc2_raw_2000mv = 0;

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_13, 600, &adc1_raw_600mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 0dB attenuation 600mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_600mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_2_5, ADC_BITWIDTH_13, 800, &adc1_raw_800mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 2.5dB attenuation 800mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_800mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_6, ADC_BITWIDTH_13, 1000, &adc1_raw_1000mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 6dB attenuation 1000mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_1000mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_12, ADC_BITWIDTH_13, 2000, &adc1_raw_2000mv));
    ESP_LOGI(TAG, "ADC1 channel %d with 12dB attenuation 2000mV average value: %d", CONFIG_ADC1_CHANNEL, adc1_raw_2000mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_13, 600, &adc2_raw_600mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 0dB attenuation 600mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_600mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_2_5, ADC_BITWIDTH_13, 800, &adc2_raw_800mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 2.5dB attenuation 800mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_800mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_6, ADC_BITWIDTH_13, 1000, &adc2_raw_1000mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 6dB attenuation 1000mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_1000mv);

    ESP_ERROR_CHECK(measure_adc_voltage(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_12, ADC_BITWIDTH_13, 2000, &adc2_raw_2000mv));
    ESP_LOGI(TAG, "ADC2 channel %d with 12dB attenuation 2000mV average value: %d", CONFIG_ADC2_CHANNEL, adc2_raw_2000mv);

    adc_tp_cali_config_t adc1_cali_config = {
        .adc_unit = ADC_UNIT_1,
        .adc_raw_value_600mv_atten0 = adc1_raw_600mv,
        .adc_raw_value_800mv_atten2_5 = adc1_raw_800mv,
        .adc_raw_value_1000mv_atten6 = adc1_raw_1000mv,
        .adc_raw_value_2000mv_atten12 = adc1_raw_2000mv,
    };

    adc_tp_cali_config_t adc2_cali_config = {
        .adc_unit = ADC_UNIT_2,
        .adc_raw_value_600mv_atten0 = adc2_raw_600mv,
        .adc_raw_value_800mv_atten2_5 = adc2_raw_800mv,
        .adc_raw_value_1000mv_atten6 = adc2_raw_1000mv,
        .adc_raw_value_2000mv_atten12 = adc2_raw_2000mv,
    };

#endif

    adc_tp_cali_handle_t adc1_tp_cali_handle = adc_tp_cali_create(&adc1_cali_config, ADC_ATTEN_DB_0);
    if (adc1_tp_cali_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC TP Calibration handle");
        return;
    }

    adc_tp_cali_handle_t adc2_tp_cali_handle = adc_tp_cali_create(&adc2_cali_config, ADC_ATTEN_DB_0);
    if (adc2_tp_cali_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC TP Calibration handle");
        return;
    }

#if CONFIG_IDF_TARGET_ESP32
    adc_driver_handle_t adc1_driver = adc_driver_create(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12);
#elif CONFIG_IDF_TARGET_ESP32S2
    adc_driver_handle_t adc1_driver = adc_driver_create(ADC_UNIT_1, CONFIG_ADC1_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_13);
#endif
    if (adc1_driver == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC driver");
        return;
    }

#if CONFIG_IDF_TARGET_ESP32
    adc_driver_handle_t adc2_driver = adc_driver_create(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_12);
#elif CONFIG_IDF_TARGET_ESP32S2
    adc_driver_handle_t adc2_driver = adc_driver_create(ADC_UNIT_2, CONFIG_ADC2_CHANNEL, ADC_ATTEN_DB_0, ADC_BITWIDTH_13);
#endif
    if (adc2_driver == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC driver");
        return;
    }

    while (1) {
        int raw = 0, cali = 0, tp_cali_voltage = 0;
        ESP_ERROR_CHECK(adc_driver_read_calibrated_value(adc1_driver, &raw, &cali));
        ESP_ERROR_CHECK(adc_tp_cali_raw_to_voltage(adc1_tp_cali_handle, raw, &tp_cali_voltage));
        ESP_LOGI(TAG, "ADC1 channel %d with 0dB attenuation raw: %d, voltage: %d mV", CONFIG_ADC1_CHANNEL, raw, tp_cali_voltage);
        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_ERROR_CHECK(adc_driver_read_calibrated_value(adc2_driver, &raw, &cali));
        ESP_ERROR_CHECK(adc_tp_cali_raw_to_voltage(adc2_tp_cali_handle, raw, &tp_cali_voltage));
        ESP_LOGI(TAG, "ADC2 channel %d with 0dB attenuation raw: %d, voltage: %d mV", CONFIG_ADC2_CHANNEL, raw, tp_cali_voltage);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
