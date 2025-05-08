/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "adc_battery_estimation.h"
#include "driver/gpio.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-460)
#define TEST_ADC_UNIT (ADC_UNIT_1)
#define TEST_ADC_BITWIDTH (ADC_BITWIDTH_DEFAULT)
#define TEST_ADC_ATTEN (ADC_ATTEN_DB_12)
#define TEST_ADC_CHANNEL (ADC_CHANNEL_1)
#define TEST_CHARGE_GPIO_NUM (GPIO_NUM_0)
#define TEST_RESISTOR_UPPER (460)
#define TEST_RESISTOR_LOWER (460)
#define TEST_ESTIMATION_TIME (100)

static size_t before_free_8bit;
static size_t before_free_32bit;

bool battery_charging_detect(void *user_data)
{
    if (gpio_get_level(TEST_CHARGE_GPIO_NUM) == 0) {
        return true;
    }
    return false;
}

TEST_CASE("adc battery estimation test", "[internal adc]")
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TEST_CHARGE_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    adc_battery_estimation_t config = {
        .internal = {
            .adc_unit = TEST_ADC_UNIT,
            .adc_bitwidth = TEST_ADC_BITWIDTH,
            .adc_atten = TEST_ADC_ATTEN,
        },
        .adc_channel = TEST_ADC_CHANNEL,
        .lower_resistor = TEST_RESISTOR_LOWER,
        .upper_resistor = TEST_RESISTOR_UPPER,
        .charging_detect_cb = battery_charging_detect,
        .charging_detect_user_data = NULL,
    };

    adc_battery_estimation_handle_t adc_battery_estimation_handle = adc_battery_estimation_create(&config);
    TEST_ASSERT(adc_battery_estimation_handle != NULL);

    for (int i = 0; i < TEST_ESTIMATION_TIME; i++) {
        float capacity = 0;
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &capacity);
        printf("Battery capacity: %.1f%%\n", capacity);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    TEST_ESP_OK(adc_battery_estimation_destroy(adc_battery_estimation_handle));
}

TEST_CASE("adc battery estimation test", "[external adc]")
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TEST_CHARGE_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = TEST_ADC_UNIT,
    };
    TEST_ESP_OK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = TEST_ADC_ATTEN,
        .bitwidth = TEST_ADC_BITWIDTH,
    };
    TEST_ESP_OK(adc_oneshot_config_channel(adc_handle, TEST_ADC_CHANNEL, &chan_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = TEST_ADC_UNIT,
        .chan = TEST_ADC_CHANNEL,
        .atten = TEST_ADC_ATTEN,
        .bitwidth = TEST_ADC_BITWIDTH,
    };
    TEST_ESP_OK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = TEST_ADC_UNIT,
        .atten = TEST_ADC_ATTEN,
        .bitwidth = TEST_ADC_BITWIDTH,
    };
    TEST_ESP_OK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));
#endif

    adc_battery_estimation_t config = {
        .external = {
            .adc_handle = adc_handle,
            .adc_cali_handle = adc_cali_handle,
        },
        .adc_channel = TEST_ADC_CHANNEL,
        .lower_resistor = TEST_RESISTOR_LOWER,
        .upper_resistor = TEST_RESISTOR_UPPER,
        .charging_detect_cb = battery_charging_detect,
        .charging_detect_user_data = NULL,
    };

    adc_battery_estimation_handle_t adc_battery_estimation_handle = adc_battery_estimation_create(&config);
    TEST_ASSERT(adc_battery_estimation_handle != NULL);

    for (int i = 0; i < TEST_ESTIMATION_TIME; i++) {
        float capacity = 0;
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &capacity);
        printf("Battery capacity: %.1f%%\n", capacity);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    TEST_ESP_OK(adc_battery_estimation_destroy(adc_battery_estimation_handle));

    TEST_ESP_OK(adc_oneshot_del_unit(adc_handle));
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    TEST_ESP_OK(adc_cali_delete_scheme_curve_fitting(adc_cali_handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    TEST_ESP_OK(adc_cali_delete_scheme_line_fitting(adc_cali_handle));
#endif
}

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
    printf("     _    ____   ____   ____        _     _____     _   _                 _   _             \n");
    printf("    / \\  |  _ \\ / ___| | __ )  __ _| |_  | ____|___| |_(_)_ __ ___   __ _| |_(_) ___  _ __  \n");
    printf("   / _ \\ | | | | |     |  _ \\ / _` | __| |  _| / __| __| | '_ ` _ \\ / _` | __| |/ _ \\| '_ \\ \n");
    printf("  / ___ \\| |_| | |___  | |_) | (_| | |_  | |___\\__ \\ |_| | | | | | | (_| | |_| | (_) | | | |\n");
    printf(" /_/   \\_\\____/ \\____| |____/ \\__,_|\\__| |_____|___/\\__|_|_| |_| |_|\\__,_|\\__|_|\\___/|_| |_|\n");
    unity_run_menu();
}
