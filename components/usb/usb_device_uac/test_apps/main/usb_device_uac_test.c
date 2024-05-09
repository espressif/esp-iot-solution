/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"
#include "usb_device_uac.h"
#include "esp_log.h"

static const char *TAG = "usb_device_uac_test";

// Some resources are lazy allocated in GPTimer driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (-300)

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    ESP_LOGI(TAG, "uac_device_output_cb");
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len, size_t *bytes_read, void *arg)
{
    ESP_LOGI(TAG, "uac_device_input_cb");
    return ESP_OK;
}

TEST_CASE("usb_device_uac_test", "[usb_device_uac]")
{
    uac_device_config_t config = {
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .cb_ctx = NULL,
    };
    uac_device_init(&config);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    //   _   _  _   ___   _____ ___ ___ _____
    //  | | | |/_\ / __| |_   _| __/ __|_   _|
    //  | |_| / _ \ (__    | | | _|\__ \ | |
    //   \___/_/ \_\___|   |_| |___|___/ |_|
    printf("  _   _  _   ___   _____ ___ ___ _____ \n");
    printf(" | | | |/_\\ / __| |_   _| __/ __|_   _|\n");
    printf(" | |_| / _ \\ (__    | | | _|\\__ \\ | |  \n");
    printf("  \\___/_/ \\_\\___|   |_| |___|___/ |_|  \n");
    unity_run_menu();
}
