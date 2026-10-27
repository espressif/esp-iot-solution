/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "driver/mcpwm_cap.h"
#include "driver/ledc.h"
#include "hal/gpio_ll.h"
#if defined(CONFIG_USE_GPTIMER)
#include "driver/gptimer.h"
#endif
#include "esp_timer.h"

#include "soc/gpio_struct.h"
#include "unity.h"
#include "esp_log.h"
#include "esp_check.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "zero_detection.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-150)

static const char *TAG = "ZERO_CROSS_DETECTION_TEST";

static size_t before_free_8bit;
static size_t before_free_32bit;

// GPIO and timer callbacks cannot use Unity assertions; record failures
// and check them in the test task after unregistering the callbacks.
static volatile uint32_t delay_gap_begin;
static volatile uint32_t delay_gap_end;
static volatile bool s_signal_lost;
static volatile bool s_signal_invalid;
static volatile bool s_freq_out_of_range;
static volatile bool s_latency_exceeded;
static volatile uint32_t s_rising_edges;
static volatile uint32_t s_falling_edges;

// Do not depend on an external GPIO5-to-GPIO2 jumper on the ESP32-C3.
// Use GPIO5 input/output loopback after zero_detect_create().
#ifdef CONFIG_IDF_TARGET_ESP32C3
#define ZERO_TEST_CAPTURE_PIN GPIO_NUM_5
#else
#define ZERO_TEST_CAPTURE_PIN GPIO_NUM_2
#endif

static void zero_test_reset_events(void)
{
    delay_gap_begin = 0;
    delay_gap_end = 0;
    s_signal_lost = false;
    s_signal_invalid = false;
    s_freq_out_of_range = false;
    s_latency_exceeded = false;
    s_rising_edges = 0;
    s_falling_edges = 0;
}

static void zero_test_check_events(bool require_falling_edge)
{
    TEST_ASSERT_GREATER_THAN_UINT32(0, s_rising_edges);
    if (require_falling_edge) {
        TEST_ASSERT_GREATER_THAN_UINT32(0, s_falling_edges);
    }
    TEST_ASSERT_FALSE_MESSAGE(s_signal_lost, "Zero-cross signal was lost");
    TEST_ASSERT_FALSE_MESSAGE(s_signal_invalid, "Zero-cross signal was invalid");
    TEST_ASSERT_FALSE_MESSAGE(s_freq_out_of_range, "Zero-cross frequency out of range");
    TEST_ASSERT_FALSE_MESSAGE(s_latency_exceeded, "Zero-cross latency exceeded 30 us");
}

static IRAM_ATTR void square_event_cb(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data)  //User's callback API
{
    switch (zero_detect_event) {
    case SIGNAL_FREQ_OUT_OF_RANGE:
        s_freq_out_of_range = true;
        break;
    case SIGNAL_LOST:
        s_signal_lost = true;
        break;
    case SIGNAL_INVALID:
        s_signal_invalid = true;
        break;
    case SIGNAL_RISING_EDGE:
        s_rising_edges++;
        delay_gap_end = esp_timer_get_time();
        if ((uint32_t)(delay_gap_end - delay_gap_begin) >= 30) {
            s_latency_exceeded = true;
        }
        break;
    case SIGNAL_FALLING_EDGE:
        s_falling_edges++;
        delay_gap_end = esp_timer_get_time();
        if ((uint32_t)(delay_gap_end - delay_gap_begin) >= 30) {
            s_latency_exceeded = true;
        }
        break;
    default:
        break;
    }
}

static IRAM_ATTR void pulse_event_cb(zero_detect_event_t zero_detect_event, zero_detect_cb_param_t *param, void *usr_data)  //User's callback API
{
    switch (zero_detect_event) {
    case SIGNAL_FREQ_OUT_OF_RANGE:
        s_freq_out_of_range = true;
        break;
    case SIGNAL_LOST:
        s_signal_lost = true;
        break;
    case SIGNAL_INVALID:
        s_signal_invalid = true;
        break;
    case SIGNAL_RISING_EDGE:
        s_rising_edges++;
        delay_gap_end = esp_timer_get_time();
        if ((uint32_t)(delay_gap_end - delay_gap_begin) >= 30) {
            s_latency_exceeded = true;
        }
        break;
    default:
        break;
    }
}

TEST_CASE("custom_zero_cross_detection_test", "[zero_cross_detecion][iot][pulse]")
{
    ESP_LOGI(TAG, "Pulse wave latency test");
    zero_test_reset_events();
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(5);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_ll_set_level(&GPIO, 5, 1);
    gpio_config(&io_conf);

    static zero_detect_handle_t g_zcds = NULL;
    zero_detect_config_t config = ZERO_DETECTION_INIT_CONFIG_DEFAULT(); //Default parameter
    config.capture_pin = ZERO_TEST_CAPTURE_PIN;
    config.freq_range_max_hz = 65;  //Hz
    config.freq_range_min_hz = 45;  //Hz
    config.valid_times = 6;
    config.zero_signal_type = PULSE_WAVE;
#if defined(SOC_MCPWM_SUPPORTED)
    config.zero_driver_type = MCPWM_TYPE;
#else
    config.zero_driver_type = GPIO_TYPE;
#endif

    g_zcds = zero_detect_create(&config);
    TEST_ASSERT_NOT_EQUAL(NULL, g_zcds);
#ifdef CONFIG_IDF_TARGET_ESP32C3
    // Creating a GPIO detector sets the capture pin to input-only; restore
    // output on GPIO5 for self-loopback without changing interrupt routing.
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT_OUTPUT));
#endif

    zero_detect_register_cb(g_zcds, pulse_event_cb, NULL);

    for (int i = 0; i < 100; i++) {
        delay_gap_begin = esp_timer_get_time();
        gpio_ll_set_level(&GPIO, 5, 1);
        vTaskDelay(2 / portTICK_PERIOD_MS);
        gpio_ll_set_level(&GPIO, 5, 0);
        vTaskDelay(8 / portTICK_PERIOD_MS);   //50Hz
        if (i != 0) {
            ESP_LOGI(TAG, "Latency:%ldus", delay_gap_end - delay_gap_begin);
            //zero_show_data(g_zcds);
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, zero_detect_delete(g_zcds));
    zero_test_check_events(false);
}

TEST_CASE("custom_zero_cross_detection_test", "[zero_cross_detecion][iot][square]")
{
    ESP_LOGI(TAG, "Square wave latency test");
    zero_test_reset_events();
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(5);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_ll_set_level(&GPIO, 5, 1);
    gpio_config(&io_conf);

    static zero_detect_handle_t g_zcds = NULL;
    zero_detect_config_t config = ZERO_DETECTION_INIT_CONFIG_DEFAULT(); //Default parameter
    config.capture_pin = ZERO_TEST_CAPTURE_PIN;
    config.freq_range_max_hz = 65;  //Hz
    config.freq_range_min_hz = 45;  //Hz
    config.valid_times = 6;
    config.zero_signal_type = SQUARE_WAVE;
    config.zero_driver_type = GPIO_TYPE;

    g_zcds = zero_detect_create(&config);
    TEST_ASSERT_NOT_EQUAL(NULL, g_zcds);
#ifdef CONFIG_IDF_TARGET_ESP32C3
    // Creating a GPIO detector sets the capture pin to input-only; restore
    // output on GPIO5 for self-loopback without changing interrupt routing.
    TEST_ASSERT_EQUAL(ESP_OK, gpio_set_direction(GPIO_NUM_5, GPIO_MODE_INPUT_OUTPUT));
#endif

    zero_detect_register_cb(g_zcds, square_event_cb, NULL);

    for (int i = 0; i < 100; i++) {
        delay_gap_begin = esp_timer_get_time();
        gpio_ll_set_level(&GPIO, 5, i % 2);
        vTaskDelay(10 / portTICK_PERIOD_MS);   //50Hz
        if (i != 0) {
            ESP_LOGI(TAG, "Latency:%ldus", delay_gap_end - delay_gap_begin);
            //zero_show_data(g_zcds);
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, zero_detect_delete(g_zcds));
    zero_test_check_events(true);
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
    printf("ZERO CROSS DETECTION TEST \n");
    unity_run_menu();
}
