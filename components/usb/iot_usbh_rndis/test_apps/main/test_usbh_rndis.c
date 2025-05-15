/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_rndis.h"
#include "iot_eth.h"
#include "esp_log.h"

#define TAG "rndis_test"

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

TEST_CASE("usbh rndis device memory leak", "[iot_usbh_rndis][read-write][auto]")
{
    iot_usbh_rndis_config_t rndis_cfg = {
        .auto_detect = true,
        .auto_detect_timeout = pdMS_TO_TICKS(1000),
    };

    iot_eth_driver_t *rndis_handle = NULL;
    esp_err_t ret = iot_eth_new_usb_rndis(&rndis_cfg, &rndis_handle);
    if (ret != ESP_OK || rndis_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create USB RNDIS driver");
        return;
    }

    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .stack_input = NULL,
        .user_data = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB RNDIS driver");
        return;
    }

    while (1) {
        ret = iot_eth_start(eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start USB RNDIS driver, try again...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        break;
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    iot_eth_stop(eth_handle);
    iot_eth_uninstall(eth_handle);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
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
    printf("IOT USBH RNDIS TEST \n");
    unity_run_menu();
}
