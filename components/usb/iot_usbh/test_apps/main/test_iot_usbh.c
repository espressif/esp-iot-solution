/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_log.h"
#include "iot_usbh.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

TEST_CASE("iot_usbh init/deinit", "[iot_usbh]")
{
    for (size_t i = 0; i < 5; i++) {
        usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
        usbh_port_handle_t port_hdl = iot_usbh_port_init(&config);
        TEST_ASSERT_NOT_NULL(port_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
        vTaskDelay(pdMS_TO_TICKS(2000));
        TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_deinit(port_hdl));
    }

    usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
    usbh_port_handle_t port_hdl = iot_usbh_port_init(&config);
    TEST_ASSERT_NOT_NULL(port_hdl);
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_stop(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(1000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_deinit(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    printf("IOT USBH TEST \n");
    unity_run_menu();
}
