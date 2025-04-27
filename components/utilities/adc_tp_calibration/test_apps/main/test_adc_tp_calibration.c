/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_config.h"
#include "adc_tp_calibration.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/adc.h"
#else
#include "esp_adc/adc_oneshot.h"
#endif

#define TEST_MEMORY_LEAK_THRESHOLD (-460)
#define TEST_SAMPLE_COUNT 100
#if CONFIG_IDF_TARGET_ESP32S2
#define TEST_ADC_CHANNEL ADC_CHANNEL_3
#elif CONFIG_IDF_TARGET_ESP32
#define TEST_ADC_CHANNEL ADC_CHANNEL_6
#endif

static size_t before_free_8bit;
static size_t before_free_32bit;

#if CONFIG_IDF_TARGET_ESP32S2
TEST_CASE("test_adc_tp_cali_raw_to_voltage", "[adc_tp_cali][adc1][esp32s2]")
{
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(TEST_ADC_CHANNEL, ADC_ATTEN_DB_0);
#else
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_13,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, TEST_ADC_CHANNEL, &config));
#endif
    adc_tp_cali_config_t tp_cali_config = {
        .adc_unit = ADC_UNIT_1,
        .adc_raw_value_600mv_atten0 = 5895,
        .adc_raw_value_800mv_atten2_5 = 5786,
        .adc_raw_value_1000mv_atten6 = 5820,
        .adc_raw_value_2000mv_atten12 = 6287,
    };

    adc_tp_cali_handle_t adc_tp_cali = adc_tp_cali_create(&tp_cali_config, ADC_ATTEN_DB_0);
    TEST_ASSERT(adc_tp_cali != NULL);

    for (int i = 0; i < TEST_SAMPLE_COUNT; i++) {
        int raw_value, voltage;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        raw_value = adc1_get_raw(TEST_ADC_CHANNEL);
#else
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, TEST_ADC_CHANNEL, &raw_value));
#endif
        ESP_ERROR_CHECK(adc_tp_cali_raw_to_voltage(adc_tp_cali, raw_value, &voltage));
        printf("raw: %d, voltage: %d mv\n", raw_value, voltage);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
#endif
    adc_tp_cali_delete(&adc_tp_cali);
    TEST_ASSERT(adc_tp_cali == NULL);
}
#endif

#if CONFIG_IDF_TARGET_ESP32
TEST_CASE("test_adc_tp_cali_raw_to_voltage", "[adc_tp_cali][adc1][esp32]")
{
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(TEST_ADC_CHANNEL, ADC_ATTEN_DB_12);
#else
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, TEST_ADC_CHANNEL, &config));
#endif
    adc_tp_cali_config_t tp_cali_config = {
        .adc_unit = ADC_UNIT_1,
        .adc_raw_value_150mv_atten0 = 335,
        .adc_raw_value_850mv_atten0 = 3316,
    };

    adc_tp_cali_handle_t adc_tp_cali = adc_tp_cali_create(&tp_cali_config, ADC_ATTEN_DB_12);
    TEST_ASSERT(adc_tp_cali != NULL);

    for (int i = 0; i < TEST_SAMPLE_COUNT; i++) {
        int raw_value, voltage;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        raw_value = adc1_get_raw(TEST_ADC_CHANNEL);
#else
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, TEST_ADC_CHANNEL, &raw_value));
#endif
        ESP_ERROR_CHECK(adc_tp_cali_raw_to_voltage(adc_tp_cali, raw_value, &voltage));
        printf("raw: %d, voltage: %d mv\n", raw_value, voltage);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
#endif
    adc_tp_cali_delete(&adc_tp_cali);
    TEST_ASSERT(adc_tp_cali == NULL);
}
#endif

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("     _    ____   ____   _____ ____     ____    _    _     \n");
    printf("    / \\  |  _ \\ / ___| |_   _|  _ \\   / ___|  / \\  | |    \n");
    printf("   / _ \\ | | | | |       | | | |_) | | |     / _ \\ | |    \n");
    printf("  / ___ \\| |_| | |___    | | |  __/  | |___ / ___ \\| |___ \n");
    printf(" /_/   \\_\\____/ \\____|   |_| |_|      \\____/_/   \\_\\_____|\n");
    printf("                                                           \n");

    unity_run_menu();
}
