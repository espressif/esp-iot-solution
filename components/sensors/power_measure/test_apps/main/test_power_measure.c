/* SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "power_measure.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "unity.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)
#define BL0937_CF_GPIO   GPIO_NUM_3  // CF  pin
#define BL0937_SEL_GPIO  GPIO_NUM_4  // SEL pin
#define BL0937_CF1_GPIO  GPIO_NUM_7  // CF1 pin

static const char *TAG = "PowerMeasureTest";

static size_t before_free_8bit;
static size_t before_free_32bit;
// Default calibration factors
#define DEFAULT_KI 1.0f
#define DEFAULT_KU 1.0f
#define DEFAULT_KP 1.0f

TEST_CASE("Power Measurement Initialization Test", "[power_measure]")
{
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,
            .sel_gpio = BL0937_SEL_GPIO,
            .cf1_gpio = BL0937_CF1_GPIO,
            .cf_gpio = BL0937_CF_GPIO,
            .sampling_resistor = 0.001f,
            .divider_resistor = 1981.0f,
            .factor = { .ki = DEFAULT_KI, .ku = DEFAULT_KU, .kp = DEFAULT_KP }
        },
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    esp_err_t ret = power_measure_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(ESP_OK, power_measure_deinit());
}

TEST_CASE("Power Measurement Get Voltage Test", "[power_measure]")
{
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,
            .sel_gpio = BL0937_SEL_GPIO,
            .cf1_gpio = BL0937_CF1_GPIO,
            .cf_gpio = BL0937_CF_GPIO,
            .sampling_resistor = 0.001f,
            .divider_resistor = 1981.0f,
            .factor = { .ki = DEFAULT_KI, .ku = DEFAULT_KU, .kp = DEFAULT_KP }
        },
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    esp_err_t ret = power_measure_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    float voltage = 0.0f;
    esp_err_t ret_v = power_measure_get_voltage(&voltage);
    TEST_ASSERT_EQUAL(ESP_OK, ret_v);
    ESP_LOGI(TAG, "Voltage: %.2f V", voltage);
    power_measure_deinit();
}

TEST_CASE("Power Measurement Get Current Test", "[power_measure]")
{
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,
            .sel_gpio = BL0937_SEL_GPIO,
            .cf1_gpio = BL0937_CF1_GPIO,
            .cf_gpio = BL0937_CF_GPIO,
            .sampling_resistor = 0.001f,
            .divider_resistor = 1981.0f,
            .factor = { .ki = DEFAULT_KI, .ku = DEFAULT_KU, .kp = DEFAULT_KP }
        },
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    esp_err_t ret = power_measure_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    float current = 0.0f;
    esp_err_t ret_c = power_measure_get_current(&current);
    TEST_ASSERT_EQUAL(ESP_OK, ret_c);
    ESP_LOGI(TAG, "Current: %.2f A", current);
    power_measure_deinit();
}

TEST_CASE("Power Measurement Get Active Power Test", "[power_measure]")
{
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,
            .sel_gpio = BL0937_SEL_GPIO,
            .cf1_gpio = BL0937_CF1_GPIO,
            .cf_gpio = BL0937_CF_GPIO,
            .sampling_resistor = 0.001f,
            .divider_resistor = 1981.0f,
            .factor = { .ki = DEFAULT_KI, .ku = DEFAULT_KU, .kp = DEFAULT_KP }
        },
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    esp_err_t ret = power_measure_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    float power = 0.0f;
    esp_err_t ret_p = power_measure_get_active_power(&power);
    TEST_ASSERT_EQUAL(ESP_OK, ret_p);
    ESP_LOGI(TAG, "Power: %.2f W", power);
    power_measure_deinit();
}

TEST_CASE("Power Measurement Get Energy Test", "[power_measure]")
{
    power_measure_init_config_t config = {
        .chip_config = {
            .type = CHIP_BL0937,
            .sel_gpio = BL0937_SEL_GPIO,
            .cf1_gpio = BL0937_CF1_GPIO,
            .cf_gpio = BL0937_CF_GPIO,
            .sampling_resistor = 0.001f,
            .divider_resistor = 1981.0f,
            .factor = { .ki = DEFAULT_KI, .ku = DEFAULT_KU, .kp = DEFAULT_KP }
        },
        .overcurrent = 15,
        .overvoltage = 260,
        .undervoltage = 180,
        .enable_energy_detection = true
    };

    esp_err_t ret = power_measure_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    float energy = 0.0f;
    esp_err_t ret_e = power_measure_get_energy(&energy);
    TEST_ASSERT_EQUAL(ESP_OK, ret_e);
    ESP_LOGI(TAG, "Energy: %.2f Kw/h", energy);
    power_measure_deinit();
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
    printf("POWER MEASURE TEST \n");
    unity_run_menu();
}
